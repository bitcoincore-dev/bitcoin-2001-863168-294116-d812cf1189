@0xe234cce74feea00c;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("interfaces::capnp::messages");

using X = import "proxy.capnp";
using Common = import "common.capnp";
using Handler = import "handler.capnp";

interface Wallet $X.proxy("interfaces::Wallet") {
    destroy @0 (context :X.Context) -> ();
    encryptWallet @1 (context :X.Context, walletPassphrase :Data) -> (result :Bool);
    isCrypted @2 (context :X.Context) -> (result :Bool);
    lock @3 (context :X.Context) -> (result :Bool);
    unlock @4 (context :X.Context, walletPassphrase :Data) -> (result :Bool);
    isLocked @5 (context :X.Context) -> (result :Bool);
    changeWalletPassphrase @6 (context :X.Context, oldWalletPassphrase :Data, newWalletPassphrase :Data) -> (result :Bool);
    abortRescan @7 (context :X.Context) -> ();
    backupWallet @8 (context :X.Context, filename :Text) -> (result :Bool);
    getWalletName @9 (context :X.Context) -> (result :Text);
    getKeyFromPool @10 (context :X.Context, internal :Bool) -> (pubKey :Data, result :Bool);
    getPubKey @11 (context :X.Context, address :Data) -> (pubKey :Data, result :Bool);
    getPrivKey @12 (context :X.Context, address :Data) -> (key :Key, result :Bool);
    isSpendable @13 (context :X.Context, dest :TxDestination) -> (result :Bool);
    haveWatchOnly @14 (context :X.Context) -> (result :Bool);
    setAddressBook @15 (context :X.Context, dest :TxDestination, name :Text, purpose :Text) -> (result :Bool);
    delAddressBook @16 (context :X.Context, dest :TxDestination) -> (result :Bool);
    getAddress @17 (context :X.Context, dest :TxDestination, wantName :Bool, wantIsMine :Bool, wantPurpose :Bool) -> (name :Text, isMine :Int32, purpose :Text, result :Bool);
    getAddresses @18 (context :X.Context) -> (result :List(WalletAddress));
    learnRelatedScripts @19 (context :X.Context, pubKey: Data, outputType :Int32) -> ();
    addDestData @20 (context :X.Context, dest :TxDestination, key :Text, value :Text) -> (result :Bool);
    eraseDestData @21 (context :X.Context, dest :TxDestination, key :Text) -> (result :Bool);
    getDestValues @22 (context :X.Context, prefix :Text) -> (result :List(Data));
    lockCoin @23 (context :X.Context, output :Data) -> ();
    unlockCoin @24 (context :X.Context, output :Data) -> ();
    isLockedCoin @25 (context :X.Context, output :Data) -> (result :Bool);
    listLockedCoins @26 (context :X.Context) -> (outputs :List(Data));
    createTransaction @27 (context :X.Context, recipients :List(Recipient), coinControl :CoinControl, sign :Bool, changePos :Int32) -> (changePos :Int32, fee :Int64, failReason :Text, result :PendingWalletTx);
    transactionCanBeAbandoned @28 (context :X.Context, txid :Data) -> (result :Bool);
    abandonTransaction @29 (context :X.Context, txid :Data) -> (result :Bool);
    transactionCanBeBumped @30 (context :X.Context, txid :Data) -> (result :Bool);
    createBumpTransaction @31 (context :X.Context, txid :Data, coinControl :CoinControl, totalFee :Int64) -> (errors :List(Text), oldFee :Int64, newFee :Int64, mtx :Data, result :Bool);
    signBumpTransaction @32 (context :X.Context, mtx :Data) -> (mtx :Data, result :Bool);
    commitBumpTransaction @33 (context :X.Context, txid :Data, mtx :Data) -> (errors :List(Text), bumpedTxid :Data, result :Bool);
    getTx @34 (context :X.Context, txid :Data) -> (result :Data);
    getWalletTx @35 (context :X.Context, txid :Data) -> (result :WalletTx);
    getWalletTxs @36 (context :X.Context) -> (result :List(WalletTx));
    tryGetTxStatus @37 (context :X.Context, txid :Data) -> (txStatus :WalletTxStatus, numBlocks :Int32, blockTime :Int64, result :Bool);
    getWalletTxDetails @38 (context :X.Context, txid :Data) -> (txStatus :WalletTxStatus, orderForm :List(Common.Pair(Text, Text)), inMempool :Bool, numBlocks :Int32, result :WalletTx);
    getBalances @39 (context :X.Context) -> (result :WalletBalances);
    tryGetBalances @40 (context :X.Context) -> (balances :WalletBalances, numBlocks :Int32, result :Bool);
    getBalance @41 (context :X.Context) -> (result :Int64);
    getAvailableBalance @42 (context :X.Context, coinControl :CoinControl) -> (result :Int64);
    txinIsMine @43 (context :X.Context, txin :Data) -> (result :Int32);
    txoutIsMine @44 (context :X.Context, txout :Data) -> (result :Int32);
    getDebit @45 (context :X.Context, txin :Data, filter :Int32) -> (result :Int64);
    getCredit @46 (context :X.Context, txout :Data, filter :Int32) -> (result :Int64);
    listCoins @47 (context :X.Context) -> (result :List(Common.Pair(TxDestination, List(Common.Pair(Data, WalletTxOut)))));
    getCoins @48 (context :X.Context, outputs :List(Data)) -> (result :List(WalletTxOut));
    getRequiredFee @49 (context :X.Context, txBytes :UInt32) -> (result :Int64);
    getMinimumFee @50 (context :X.Context, txBytes :UInt32, coinControl :CoinControl, wantReturnedTarget :Bool, wantReason :Bool) -> (returnedTarget :Int32, reason :Int32, result :Int64);
    getConfirmTarget @51 (context :X.Context) -> (result :UInt32);
    hdEnabled @52 (context :X.Context) -> (result :Bool);
    canGetAddresses @53 (context :X.Context) -> (result :Bool);
    privateKeysDisabled @54 (context :X.Context) -> (result :Bool);
    getDefaultAddressType @55 (context :X.Context) -> (result :Int32);
    getDefaultChangeType @56 (context :X.Context) -> (result :Int32);
    getDefaultMaxTxFee @57 (context :X.Context) -> (result :Int64);
    remove @58 (context :X.Context) -> ();
    handleUnload @59 (context :X.Context, callback :UnloadWalletCallback) -> (result :Handler.Handler);
    handleShowProgress @60 (context :X.Context, callback :ShowWalletProgressCallback) -> (result :Handler.Handler);
    handleStatusChanged @61 (context :X.Context, callback :StatusChangedCallback) -> (result :Handler.Handler);
    handleAddressBookChanged @62 (context :X.Context, callback :AddressBookChangedCallback) -> (result :Handler.Handler);
    handleTransactionChanged @63 (context :X.Context, callback :TransactionChangedCallback) -> (result :Handler.Handler);
    handleWatchOnlyChanged @64 (context :X.Context, callback :WatchOnlyChangedCallback) -> (result :Handler.Handler);
    handleCanGetAddressesChanged @65 (context :X.Context, callback :CanGetAddressesChangedCallback) -> (result :Handler.Handler);
}

interface PendingWalletTx $X.proxy("interfaces::PendingWalletTx") {
    destroy @0 (context :X.Context) -> ();
    customGet @1 (context :X.Context) -> (result :Data) $X.name("get");
    commit @2 (context :X.Context, valueMap :List(Common.Pair(Text, Text)), orderForm :List(Common.Pair(Text, Text))) -> (rejectReason :Text, result :Bool);
}

interface UnloadWalletCallback $X.proxy("ProxyCallback<interfaces::Wallet::UnloadFn>") {
    destroy @0 (context :X.Context) -> ();
    call @1 (context :X.Context) -> ();
}

interface ShowWalletProgressCallback $X.proxy("ProxyCallback<interfaces::Wallet::ShowProgressFn>") {
    destroy @0 (context :X.Context) -> ();
    call @1 (context :X.Context, title :Text, progress :Int32) -> ();
}

interface StatusChangedCallback $X.proxy("ProxyCallback<interfaces::Wallet::StatusChangedFn>") {
    destroy @0 (context :X.Context) -> ();
    call @1 (context :X.Context) -> ();
}

interface AddressBookChangedCallback $X.proxy("ProxyCallback<interfaces::Wallet::AddressBookChangedFn>") {
    destroy @0 (context :X.Context) -> ();
    call @1 (context :X.Context, address :TxDestination, label :Text, isMine :Bool, purpose :Text, status :Int32) -> ();
}

interface TransactionChangedCallback $X.proxy("ProxyCallback<interfaces::Wallet::TransactionChangedFn>") {
    destroy @0 (context :X.Context) -> ();
    call @1 (context :X.Context, txid :Data, status :Int32) -> ();
}

interface WatchOnlyChangedCallback $X.proxy("ProxyCallback<interfaces::Wallet::WatchOnlyChangedFn>") {
    destroy @0 (context :X.Context) -> ();
    call @1 (context :X.Context, haveWatchOnly :Bool) -> ();
}

interface CanGetAddressesChangedCallback $X.proxy("ProxyCallback<interfaces::Wallet::CanGetAddressesChangedFn>") {
    destroy @0 (context :X.Context) -> ();
    call @1 (context :X.Context) -> ();
}

struct Key {
    secret @0 :Data;
    isCompressed @1 :Bool;
}

struct TxDestination {
    pkHash @0 :Data;
    scriptHash @1 :Data;
    witnessV0ScriptHash @2 :Data;
    witnessV0KeyHash @3 :Data;
    witnessUnknown @4 :WitnessUnknown;
}

struct WitnessUnknown $X.proxy("WitnessUnknown")
{
    version @0 :UInt32;
    length @1 :UInt32;
    program @2 :Data;
}

struct WalletAddress $X.proxy("WalletAddress") {
    dest @0 :TxDestination;
    isMine @1 :Int32 $X.name("is_mine");
    name @2 :Text;
    purpose @3 :Text;
}

struct Recipient $X.proxy("CRecipient") {
    scriptPubKey @0 :Data;
    amount @1 :Int64 $X.name("nAmount");
    subtractFeeFromAmount @2 :Bool $X.name("fSubtractFeeFromAmount");
}

struct CoinControl {
    destChange @0 :TxDestination;
    hasChangeType @1 :Bool;
    changeType @2 :Int32;
    allowOtherInputs @3 :Bool;
    allowWatchOnly @4 :Bool;
    overrideFeeRate @5 :Bool;
    hasFeeRate @6 :Bool;
    feeRate @7 :Data;
    hasConfirmTarget @8 :Bool;
    confirmTarget @9 :Int32;
    hasSignalRbf @10 :Bool;
    signalRbf @11 :Bool;
    feeMode @12 :Int32;
    minDepth @13 :Int32;
    setSelected @14 :List(Data);
}

struct WalletTx $X.proxy("WalletTx") {
    tx @0 :Data;
    txinIsMine @1 :List(Int32) $X.name("txin_is_mine");
    txoutIsMine @2 :List(Int32) $X.name("txout_is_mine");
    txoutAddress @3 :List(TxDestination) $X.name("txout_address");
    txoutAddressIsMine @4 :List(Int32) $X.name("txout_address_is_mine");
    credit @5 :Int64;
    debit @6 :Int64;
    change @7 :Int64;
    time @8 :Int64;
    valueMap @9 :List(Common.Pair(Text, Text)) $X.name("value_map");
    isCoinbase @10 :Bool $X.name("is_coinbase");
}

struct WalletTxOut $X.proxy("WalletTxOut") {
    txout @0 :Data;
    time @1 :Int64;
    depthInMainChain @2 :Int32 $X.name("depth_in_main_chain");
    isSpent @3 :Bool $X.name("is_spent");
}

struct WalletTxStatus $X.proxy("WalletTxStatus") {
    blockHeight @0 :Int32 $X.name("block_height");
    blocksToMaturity @1 :Int32 $X.name("blocks_to_maturity");
    depthInMainChain @2 :Int32 $X.name("depth_in_main_chain");
    timeReceived @3 :UInt32 $X.name("time_received");
    lockTime @4 :UInt32 $X.name("lock_time");
    isFinal @5 :Bool $X.name("is_final");
    isTrusted @6 :Bool $X.name("is_trusted");
    isAbandoned @7 :Bool $X.name("is_abandoned");
    isCoinbase @8 :Bool $X.name("is_coinbase");
    isInMainChain @9 :Bool $X.name("is_in_main_chain");
}

struct WalletBalances $X.proxy("WalletBalances") {
    balance @0 :Int64;
    unconfirmedBalance @1 :Int64 $X.name("unconfirmed_balance");
    immatureBalance @2 :Int64 $X.name("immature_balance");
    haveWatchOnly @3 :Bool $X.name("have_watch_only");
    watchOnlyBalance @4 :Int64 $X.name("watch_only_balance");
    unconfirmedWatchOnlyBalance @5 :Int64 $X.name("unconfirmed_watch_only_balance");
    immatureWatchOnlyBalance @6 :Int64 $X.name("immature_watch_only_balance");
}