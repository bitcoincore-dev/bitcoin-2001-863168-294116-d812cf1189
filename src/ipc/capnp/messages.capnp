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
    appInit @10 () -> (result :Bool, error: Text);
    appShutdown @11 () -> ();
    startShutdown @12 () -> ();
    shutdownRequested @13 () -> (result :Bool);
    helpMessage @14 (mode :Int32) -> (result :Text);
    mapPort @15 (useUPnP :Bool) -> ();
    getProxy @16 (net :Int32) -> (proxyInfo :Proxy, result :Bool);
    getNodeCount @17 (flags :Int32) -> (result :UInt64);
    getNodesStats @18 () -> (stats :List(NodeStats));
    getBanned @19 () -> (banMap :BanMap);
    ban @20 (netAddr :Data, reason :Int32, bantimeoffset :Int64) -> (result :Bool);
    unban @21 (ip :Data) -> (result :Bool);
    disconnect @22 (id :Int64) -> (result :Bool);
    getTotalBytesRecv @23 () -> (result :Int64);
    getTotalBytesSent @24 () -> (result :Int64);
    getMempoolSize @25 () -> (result :UInt64);
    getMempoolDynamicUsage @26 () -> (result :UInt64);
    getHeaderTip @27 () -> (height :Int32, blockTime :Int64, result: Bool);
    getNumBlocks @28 () -> (result :Int32);
    getLastBlockTime @29 () -> (result :Int64);
    getVerificationProgress @30 () -> (result :Float64);
    isInitialBlockDownload @31 () -> (result :Bool);
    getReindex @32 () -> (result :Bool);
    getImporting @33 () -> (result :Bool);
    setNetworkActive @34 (active :Bool) -> ();
    getNetworkActive @35 () -> (result :Bool);
    getTxConfirmTarget @36 () -> (result :UInt32);
    getWalletRbf @37 () -> (result :Bool);
    getRequiredFee @38 (txBytes :UInt32) -> (result :Int64);
    getMinimumFee @39 (txBytes :UInt32) -> (result :Int64);
    getMaxTxFee @40 () -> (result :Int64);
    estimateSmartFee @41 (nBlocks :Int32) -> (answerFoundAtBlocks :Int32, result :Data);
    getDustRelayFee @42 () -> (result :Data);
    getFallbackFee @43 () -> (result :Data);
    getPayTxFee @44 () -> (result :Data);
    setPayTxFee @45 (rate :Data) -> ();
    executeRpc @46 (command :Text, params :UniValue) -> (result :UniValue, error :Text, rpcError :UniValue);
    listRpcCommands @47 () -> (result :List(Text));
    rpcSetTimerInterfaceIfUnset @48 () -> ();
    rpcUnsetTimerInterface @49 () -> ();
    getUnspentOutputs @50 (txHash :Data) -> (coins :Data, result :Bool);
    getWallet @51 () -> (result :Wallet);
    handleInitMessage @52 (callback: InitMessageCallback) -> (handler :Handler);
    handleMessageBox @53 (callback: MessageBoxCallback) -> (handler :Handler);
    handleQuestion @54 (callback: QuestionCallback) -> (handler :Handler);
    handleShowProgress @55 (callback: ShowProgressCallback) -> (handler :Handler);
    handleLoadWallet @56 (callback: LoadWalletCallback) -> (handler :Handler);
    handleNotifyNumConnectionsChanged @57 (callback: NotifyNumConnectionsChangedCallback) -> (handler :Handler);
    handleNotifyNetworkActiveChanged @58 (callback: NotifyNetworkActiveChangedCallback) -> (handler :Handler);
    handleNotifyAlertChanged @59 (callback: NotifyAlertChangedCallback) -> (handler :Handler);
    handleBannedListChanged @60 (callback: BannedListChangedCallback) -> (handler :Handler);
    handleNotifyBlockTip @61 (callback: NotifyBlockTipCallback) -> (handler :Handler);
    handleNotifyHeaderTip @62 (callback: NotifyHeaderTipCallback) -> (handler :Handler);
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
    getKey @9 (address :Data) -> (key :Key, result :Bool);
    haveKey @10 (address :Data) -> (result :Bool);
    haveWatchOnly @11 () -> (result :Bool);
    setAddressBook @12 (dest :TxDestination, name :Text, purpose :Text) -> (result :Bool);
    delAddressBook @13 (dest :TxDestination) -> (result :Bool);
    getAddress @14 (dest :TxDestination) -> (name :Text, ismine :Int32, result :Bool);
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
    transactionCanBeAbandoned @25 (txHash :Data) -> (result :Bool);
    abandonTransaction @26 (txHash :Data) -> (result :Bool);
    getTx @27 (txHash :Data) -> (result :Data);
    getWalletTx @28 (txHash :Data) -> (result :WalletTx);
    getWalletTxs @29 () -> (result :List(WalletTx));
    tryGetTxStatus @30 (txHash :Data) -> (txStatus :WalletTxStatus, numBlocks :Int32, adjustedTime :Int64, result :Bool);
    getWalletTxDetails @31 (txHash :Data) -> (txStatus :WalletTxStatus, orderForm :WalletOrderForm, inMempool :Bool, numBlocks :Int32, adjustedTime :Int64, result :WalletTx);
    getBalances @32 () -> (result :WalletBalances);
    tryGetBalances @33 () -> (balances :WalletBalances, numBlocks :Int32, result :Bool);
    getBalance @34 () -> (result :Int64);
    getUnconfirmedBalance @35 () -> (result :Int64);
    getImmatureBalance @36 () -> (result :Int64);
    getWatchOnlyBalance @37 () -> (result :Int64);
    getUnconfirmedWatchOnlyBalance @38 () -> (result :Int64);
    getImmatureWatchOnlyBalance @39 () -> (result :Int64);
    getAvailableBalance @40 (coinControl :CoinControl) -> (result :Int64);
    isMine @41 (txin :Data, txout :Data) -> (result :Int32);
    getDebit @42 (txin :Data, filter :Int32) -> (result :Int64);
    getCredit @43 (txout :Data, filter :Int32) -> (result :Int64);
    listCoins @44 () -> (result :CoinsList);
    getCoins @45 (outputs :List(Data)) -> (result :List(WalletTxOut));
    hdEnabled @46 () -> (result :Bool);
    handleShowProgress @47 (callback: ShowProgressCallback) -> (handler :Handler);
    handleStatusChanged @48 (callback: StatusChangedCallback) -> (handler :Handler);
    handleAddressBookChanged @49 (callback: AddressBookChangedCallback) -> (handler :Handler);
    handleTransactionChanged @50 (callback: TransactionChangedCallback) -> (handler :Handler);
    handleWatchonlyChanged @51 (callback: WatchonlyChangedCallback) -> (handler :Handler);
}

interface Handler {
    disconnect @0 () -> ();
}

interface PendingWalletTx {
    get @0 () -> (result :Data);
    getVirtualSize @1 () -> (result :Int64);
    commit @2 (mapValue :WalletValueMap, orderForm :WalletOrderForm, fromAccount :Text) -> (rejectReason :Text, result :Bool);
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
    call @0 (hashTx :Data, status :Int32, isCoinBase :Bool, isInMainChain :Bool) -> ();
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
    stateStats @23 :NodeStateStats;
}

struct NodeStateStats {
    misbehavior @0 :Int32;
    syncHeight @1 :Int32;
    commonHeight @2 :Int32;
    heightInFlight @3 :List(Int32);
}

struct BanMap {
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
    minimumTotalFee @3 :Int64;
    overrideFeeRate @4 :Bool;
    feeRate @5 :Data;
    confirmTarget @6 :Int32;
    signalRbf @7 :Bool;
    setSelected @8 :List(Data);
}
struct CoinsList {
    entries @0 :List(Entry);
    struct Entry {
        dest @0 :TxDestination;
        coins @1 :List(Coin);
    }
    struct Coin {
        output @0 :Data;
        txOut @1 :WalletTxOut;
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
    txInIsMine @1 :List(Int32);
    txOutIsMine @2 :List(Int32);
    txOutAddress @3 :List(TxDestination);
    txOutAddressIsMine @4 :List(Int32);
    credit @5 :Int64;
    debit @6 :Int64;
    change @7 :Int64;
    txTime @8 :Int64;
    mapValue @9 :WalletValueMap;
    isCoinBase @10 :Bool;
}
struct WalletTxOut {
    txOut @0 :Data;
    txTime @1 :Int64;
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
