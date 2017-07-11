@0xa4478fe5ad6d80f5;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("ipc::capnp::messages");

using X = import "proxy.capnp";
$X.include("ipc/capnp/types.h");

interface Node $X.proxy("ipc::Node") $X.server("NodeServerBase") {
    parseParameters @0 (argv :List(Text) $X.count(2)) -> ();
    softSetArg @1 (arg :Text, value :Text) -> (result :Bool);
    softSetBoolArg @2 (arg :Text, value :Bool) -> (result :Bool);
    readConfigFile @3 (confPath :Text) -> (error :Text);
    selectParams @4 (network :Text) -> (error :Text);
    getNetwork @5 () -> (result :Text);
    initLogging @6 () -> ();
    initParameterInteraction @7 () -> ();
    getWarnings @8 (type :Text) -> (result :Text);
    getLogCategories @9 () -> (result :UInt32);
    baseInitialize @10 () -> (result :Bool, error: Text) $X.async;
    appInitMain @11 () -> (result :Bool, error: Text) $X.async;
    appShutdown @12 () -> ();
    startShutdown @13 () -> ();
    shutdownRequested @14 () -> (result :Bool);
    helpMessage @15 (mode :Int32) -> (result :Text);
    mapPort @16 (useUPnP :Bool) -> ();
    getProxy @17 (net :Int32) -> (proxyInfo :Proxy, result :Bool);
    getNodeCount @18 (flags :Int32) -> (result :UInt64);
    getNodesStats @19 () -> (stats :List(NodeStats), result: Bool);
    getBanned @20 () -> (banmap :Banmap, result: Bool);
    ban @21 (netAddr :Data, reason :Int32, banTimeOffset :Int64) -> (result :Bool);
    unban @22 (ip :Data) -> (result :Bool);
    disconnect @23 (id :Int64) -> (result :Bool);
    getTotalBytesRecv @24 () -> (result :Int64);
    getTotalBytesSent @25 () -> (result :Int64);
    getMempoolSize @26 () -> (result :UInt64);
    getMempoolDynamicUsage @27 () -> (result :UInt64);
    getHeaderTip @28 () -> (height :Int32, blockTime :Int64, result: Bool);
    getNumBlocks @29 () -> (result :Int32);
    getLastBlockTime @30 () -> (result :Int64);
    getVerificationProgress @31 () -> (result :Float64);
    isInitialBlockDownload @32 () -> (result :Bool);
    getReindex @33 () -> (result :Bool);
    getImporting @34 () -> (result :Bool);
    setNetworkActive @35 (active :Bool) -> ();
    getNetworkActive @36 () -> (result :Bool);
    getTxConfirmTarget @37 () -> (result :UInt32);
    getWalletRbf @38 () -> (result :Bool);
    getRequiredFee @39 (txBytes :UInt32) -> (result :Int64);
    getMinimumFee @40 (txBytes :UInt32, coinControl :CoinControl, hasReturnedTarget :Bool, hasReason :Bool) -> (returnedTarget :Int32, reason :Int32, result :Int64);
    getMaxTxFee @41 () -> (result :Int64);
    estimateSmartFee @42 (numBlocks :Int32, conservative :Bool, hasReturnedTarget: Bool) -> (returnedTarget :Int32, result :Data);
    getDustRelayFee @43 () -> (result :Data);
    getFallbackFee @44 () -> (result :Data);
    getPayTxFee @45 () -> (result :Data);
    setPayTxFee @46 (rate :Data) -> ();
    executeRpc @47 (command :Text, params :UniValue, uri :Text) -> (result :UniValue, error :Text, rpcError :UniValue);
    listRpcCommands @48 () -> (result :List(Text));
    rpcSetTimerInterfaceIfUnset @49 (iface: Void) -> () $X.server("localRpcSetTimerInterfaceIfUnset");
    rpcUnsetTimerInterface @50 (iface: Void) -> () $X.server("localRpcUnsetTimerInterface");
    getUnspentOutput @51 (output :Data) -> (coin :Data, result :Bool);
    getWallet @52 (index: UInt64) -> (result :Wallet);
    handleInitMessage @53 (callback: InitMessageCallback) -> (result :Handler);
    handleMessageBox @54 (callback: MessageBoxCallback) -> (result :Handler);
    handleQuestion @55 (callback: QuestionCallback) -> (result :Handler);
    handleShowProgress @56 (callback: ShowNodeProgressCallback) -> (result :Handler);
    handleLoadWallet @57 (callback: LoadWalletCallback) -> (result :Handler);
    handleNotifyNumConnectionsChanged @58 (callback: NotifyNumConnectionsChangedCallback) -> (result :Handler);
    handleNotifyNetworkActiveChanged @59 (callback: NotifyNetworkActiveChangedCallback) -> (result :Handler);
    handleNotifyAlertChanged @60 (callback: NotifyAlertChangedCallback) -> (result :Handler);
    handleBannedListChanged @61 (callback: BannedListChangedCallback) -> (result :Handler);
    handleNotifyBlockTip @62 (callback: NotifyBlockTipCallback) -> (result :Handler);
    handleNotifyHeaderTip @63 (callback: NotifyHeaderTipCallback) -> (result :Handler);
}

interface Wallet $X.proxy("ipc::Wallet") {
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
    isSpendable @10 (dest :TxDestination) -> (result :Bool);
    haveWatchOnly @11 () -> (result :Bool);
    setAddressBook @12 (dest :TxDestination, name :Text, purpose :Text) -> (result :Bool) $X.async;
    delAddressBook @13 (dest :TxDestination) -> (result :Bool);
    getAddress @14 (dest :TxDestination, hasName :Bool, hasIsMine :Bool) -> (name :Text, isMine :Int32, result :Bool);
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
    handleShowProgress @47 (callback: ShowWalletProgressCallback) -> (result :Handler);
    handleStatusChanged @48 (callback: StatusChangedCallback) -> (result :Handler);
    handleAddressBookChanged @49 (callback: AddressBookChangedCallback) -> (result :Handler);
    handleTransactionChanged @50 (callback: TransactionChangedCallback) -> (result :Handler);
    handleWatchOnlyChanged @51 (callback: WatchOnlyChangedCallback) -> (result :Handler);
}

interface Handler $X.proxy("ipc::Handler") {
    disconnect @0 () -> ();
}

interface PendingWalletTx $X.proxy("ipc::PendingWalletTx") $X.client("ipc::RemotePendingWalletTx") {
    get @0 () -> (result :Data) $X.client("getRemote");
    getVirtualSize @1 () -> (result :Int64);
    commit @2 (valueMap :WalletValueMap, orderForm :WalletOrderForm, fromAccount :Text) -> (rejectReason :Text, result :Bool) $X.async;
}

interface InitMessageCallback {
    call @0 (message :Text) -> () $X.proxy("ipc::Node::InitMessageFn");
}

interface MessageBoxCallback {
    call @0 (message :Text, caption :Text, style :UInt32) -> (result :Bool) $X.proxy("ipc::Node::MessageBoxFn") $X.async;
}

interface QuestionCallback {
    call @0 (message :Text, nonInteractiveMessage :Text, caption :Text, style :UInt32) -> (result :Bool) $X.proxy("ipc::Node::QuestionFn");
}

interface ShowNodeProgressCallback {
    call @0 (title :Text, progress :Int32, resumePossible :Bool) -> () $X.proxy("ipc::Node::ShowProgressFn");
}

interface ShowWalletProgressCallback {
    call @0 (title :Text, progress :Int32) -> () $X.proxy("ipc::Wallet::ShowProgressFn");
}

interface LoadWalletCallback {
    call @0 (wallet :Wallet) -> () $X.proxy("ipc::Node::LoadWalletFn") $X.async;
}

interface NotifyNumConnectionsChangedCallback {
    call @0 (newNumConnections :Int32) -> () $X.proxy("ipc::Node::NotifyNumConnectionsChangedFn");
}

interface NotifyNetworkActiveChangedCallback {
    call @0 (networkActive :Bool) -> () $X.proxy("ipc::Node::NotifyNetworkActiveChangedFn");
}

interface NotifyAlertChangedCallback {
    call @0 () -> () $X.proxy("ipc::Node::NotifyAlertChangedFn");
}

interface BannedListChangedCallback {
    call @0 () -> () $X.proxy("ipc::Node::BannedListChangedFn");
}

interface NotifyBlockTipCallback {
    call @0 (initialDownload :Bool, height :Int32, blockTime :Int64, verificationProgress :Float64) -> () $X.proxy("ipc::Node::NotifyBlockTipFn");
}

interface NotifyHeaderTipCallback {
    call @0 (initialDownload :Bool, height :Int32, blockTime :Int64, verificationProgress :Float64) -> () $X.proxy("ipc::Node::NotifyHeaderTipFn");
}

interface StatusChangedCallback {
    call @0 () -> () $X.proxy("ipc::Wallet::StatusChangedFn");
}

interface AddressBookChangedCallback {
    call @0 (address :TxDestination, label :Text, isMine :Bool, purpose :Text, status :Int32) -> () $X.proxy("ipc::Wallet::AddressBookChangedFn");
}

interface TransactionChangedCallback {
    call @0 (txid :Data, status :Int32) -> () $X.proxy("ipc::Wallet::TransactionChangedFn");
}

interface WatchOnlyChangedCallback {
    call @0 (haveWatchOnly :Bool) -> () $X.proxy("ipc::Wallet::WatchOnlyChangedFn");
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
