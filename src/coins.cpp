// Copyright (c) 2012-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "coins.h"

#include "consensus/consensus.h"
#include "memusage.h"
#include "random.h"

#include <assert.h>

bool CCoinsView::GetCoin(const COutPoint &outpoint, Coin &coin) const { return false; }
uint256 CCoinsView::GetBestBlock() const { return uint256(); }
std::vector<uint256> CCoinsView::GetHeadBlocks() const { return std::vector<uint256>(); }
bool CCoinsView::BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock) { return false; }
CCoinsViewCursor *CCoinsView::Cursor() const { return 0; }

bool CCoinsView::HaveCoin(const COutPoint &outpoint) const
{
    Coin coin;
    return GetCoin(outpoint, coin);
}

CCoinsViewBacked::CCoinsViewBacked(CCoinsView *viewIn) : base(viewIn) { }
bool CCoinsViewBacked::GetCoin(const COutPoint &outpoint, Coin &coin) const { return base->GetCoin(outpoint, coin); }
bool CCoinsViewBacked::HaveCoin(const COutPoint &outpoint) const { return base->HaveCoin(outpoint); }
uint256 CCoinsViewBacked::GetBestBlock() const { return base->GetBestBlock(); }
std::vector<uint256> CCoinsViewBacked::GetHeadBlocks() const { return base->GetHeadBlocks(); }
void CCoinsViewBacked::SetBackend(CCoinsView &viewIn) { base = &viewIn; }
bool CCoinsViewBacked::BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock) { return base->BatchWrite(mapCoins, hashBlock); }
CCoinsViewCursor *CCoinsViewBacked::Cursor() const { return base->Cursor(); }
size_t CCoinsViewBacked::EstimateSize() const { return base->EstimateSize(); }

SaltedOutpointHasher::SaltedOutpointHasher() : k0(GetRand(std::numeric_limits<uint64_t>::max())), k1(GetRand(std::numeric_limits<uint64_t>::max())) {}

CCoinsViewCache::CCoinsViewCache(CCoinsView *baseIn) : CCoinsViewBacked(baseIn), cachedCoinsUsage(0) {}

size_t CCoinsViewCache::DynamicMemoryUsage() const {
    return memusage::DynamicUsage(cacheCoins) + cachedCoinsUsage;
}

class CCoinsViewCache::Modifier
{
public:
    Modifier(const CCoinsViewCache& cache, const COutPoint& outpoint, CCoinsCacheEntry* entry = nullptr);
    ~Modifier();
    Coin& Modify();
    CCoinsMap::iterator Flush();

private:
    const CCoinsViewCache& cache;
    CCoinsMap::iterator it;
    CCoinsMap::value_type new_entry;
    bool has_new_entry;
};

CCoinsMap::iterator CCoinsViewCache::FetchCoin(const COutPoint &outpoint) const {
    return Modifier(*this, outpoint).Flush();
}

bool CCoinsViewCache::GetCoin(const COutPoint &outpoint, Coin &coin) const {
    CCoinsMap::const_iterator it = FetchCoin(outpoint);
    if (it != cacheCoins.end()) {
        coin = it->second.coin;
        return !coin.IsSpent();
    }
    return false;
}

void CCoinsViewCache::AddCoin(const COutPoint &outpoint, Coin&& coin, bool possible_overwrite) {
    assert(!coin.IsSpent());
    if (coin.out.scriptPubKey.IsUnspendable()) return;
    CCoinsCacheEntry entry;
    // If we are not possibly overwriting, any coin in the base view below the
    // cache will be spent, so the cache entry can be marked fresh.
    // If we are possibly overwriting, we can't make any assumption about the
    // coin in the the base view below the cache, so the new cache entry which
    // will replace it must be marked dirty.
    entry.flags |= possible_overwrite ? CCoinsCacheEntry::DIRTY : CCoinsCacheEntry::FRESH;
    Modifier(*this, outpoint, &entry).Modify() = std::move(coin);
}

void AddCoins(CCoinsViewCache& cache, const CTransaction &tx, int nHeight, bool check) {
    bool fCoinbase = tx.IsCoinBase();
    const uint256& txid = tx.GetHash();
    for (size_t i = 0; i < tx.vout.size(); ++i) {
        bool overwrite = check ? cache.HaveCoin(COutPoint(txid, i)) : fCoinbase;
        // Always set the possible_overwrite flag to AddCoin for coinbase txn, in order to correctly
        // deal with the pre-BIP30 occurrences of duplicate coinbase transactions.
        cache.AddCoin(COutPoint(txid, i), Coin(tx.vout[i], nHeight, fCoinbase), overwrite);
    }
}

bool CCoinsViewCache::SpendCoin(const COutPoint &outpoint, Coin* moveout) {
    Modifier modifier(*this, outpoint);
    Coin& coin = modifier.Modify();
    bool already_spent = coin.IsSpent();
    if (moveout) {
        *moveout = std::move(coin);
    }
    coin.Clear();
    return !already_spent;
}

static const Coin coinEmpty;

const Coin& CCoinsViewCache::AccessCoin(const COutPoint &outpoint) const {
    CCoinsMap::const_iterator it = FetchCoin(outpoint);
    if (it == cacheCoins.end()) {
        return coinEmpty;
    } else {
        return it->second.coin;
    }
}

bool CCoinsViewCache::HaveCoin(const COutPoint &outpoint) const {
    CCoinsMap::const_iterator it = FetchCoin(outpoint);
    return (it != cacheCoins.end() && !it->second.coin.IsSpent());
}

bool CCoinsViewCache::HaveCoinInCache(const COutPoint &outpoint) const {
    CCoinsMap::const_iterator it = cacheCoins.find(outpoint);
    return (it != cacheCoins.end() && !it->second.coin.IsSpent());
}

uint256 CCoinsViewCache::GetBestBlock() const {
    if (hashBlock.IsNull())
        hashBlock = base->GetBestBlock();
    return hashBlock;
}

void CCoinsViewCache::SetBestBlock(const uint256 &hashBlockIn) {
    hashBlock = hashBlockIn;
}

bool CCoinsViewCache::BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlockIn) {
    for (CCoinsMap::iterator it = mapCoins.begin(); it != mapCoins.end();) {
        if (it->second.flags & CCoinsCacheEntry::DIRTY) { // Ignore non-dirty entries (optimization).
            Modifier(*this, it->first, &it->second);
        }
        CCoinsMap::iterator itOld = it++;
        mapCoins.erase(itOld);
    }
    hashBlock = hashBlockIn;
    return true;
}

bool CCoinsViewCache::Flush() {
    bool fOk = base->BatchWrite(cacheCoins, hashBlock);
    cacheCoins.clear();
    cachedCoinsUsage = 0;
    return fOk;
}

void CCoinsViewCache::Uncache(const COutPoint& hash)
{
    CCoinsMap::iterator it = cacheCoins.find(hash);
    if (it != cacheCoins.end() && it->second.flags == 0) {
        cachedCoinsUsage -= it->second.coin.DynamicMemoryUsage();
        cacheCoins.erase(it);
    }
}

unsigned int CCoinsViewCache::GetCacheSize() const {
    return cacheCoins.size();
}

CAmount CCoinsViewCache::GetValueIn(const CTransaction& tx) const
{
    if (tx.IsCoinBase())
        return 0;

    CAmount nResult = 0;
    for (unsigned int i = 0; i < tx.vin.size(); i++)
        nResult += AccessCoin(tx.vin[i].prevout).out.nValue;

    return nResult;
}

bool CCoinsViewCache::HaveInputs(const CTransaction& tx) const
{
    if (!tx.IsCoinBase()) {
        for (unsigned int i = 0; i < tx.vin.size(); i++) {
            if (!HaveCoin(tx.vin[i].prevout)) {
                return false;
            }
        }
    }
    return true;
}

static const size_t MAX_OUTPUTS_PER_BLOCK = MAX_BLOCK_BASE_SIZE /  ::GetSerializeSize(CTxOut(), SER_NETWORK, PROTOCOL_VERSION); // TODO: merge with similar definition in undo.h.

const Coin& AccessByTxid(const CCoinsViewCache& view, const uint256& txid)
{
    COutPoint iter(txid, 0);
    while (iter.n < MAX_OUTPUTS_PER_BLOCK) {
        const Coin& alternate = view.AccessCoin(iter);
        if (!alternate.IsSpent()) return alternate;
        ++iter.n;
    }
    return coinEmpty;
}

CCoinsViewCache::Modifier::Modifier(const CCoinsViewCache& cache_, const COutPoint& outpoint, CCoinsCacheEntry* entry)
    : cache(cache_), it(cache_.cacheCoins.find(outpoint)), new_entry(outpoint, {}), has_new_entry(false)
{
    if (entry) {
        // If a new entry was provided, merge it into the cache.
        bool existing_entry = it != cache.cacheCoins.end();
        bool existing_spent = existing_entry && it->second.coin.IsSpent();
        bool existing_dirty = existing_entry && it->second.flags & CCoinsCacheEntry::DIRTY;
        bool existing_fresh = existing_entry && it->second.flags & CCoinsCacheEntry::FRESH;
        bool new_spent = entry->coin.IsSpent();
        bool new_dirty = entry->flags & CCoinsCacheEntry::DIRTY;
        bool new_fresh = entry->flags & CCoinsCacheEntry::FRESH;

        // If the new value is marked FRESH, assert any existing cache entry is
        // spent, otherwise it means the FRESH flag was misapplied.
        if (new_fresh && existing_entry && !existing_spent) {
            throw std::logic_error("FRESH flag misapplied to cache of unspent coin");
        }

        // If a cache entry is spent but not dirty, it should be marked fresh.
        if (new_spent && !new_fresh && !new_dirty) {
            throw std::logic_error("Missing FRESH or DIRTY flags for spent cache entry.");
        }

        // If `existing_fresh` is true it means the `this->cache.base` coin is
        // spent, and nothing here changes that, so keep the FRESH flag.
        // If `new_fresh` is true, it means that the `this->cache` coin is spent,
        // which implies that the `this->cache.base` coin is also spent as long as
        // the cache is not dirty, so keep the FRESH flag in this case as well.
        if (existing_fresh || (new_fresh && !existing_dirty)) {
            new_entry.second.flags |= CCoinsCacheEntry::FRESH;
        }

        if (existing_dirty || new_dirty) {
            new_entry.second.flags |= CCoinsCacheEntry::DIRTY;
        }

        new_entry.second.coin = std::move(entry->coin);
        has_new_entry = true;
    } else if (it == cache.cacheCoins.end()) {
        // If no entry in cache, look up in base view.
        has_new_entry = true;
        cache.base->GetCoin(new_entry.first, new_entry.second.coin);
        if (new_entry.second.coin.IsSpent()) {
            new_entry.second.flags |= CCoinsCacheEntry::FRESH;
        }
    }
}

CCoinsViewCache::Modifier::~Modifier()
{
    Flush();
}

// Add DIRTY flag to new_entry. If has_new_entry if false, first populates
// new_entry by copying the existing cache entry if there is one, or falling
// back to a lookup in the base view.
Coin& CCoinsViewCache::Modifier::Modify()
{
    if (!has_new_entry) {
        has_new_entry = true;
        assert(it != cache.cacheCoins.end());
        new_entry.second = it->second;
    }
    new_entry.second.flags |= CCoinsCacheEntry::DIRTY;
    return new_entry.second.coin;
}

// Update cache.cacheCoins with the contents of new_entry, and set
// has_new_entry to false. Does nothing if has_new_entry is false.
CCoinsMap::iterator CCoinsViewCache::Modifier::Flush()
{
    if (has_new_entry) {
        has_new_entry = false;

        bool erase = (new_entry.second.flags & CCoinsCacheEntry::FRESH) && new_entry.second.coin.IsSpent();

        if (it != cache.cacheCoins.end()) {
            cache.cachedCoinsUsage -= it->second.coin.DynamicMemoryUsage();
            if (erase) {
                cache.cacheCoins.erase(it);
                it = cache.cacheCoins.end();
            } else {
                it->second = std::move(new_entry.second);
            }
        } else if (!erase) {
            it = cache.cacheCoins.emplace(std::move(new_entry)).first;
        }

        // If the coin still exists after the modification, add the new usage
        if (it != cache.cacheCoins.end()) {
            cache.cachedCoinsUsage += it->second.coin.DynamicMemoryUsage();
        }
    }
    return it;
}
