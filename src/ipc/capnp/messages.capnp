@0xa4478fe5ad6d80f5;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("ipc::capnp::messages");

interface Node {
    parseParameters @0 (argv :List(Text)) -> ();
    softSetArg @1 (arg :Text, value :Text) -> (result :Bool);
    softSetBoolArg @2 (arg :Text, value :Bool) -> (result :Bool);
    readConfigFile @3 (confPath :Text) -> (error :Text);
    selectParams @4 (network :Text) -> (error :Text);
    getNetwork @5 () -> (result :Text);
    initLogging @6 () -> ();
    initParameterInteraction @7 () -> ();
    getWarnings @8 (type :Text) -> (result :Text);
    getLogCategories @9 () -> (result :UInt32);
    baseInitialize @10 () -> (result :Bool, error: Text);
    appInitMain @11 () -> (result :Bool, error: Text);
    appShutdown @12 () -> ();
    startShutdown @13 () -> ();
    shutdownRequested @14 () -> (result :Bool);
    interruptInit @15 () -> (result :Bool);
    helpMessage @16 (mode :Int32) -> (result :Text);
    mapPort @17 (useUPnP :Bool) -> ();
    getProxy @18 (net :Int32) -> (proxyInfo :Proxy, result :Bool);
    getNodeCount @19 (flags :Int32) -> (result :UInt64);
    getNodesStats @20 () -> (stats :List(NodeStats), result: Bool);
    getBanned @21 () -> (banmap :Banmap, result: Bool);
    ban @22 (netAddr :Data, reason :Int32, banTimeOffset :Int64) -> (result :Bool);
    unban @23 (ip :Data) -> (result :Bool);
    disconnect @24 (id :Int64) -> (result :Bool);
    getTotalBytesRecv @25 () -> (result :Int64);
    getTotalBytesSent @26 () -> (result :Int64);
    getMempoolSize @27 () -> (result :UInt64);
    getMempoolDynamicUsage @28 () -> (result :UInt64);
    getHeaderTip @29 () -> (height :Int32, blockTime :Int64, result: Bool);
    getNumBlocks @30 () -> (result :Int32);
    getLastBlockTime @31 () -> (result :Int64);
    getVerificationProgress @32 () -> (result :Float64);
    isInitialBlockDownload @33 () -> (result :Bool);
    getReindex @34 () -> (result :Bool);
    getImporting @35 () -> (result :Bool);
    setNetworkActive @36 (active :Bool) -> ();
    getNetworkActive @37 () -> (result :Bool);
    getTxConfirmTarget @38 () -> (result :UInt32);
    getWalletRbf @39 () -> (result :Bool);
    getRequiredFee @40 (txBytes :UInt32) -> (result :Int64);
    getMinimumFee @41 (txBytes :UInt32, coinControl :CoinControl) -> (returnedTarget :Int32, reason :Int32, result :Int64);
    getMaxTxFee @42 () -> (result :Int64);
    estimateSmartFee @43 (numBlocks :Int32, conservative :Bool) -> (returnedTarget :Int32, result :Data);
    getDustRelayFee @44 () -> (result :Data);
    getFallbackFee @45 () -> (result :Data);
    getPayTxFee @46 () -> (result :Data);
    setPayTxFee @47 (rate :Data) -> ();
    executeRpc @48 (command :Text, params :UniValue, uri :Text) -> (result :UniValue, error :Text, rpcError :UniValue);
    listRpcCommands @49 () -> (result :List(Text));
    rpcSetTimerInterfaceIfUnset @50 () -> ();
    rpcUnsetTimerInterface @51 () -> ();
    getUnspentOutput @52 (output :Data) -> (coin :Data, result :Bool);
    getWallet @53 (index: Int64) -> (result :Wallet);
    handleInitMessage @54 (callback: InitMessageCallback) -> (handler :Handler);
    handleMessageBox @55 (callback: MessageBoxCallback) -> (handler :Handler);
    handleQuestion @56 (callback: QuestionCallback) -> (handler :Handler);
    handleShowProgress @57 (callback: ShowProgressCallback) -> (handler :Handler);
    handleLoadWallet @58 (callback: LoadWalletCallback) -> (handler :Handler);
    handleNotifyNumConnectionsChanged @59 (callback: NotifyNumConnectionsChangedCallback) -> (handler :Handler);
    handleNotifyNetworkActiveChanged @60 (callback: NotifyNetworkActiveChangedCallback) -> (handler :Handler);
    handleNotifyAlertChanged @61 (callback: NotifyAlertChangedCallback) -> (handler :Handler);
    handleBannedListChanged @62 (callback: BannedListChangedCallback) -> (handler :Handler);
    handleNotifyBlockTip @63 (callback: NotifyBlockTipCallback) -> (handler :Handler);
    handleNotifyHeaderTip @64 (callback: NotifyHeaderTipCallback) -> (handler :Handler);
}

interface Wallet {
    encryptWallet @0 (walletPassphrase :Data) -> (result :Bool);
    isCrypted @1 () -> (result :Bool);
    lock @2 () -> (result :Bool);
    unlock @3 (walletPassphrase :Data) -> (result :Bool);
    isLocked @4 () -> (result :Bool);
    changeWalletPassphrase @5 (oldWalletPassphrase :Data, newWalletPassphrase :Data) -> (result :Bool);
    backupWallet @6 (filename :Text) -> (result :Bool);
    getKeyFromPool @7 (internal :Bool) -> (pubKey :Data, result :Bool);
    getPubKey @8 (address :Data) -> (pubKey :Data, result :Bool);
    getPrivKey @9 (address :Data) -> (key :Key, result :Bool);
    havePrivKey @10 (address :Data) -> (result :Bool);
    haveWatchOnly @11 () -> (result :Bool);
    setAddressBook @12 (dest :TxDestination, name :Text, purpose :Text) -> (result :Bool);
    delAddressBook @13 (dest :TxDestination) -> (result :Bool);
    getAddress @14 (dest :TxDestination) -> (name :Text, isMine :Int32, result :Bool);
    getAddresses @15 () -> (result :List(WalletAddress));
    getAccountAddresses @16 (account :Text) -> (result :List(TxDestination));
    addDestData @17 (dest :TxDestination, key :Text, value :Text) -> (result :Bool);
    eraseDestData @18 (dest :TxDestination, key :Text) -> (result :Bool);
    getDestValues @19 (prefix :Text) -> (result :List(Data));
    lockCoin @20 (output :Data) -> ();
    unlockCoin @21 (output :Data) -> ();
    isLockedCoin @22 (output :Data) -> (result :Bool);
    listLockedCoins @23 () -> (outputs :List(Data));
    createTransaction @24 (recipients :List(Recipient), coinControl :CoinControl, sign :Bool, changePos :Int32) -> (changePos :Int32, fee :Int64, failReason :Text, result :PendingWalletTx);
    transactionCanBeAbandoned @25 (txid :Data) -> (result :Bool);
    abandonTransaction @26 (txid :Data) -> (result :Bool);
    transactionCanBeBumped @27 (txid :Data) -> (result :Bool);
    createBumpTransaction @28 (txid :Data, coinControl :CoinControl, totalFee :Int64) -> (errors :List(Text), oldFee :Int64, newFee :Int64, mtx :Data, result :Bool);
    signBumpTransaction @29 (mtx :Data) -> (mtx :Data, result :Bool);
    commitBumpTransaction @30 (txid :Data, mtx :Data) -> (errors :List(Text), bumpedTxid :Data, result :Bool);
    getTx @31 (txid :Data) -> (result :Data);
    getWalletTx @32 (txid :Data) -> (result :WalletTx);
    getWalletTxs @33 () -> (result :List(WalletTx));
    tryGetTxStatus @34 (txid :Data) -> (txStatus :WalletTxStatus, numBlocks :Int32, adjustedTime :Int64, result :Bool);
    getWalletTxDetails @35 (txid :Data) -> (txStatus :WalletTxStatus, orderForm :WalletOrderForm, inMempool :Bool, numBlocks :Int32, adjustedTime :Int64, result :WalletTx);
    getBalances @36 () -> (result :WalletBalances);
    tryGetBalances @37 () -> (balances :WalletBalances, numBlocks :Int32, result :Bool);
    getBalance @38 () -> (result :Int64);
    getAvailableBalance @39 (coinControl :CoinControl) -> (result :Int64);
    txinIsMine @40 (txin :Data) -> (result :Int32);
    txoutIsMine @41 (txout :Data) -> (result :Int32);
    getDebit @42 (txin :Data, filter :Int32) -> (result :Int64);
    getCredit @43 (txout :Data, filter :Int32) -> (result :Int64);
    listCoins @44 () -> (result :CoinsList);
    getCoins @45 (outputs :List(Data)) -> (result :List(WalletTxOut));
    hdEnabled @46 () -> (result :Bool);
    handleShowProgress @47 (callback: ShowProgressCallback) -> (handler :Handler);
    handleStatusChanged @48 (callback: StatusChangedCallback) -> (handler :Handler);
    handleAddressBookChanged @49 (callback: AddressBookChangedCallback) -> (handler :Handler);
    handleTransactionChanged @50 (callback: TransactionChangedCallback) -> (handler :Handler);
    handleWatchOnlyChanged @51 (callback: WatchonlyChangedCallback) -> (handler :Handler);
}

interface Handler {
    disconnect @0 () -> ();
}

interface PendingWalletTx {
    get @0 () -> (result :Data);
    getVirtualSize @1 () -> (result :Int64);
    commit @2 (valueMap :WalletValueMap, orderForm :WalletOrderForm, fromAccount :Text) -> (rejectReason :Text, result :Bool);
}

interface InitMessageCallback {
    call @0 (message :Text) -> ();
}

interface MessageBoxCallback {
    call @0 (message :Text, caption :Text, style :UInt32) -> (result :Bool);
}

interface QuestionCallback {
    call @0 (message :Text, nonInteractiveMessage :Text, caption :Text, style :UInt32) -> (result :Bool);
}

interface ShowProgressCallback {
    call @0 (title :Text, progress :Int32) -> ();
}

interface LoadWalletCallback {
    call @0 (wallet :Wallet) -> ();
}

interface NotifyNumConnectionsChangedCallback {
    call @0 (newNumConnections :Int32) -> ();
}

interface NotifyNetworkActiveChangedCallback {
    call @0 (networkActive :Bool) -> ();
}

interface NotifyAlertChangedCallback {
    call @0 () -> ();
}

interface BannedListChangedCallback {
    call @0 () -> ();
}

interface NotifyBlockTipCallback {
    call @0 (initialDownload :Bool, height :Int32, blockTime :Int64, verificationProgress :Float64) -> ();
}

interface NotifyHeaderTipCallback {
    call @0 (initialDownload :Bool, height :Int32, blockTime :Int64, verificationProgress :Float64) -> ();
}

interface StatusChangedCallback {
    call @0 () -> ();
}

interface AddressBookChangedCallback {
    call @0 (address :TxDestination, label :Text, isMine :Bool, purpose :Text, status :Int32) -> ();
}

interface TransactionChangedCallback {
    call @0 (txid :Data, status :Int32, isCoinBase :Bool, isInMainChain :Bool) -> ();
}

interface WatchonlyChangedCallback {
    call @0 (haveWatchOnly :Bool) -> ();
}

struct MapMsgCmdSize {
    entries @0 :List(Entry);
    struct Entry {
        key @0 :Text;
        value @1 :UInt64;
    }
}

struct Proxy {
    proxy @0 :Data;
    randomizeCredentials @1 :Bool;
}

struct NodeStats {
    nodeid @0 :Int32;
    services @1 :Int64;
    relayTxes @2 :Bool;
    lastSend @3 :Int64;
    lastRecv @4 :Int64;
    timeConnected @5 :Int64;
    timeOffset @6 :Int64;
    addrName @7 :Text;
    version @8 :Int32;
    cleanSubVer @9 :Text;
    inbound @10 :Bool;
    addnode @11 :Bool;
    startingHeight @12 :Int32;
    sendBytes @13 :UInt64;
    sendBytesPerMsgCmd @14 :MapMsgCmdSize;
    recvBytes @15 :UInt64;
    recvBytesPerMsgCmd @16 :MapMsgCmdSize;
    whitelisted @17 :Bool;
    pingTime @18 :Float64;
    pingWait @19 :Float64;
    minPing @20 :Float64;
    addrLocal @21 :Text;
    addr @22 :Data;
    addrBind @23 :Data;
    stateStats @24 :NodeStateStats;
}

struct NodeStateStats {
    misbehavior @0 :Int32;
    syncHeight @1 :Int32;
    commonHeight @2 :Int32;
    heightInFlight @3 :List(Int32);
}

struct Banmap {
    entries @0 :List(Entry);
    struct Entry {
        subnet @0 :Data;
        banEntry @1 :Data;
    }
}

# The current version of UniValue included in bitcoin doesn't support
# round-trip serialization of raw values. After it gets updated, and
# https://github.com/jgarzik/univalue/pull/31 is merged, this struct
# can go away and UniValues can just be serialized as text using
# UniValue::read() and UniValue::write() methods.
struct UniValue {
    type @0 :Int32;
    value @1 :Text;
}

struct CoinControl {
    destChange @0 :TxDestination;
    allowOtherInputs @1 :Bool;
    allowWatchOnly @2 :Bool;
    overrideFeeRate @3 :Bool;
    feeRate @4 :Data;
    hasConfirmTarget @5 :Bool;
    confirmTarget @6 :Int32;
    signalRbf @7 :Bool;
    feeMode @8 :Int32;
    setSelected @9 :List(Data);
}
struct CoinsList {
    entries @0 :List(Entry);
    struct Entry {
        dest @0 :TxDestination;
        coins @1 :List(Coin);
    }
    struct Coin {
        output @0 :Data;
        txout @1 :WalletTxOut;
    }
}
struct Recipient {
    scriptPubKey @0 :Data;
    amount @1 :Int64;
    subtractFeeFromAmount @2 :Bool;
}
struct Key {
  secret @0 :Data;
  isCompressed @1 :Bool;
}
struct TxDestination {
  keyId @0 :Data;
  scriptId @1 :Data;
}
struct WalletAddress {
    dest @0 :TxDestination;
    isMine @1 :Int32;
    name @2 :Text;
    purpose @3 :Text;
}
struct WalletBalances {
    balance @0 :Int64;
    unconfirmedBalance @1 :Int64;
    immatureBalance @2 :Int64;
    haveWatchOnly @3 :Bool;
    watchOnlyBalance @4 :Int64;
    unconfirmedWatchOnlyBalance @5 :Int64;
    immatureWatchOnlyBalance @6 :Int64;
}
struct WalletOrderForm {
    entries @0 :List(Entry);
    struct Entry {
        key @0 :Text;
        value @1 :Text;
    }
}
struct WalletTx {
    tx @0 :Data;
    txinIsMine @1 :List(Int32);
    txoutIsMine @2 :List(Int32);
    txoutAddress @3 :List(TxDestination);
    txoutAddressIsMine @4 :List(Int32);
    credit @5 :Int64;
    debit @6 :Int64;
    change @7 :Int64;
    time @8 :Int64;
    valueMap @9 :WalletValueMap;
    isCoinBase @10 :Bool;
}
struct WalletTxOut {
    txout @0 :Data;
    time @1 :Int64;
    depthInMainChain @2 :Int32;
    isSpent @3 :Bool;
}
struct WalletTxStatus {
    blockHeight @0 :Int32;
    blocksToMaturity @1 :Int32;
    depthInMainChain @2 :Int32;
    requestCount @3 :Int32;
    timeReceived @4 :UInt32;
    lockTime @5 :UInt32;
    isFinal @6 :Bool;
    isTrusted @7 :Bool;
    isAbandoned @8 :Bool;
    isCoinBase @9 :Bool;
    isInMainChain @10 :Bool;
}
struct WalletValueMap {
    entries @0 :List(Entry);
    struct Entry {
        key @0 :Text;
        value @1 :Text;
    }
}
