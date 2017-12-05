@0xa4478fe5ad6d80f5;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("interface::capnp::messages");

using X = import "proxy.capnp";

interface Init $X.proxy("interface::Init") {
    makeNode @0 () -> (result :Node);
    makeWalletClient @1 (chain :Chain, walletFilenames :List(Text), globalArgs :GlobalArgs $X.count(0)) -> (result :ChainClient);
}

interface Chain $X.proxy("interface::Chain") {
    lock @0 (tryLock :Bool) -> (result :ChainLock);
    assumeLocked @1 () -> (result :ChainLock);
    findBlock @2 (hash :Data, wantBlock :Bool, wantTime :Bool, wantMaxTime :Bool) -> (block :Data, time :Int64, maxTime :Int64, result :Bool);
    guessVerificationProgress @3 (blockHash :Data) -> (result :Float64);
    getVirtualTransactionSize @4 (tx :Data) -> (result :Int64);
    isRBFOptIn @5 (tx :Data) -> (result :Int32);
    hasDescendantsInMempool @6 (txid :Data) -> (result :Bool);
    relayTransaction @7 (txid :Data) -> (result :Bool);
    transactionWithinChainLimit @8 (txid :Data, chainLimit :UInt64) -> (result :Bool);
    checkChainLimits @9 (tx :Data) -> (result :Bool);
    getMinPoolFeeRate @10 () -> (result :Data);
    getMinRelayFeeRate @11 () -> (result :Data);
    getIncrementalRelayFeeRate @12 () -> (result :Data);
    getDustRelayFeeRate @13 () -> (result :Data);
    getMaxDiscardFeeRate @14 () -> (result :Data);
    getMaxTxFee @15 () -> (result :Int64);
    getMinTxFee @16 (txBytes :UInt32, coinControl :CoinControl, wantCalc :Bool) -> (calc :FeeCalculation, result :Int64);
    getRequiredTxFee @17 (txBytes :UInt32) -> (result :Int64);
    getPruneMode @18 () -> (result :Bool);
    p2pEnabled @19 () -> (result :Bool);
    getAdjustedTime @20 () -> (result :Int64);
    initMessage @21 (message :Text) -> ();
    initWarning @22 (message :Text) -> ();
    initError @23 (message :Text) -> (result :Bool);
    loadWallet @24 (wallet :Wallet) -> ();
    generateBlocks @25 (coinbaseScript :Data, numBlocks :Int32, maxTries:UInt64, keepScript:Bool) -> (result :UniValue);
    parseConfirmTarget @26 (value :UniValue) -> (result :UInt32);
    handleNotifications @27 (notifications :ChainNotifications) -> (result :Handler);
    waitForNotifications @28 () -> ();
    handleRpc @29 (command :RPCCommand) -> (result :Handler);
}

interface ChainLock $X.proxy("interface::Chain::Lock") {
    getHeight @0 () -> (result :Int32, hasResult :Bool);
    getBlockHeight @1 (hash :Data) -> (result :Int32, hasResult :Bool);
    getBlockDepth @2 (hash :Data) -> (result :Int32);
    getBlockHash @3 (height :Int32) -> (result :Data);
    getBlockTime @4 (height :Int32) -> (result :Int64);
    getBlockMedianTimePast @5 (height :Int32) -> (result :Int64);
    blockHasTransactions @6 (height :Int32) -> (result :Bool);
    findEarliestAtLeast @7 (time :Int64) -> (result :Int32, hasResult :Bool);
    findLastBefore @8 (time :Int64, startHeight :Int32) -> (result :Int32, hasResult :Bool);
    findPruned @9 (startHeight: Int32, stopHeight :Int32, hasStopHeight :Bool) -> (result :Int32, hasResult :Bool);
    findFork @10 (hash :Data, wantHeight :Bool) -> (height :Int32, hasHeight :Int32, result :Int32, hasResult :Bool);
    isPotentialTip @11 (hash :Data) -> (result :Bool);
    getLocator @12 () -> (result :Data);
    findLocatorFork @13 (locator :Data) -> (result :Int32, hasResult :Bool);
    checkFinalTx @14 (tx :Data) -> (result :Bool);
    isWitnessEnabled @15 () -> (result :Bool);
    acceptToMemoryPool @16 (tx :Data, state :ValidationState) -> (state :ValidationState, result :Bool);
}

interface ChainNotifications $X.proxy("interface::Chain::Notifications") {
    transactionAddedToMempool @0 (tx :Data) -> () $X.name("TransactionAddedToMempool");
    transactionRemovedFromMempool @1 (tx :Data) -> () $X.name("TransactionRemovedFromMempool");
    blockConnected @2 (block :Data, blockHash :Data, txConflicted :List(Data)) -> () $X.name("BlockConnected");
    blockDisconnected @3 (block :Data) -> () $X.name("BlockDisconnected");
    setBestChain @4 (locator :Data) -> () $X.name("SetBestChain");
    inventory @5 (hash :Data) -> () $X.name("Inventory");
    resendWalletTransactions @6 (bestBlockTime :Int64) -> () $X.name("ResendWalletTransactions");
}

interface ChainClient $X.proxy("interface::Chain::Client") {
    registerRpcs @0 () -> () $X.async;
    prepare @1 () -> (result :Bool) $X.async;
    start @2 (scheduler :Void) -> () $X.async;
    stop @3 () -> ();
    shutdown @4 () -> ();
    getWallets @5 () -> (result :List(Wallet));
}

interface Node $X.proxy("interface::Node") {
    customParseParameters @0 (argv :List(Text) $X.count(2)) -> () $X.name("parseParameters");
    customSoftSetArg @1 (arg :Text, value :Text) -> (result :Bool) $X.name("softSetArg");
    customSoftSetBoolArg @2 (arg :Text, value :Bool) -> (result :Bool) $X.name("softSetBoolArg");
    customReadConfigFile @3 (confPath :Text) -> (error :Text) $X.name("readConfigFile");
    customSelectParams @4 (network :Text) -> (error :Text) $X.name("selectParams");
    getNetwork @5 () -> (result :Text);
    initLogging @6 () -> ();
    initParameterInteraction @7 () -> ();
    getWarnings @8 (type :Text) -> (result :Text);
    getLogCategories @9 () -> (result :UInt32);
    baseInitialize @10 () -> (result :Bool, error :Text) $X.async;
    appInitMain @11 () -> (result :Bool, error :Text) $X.async;
    appShutdown @12 () -> ();
    startShutdown @13 () -> ();
    shutdownRequested @14 () -> (result :Bool);
    helpMessage @15 (mode :Int32) -> (result :Text);
    mapPort @16 (useUPnP :Bool) -> ();
    getProxy @17 (net :Int32) -> (proxyInfo :Proxy, result :Bool);
    getNodeCount @18 (flags :Int32) -> (result :UInt64);
    getNodesStats @19 () -> (stats :List(NodeStats), result :Bool);
    getBanned @20 () -> (banmap :List(X.Pair(Data, Data)), result :Bool);
    ban @21 (netAddr :Data, reason :Int32, banTimeOffset :Int64) -> (result :Bool);
    unban @22 (ip :Data) -> (result :Bool);
    disconnect @23 (id :Int64) -> (result :Bool);
    getTotalBytesRecv @24 () -> (result :Int64);
    getTotalBytesSent @25 () -> (result :Int64);
    getMempoolSize @26 () -> (result :UInt64);
    getMempoolDynamicUsage @27 () -> (result :UInt64);
    getHeaderTip @28 () -> (height :Int32, blockTime :Int64, result :Bool);
    getNumBlocks @29 () -> (result :Int32);
    getLastBlockTime @30 () -> (result :Int64);
    getVerificationProgress @31 () -> (result :Float64);
    isInitialBlockDownload @32 () -> (result :Bool);
    getReindex @33 () -> (result :Bool);
    getImporting @34 () -> (result :Bool);
    setNetworkActive @35 (active :Bool) -> ();
    getNetworkActive @36 () -> (result :Bool);
    getTxConfirmTarget @37 () -> (result :UInt32);
    getRequiredFee @38 (txBytes :UInt32) -> (result :Int64);
    getMinimumFee @39 (txBytes :UInt32, coinControl :CoinControl, wantReturnedTarget :Bool, wantReason :Bool) -> (returnedTarget :Int32, reason :Int32, result :Int64);
    getMaxTxFee @40 () -> (result :Int64);
    estimateSmartFee @41 (numBlocks :Int32, conservative :Bool, wantReturnedTarget :Bool) -> (returnedTarget :Int32, result :Data);
    getDustRelayFee @42 () -> (result :Data);
    getFallbackFee @43 () -> (result :Data);
    getPayTxFee @44 () -> (result :Data);
    setPayTxFee @45 (rate :Data) -> ();
    executeRpc @46 (command :Text, params :UniValue, uri :Text) -> (result :UniValue, error :Text, rpcError :UniValue);
    listRpcCommands @47 () -> (result :List(Text));
    rpcSetTimerInterfaceIfUnset @48 (iface :Void) -> ();
    rpcUnsetTimerInterface @49 (iface :Void) -> ();
    getUnspentOutput @50 (output :Data) -> (coin :Data, result :Bool);
    getWallets @51 () -> (result :List(Wallet));
    handleInitMessage @52 (callback :InitMessageCallback) -> (result :Handler);
    handleMessageBox @53 (callback :MessageBoxCallback) -> (result :Handler);
    handleQuestion @54 (callback :QuestionCallback) -> (result :Handler);
    handleShowProgress @55 (callback :ShowNodeProgressCallback) -> (result :Handler);
    handleLoadWallet @56 (callback :LoadWalletCallback) -> (result :Handler);
    handleNotifyNumConnectionsChanged @57 (callback :NotifyNumConnectionsChangedCallback) -> (result :Handler);
    handleNotifyNetworkActiveChanged @58 (callback :NotifyNetworkActiveChangedCallback) -> (result :Handler);
    handleNotifyAlertChanged @59 (callback :NotifyAlertChangedCallback) -> (result :Handler);
    handleBannedListChanged @60 (callback :BannedListChangedCallback) -> (result :Handler);
    handleNotifyBlockTip @61 (callback :NotifyBlockTipCallback) -> (result :Handler);
    handleNotifyHeaderTip @62 (callback :NotifyHeaderTipCallback) -> (result :Handler);
}

interface Wallet $X.proxy("interface::Wallet") {
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
    getAddress @14 (dest :TxDestination, wantName :Bool, wantIsMine :Bool) -> (name :Text, isMine :Int32, result :Bool);
    getAddresses @15 () -> (result :List(WalletAddress));
    learnRelatedScripts @16 (pubKey: Data, outputType :Int32) -> ();
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
    getWalletTxDetails @35 (txid :Data) -> (txStatus :WalletTxStatus, orderForm :List(X.Pair(Text, Text)), inMempool :Bool, numBlocks :Int32, adjustedTime :Int64, result :WalletTx);
    getBalances @36 () -> (result :WalletBalances);
    tryGetBalances @37 () -> (balances :WalletBalances, numBlocks :Int32, result :Bool);
    getBalance @38 () -> (result :Int64);
    getAvailableBalance @39 (coinControl :CoinControl) -> (result :Int64);
    txinIsMine @40 (txin :Data) -> (result :Int32);
    txoutIsMine @41 (txout :Data) -> (result :Int32);
    getDebit @42 (txin :Data, filter :Int32) -> (result :Int64);
    getCredit @43 (txout :Data, filter :Int32) -> (result :Int64);
    listCoins @44 () -> (result :List(X.Pair(TxDestination, List(X.Pair(Data, WalletTxOut)))));
    getCoins @45 (outputs :List(Data)) -> (result :List(WalletTxOut));
    hdEnabled @46 () -> (result :Bool);
    getDefaultAddressType @47 () -> (result :Int32);
    handleShowProgress @48 (callback :ShowWalletProgressCallback) -> (result :Handler);
    handleStatusChanged @49 (callback :StatusChangedCallback) -> (result :Handler);
    handleAddressBookChanged @50 (callback :AddressBookChangedCallback) -> (result :Handler);
    handleTransactionChanged @51 (callback :TransactionChangedCallback) -> (result :Handler);
    handleWatchOnlyChanged @52 (callback :WatchOnlyChangedCallback) -> (result :Handler);
}

interface Handler $X.proxy("interface::Handler") {
    disconnect @0 () -> ();
}

interface PendingWalletTx $X.proxy("interface::PendingWalletTx") {
    customGet @0 () -> (result :Data) $X.name("get");
    getVirtualSize @1 () -> (result :Int64);
    commit @2 (valueMap :List(X.Pair(Text, Text)), orderForm :List(X.Pair(Text, Text)), fromAccount :Text) -> (rejectReason :Text, result :Bool) $X.async;
}

interface InitMessageCallback $X.proxy("ProxyCallback<interface::Node::InitMessageFn>") {
    call @0 (message :Text) -> ();
}

interface MessageBoxCallback $X.proxy("ProxyCallback<interface::Node::MessageBoxFn>") {
    call @0 (message :Text, caption :Text, style :UInt32) -> (result :Bool) $X.async;
}

interface QuestionCallback $X.proxy("ProxyCallback<interface::Node::QuestionFn>") {
    call @0 (message :Text, nonInteractiveMessage :Text, caption :Text, style :UInt32) -> (result :Bool);
}

interface ShowNodeProgressCallback $X.proxy("ProxyCallback<interface::Node::ShowProgressFn>") {
    call @0 (title :Text, progress :Int32, resumePossible :Bool) -> ();
}

interface ShowWalletProgressCallback $X.proxy("ProxyCallback<interface::Wallet::ShowProgressFn>") {
    call @0 (title :Text, progress :Int32) -> ();
}

interface LoadWalletCallback $X.proxy("ProxyCallback<interface::Node::LoadWalletFn>") {
    call @0 (wallet :Wallet) -> () $X.async;
}

interface NotifyNumConnectionsChangedCallback $X.proxy("ProxyCallback<interface::Node::NotifyNumConnectionsChangedFn>") {
    call @0 (newNumConnections :Int32) -> ();
}

interface NotifyNetworkActiveChangedCallback $X.proxy("ProxyCallback<interface::Node::NotifyNetworkActiveChangedFn>") {
    call @0 (networkActive :Bool) -> ();
}

interface NotifyAlertChangedCallback $X.proxy("ProxyCallback<interface::Node::NotifyAlertChangedFn>") {
    call @0 () -> ();
}

interface BannedListChangedCallback $X.proxy("ProxyCallback<interface::Node::BannedListChangedFn>") {
    call @0 () -> ();
}

interface NotifyBlockTipCallback $X.proxy("ProxyCallback<interface::Node::NotifyBlockTipFn>") {
    call @0 (initialDownload :Bool, height :Int32, blockTime :Int64, verificationProgress :Float64) -> ();
}

interface NotifyHeaderTipCallback $X.proxy("ProxyCallback<interface::Node::NotifyHeaderTipFn>") {
    call @0 (initialDownload :Bool, height :Int32, blockTime :Int64, verificationProgress :Float64) -> ();
}

interface StatusChangedCallback $X.proxy("ProxyCallback<interface::Wallet::StatusChangedFn>") {
    call @0 () -> ();
}

interface AddressBookChangedCallback $X.proxy("ProxyCallback<interface::Wallet::AddressBookChangedFn>") {
    call @0 (address :TxDestination, label :Text, isMine :Bool, purpose :Text, status :Int32) -> ();
}

interface TransactionChangedCallback $X.proxy("ProxyCallback<interface::Wallet::TransactionChangedFn>") {
    call @0 (txid :Data, status :Int32) -> ();
}

interface WatchOnlyChangedCallback $X.proxy("ProxyCallback<interface::Wallet::WatchOnlyChangedFn>") {
    call @0 (haveWatchOnly :Bool) -> ();
}

struct PairStr64 {
    key @0 :Text;
    value @1 :UInt64;
}

struct Proxy $X.proxy("proxyType") {
    proxy @0 :Data;
    randomizeCredentials @1 :Bool $X.name("randomize_credentials");
}

struct NodeStats $X.proxy("CNodeStats") {
    nodeid @0 :Int64;
    services @1 :Int64 $X.name("nServices");
    relayTxes @2 :Bool $X.name("fRelayTxes");
    lastSend @3 :Int64 $X.name("nLastSend");
    lastRecv @4 :Int64 $X.name("nLastRecv");
    timeConnected @5 :Int64 $X.name("nTimeConnected");
    timeOffset @6 :Int64 $X.name("nTimeOffset");
    addrName @7 :Text;
    version @8 :Int32 $X.name("nVersion");
    cleanSubVer @9 :Text;
    inbound @10 :Bool $X.name("fInbound");
    manualConnection @11 :Bool $X.name("m_manual_connection");
    startingHeight @12 :Int32 $X.name("nStartingHeight");
    sendBytes @13 :UInt64 $X.name("nSendBytes");
    sendBytesPerMsgCmd @14 :List(PairStr64) $X.name("mapSendBytesPerMsgCmd");
    recvBytes @15 :UInt64 $X.name("nRecvBytes");
    recvBytesPerMsgCmd @16 :List(PairStr64) $X.name("mapRecvBytesPerMsgCmd");
    whitelisted @17 :Bool $X.name("fWhitelisted");
    pingTime @18 :Float64 $X.name("dPingTime");
    pingWait @19 :Float64 $X.name("dPingWait");
    minPing @20 :Float64 $X.name("dMinPing");
    addrLocal @21 :Text;
    addr @22 :Data;
    addrBind @23 :Data;
    stateStats @24 :NodeStateStats $X.skip;
}

struct NodeStateStats $X.proxy("CNodeStateStats") {
    misbehavior @0 :Int32 $X.name("nMisbehavior");
    syncHeight @1 :Int32 $X.name("nSyncHeight");
    commonHeight @2 :Int32 $X.name("nCommonHeight");
    heightInFlight @3 :List(Int32) $X.name("vHeightInFlight");
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
struct Recipient $X.proxy("CRecipient") {
    scriptPubKey @0 :Data;
    amount @1 :Int64 $X.name("nAmount");
    subtractFeeFromAmount @2 :Bool $X.name("fSubtractFeeFromAmount");
}
struct Key {
    secret @0 :Data;
    isCompressed @1 :Bool;
}
struct TxDestination {
    keyId @0 :Data;
    scriptId @1 :Data;
}
struct WalletAddress $X.proxy("WalletAddress") {
    dest @0 :TxDestination;
    isMine @1 :Int32 $X.name("is_mine");
    name @2 :Text;
    purpose @3 :Text;
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
    valueMap @9 :List(X.Pair(Text, Text)) $X.name("value_map");
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
    requestCount @3 :Int32 $X.name("request_count");
    timeReceived @4 :UInt32 $X.name("time_received");
    lockTime @5 :UInt32 $X.name("lock_time");
    isFinal @6 :Bool $X.name("is_final");
    isTrusted @7 :Bool $X.name("is_trusted");
    isAbandoned @8 :Bool $X.name("is_abandoned");
    isCoinbase @9 :Bool $X.name("is_coinbase");
    isInMainChain @10 :Bool $X.name("is_in_main_chain");
}

struct ValidationState {
    valid @0 :Bool;
    error @1 :Bool;
    dosCode @2 :Int32;
    rejectCode @3 :UInt32;
    rejectReason @4 :Text;
    corruptionPossible @5 :Bool;
    debugMessage @6 :Text;
}

struct EstimatorBucket $X.proxy("EstimatorBucket")
{
    start @0 :Float64;
    end @1 :Float64;
    withinTarget @2 :Float64;
    totalConfirmed @3 :Float64;
    inMempool @4 :Float64;
    leftMempool @5 :Float64;
}

struct EstimationResult $X.proxy("EstimationResult")
{
    pass @0 :EstimatorBucket;
    fail @1 :EstimatorBucket;
    decay @2 :Float64;
    scale @3 :UInt32;
}

struct FeeCalculation $X.proxy("FeeCalculation") {
    est @0 :EstimationResult;
    reason @1 :Int32;
    desiredTarget @2 :Int32;
    returnedTarget @3 :Int32;
}

struct RPCCommand $X.proxy("CRPCCommand") {
   category @0 :Text;
   name @1 :Text;
   actor @2 :ActorCallback;
   argNames @3 :List(Text);
   uniqueId @4 :Int64 $X.name("unique_id");
}

interface ActorCallback $X.proxy("ProxyCallback<CRPCCommand::Actor>") {
    call @0 (request :JSONRPCRequest) -> (result :UniValue) $X.async;
}

struct JSONRPCRequest $X.proxy("JSONRPCRequest") {
    id @0 :UniValue;
    method @1 :Text $X.name("strMethod");
    params @2 :UniValue;
    help @3 :Bool $X.name("fHelp");
    uri @4 :Text $X.name("URI");
    authUser @5 :Text;
}

struct GlobalArgs {
   args @0 :List(X.Pair(Text, Text));
   multiArgs @1 :List(X.Pair(Text, List(Text)));
   # FIXME: add ChainParams
}
