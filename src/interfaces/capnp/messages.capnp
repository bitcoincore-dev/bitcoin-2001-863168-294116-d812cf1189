@0xa4478fe5ad6d80f5;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("interfaces::capnp::messages");

using X = import "proxy.capnp";

interface Init $X.proxy("interfaces::Init") {
    construct @0 (threadMap: X.ThreadMap) -> (threadMap :X.ThreadMap);
    makeNode @1 (context :X.Context) -> (result :Node);
    makeWalletClient @2 (context :X.Context, globalArgs :GlobalArgs, chain :Chain, walletFilenames :List(Text)) -> (result :ChainClient);
}

interface Chain $X.proxy("interfaces::Chain") {
    destroy @0 (context :X.Context) -> ();
    lock @1 (context :X.Context, tryLock :Bool) -> (result :ChainLock);
    assumeLocked @2 (context :X.Context) -> (result :ChainLock);
    findBlock @3 (context :X.Context, hash :Data, wantBlock :Bool, wantTime :Bool, wantMaxTime :Bool) -> (block :Data, time :Int64, maxTime :Int64, result :Bool);
    findCoins @4 (context :X.Context, output :List(Data)) -> (result :List(Data));
    guessVerificationProgress @5 (context :X.Context, blockHash :Data) -> (result :Float64);
    getVirtualTransactionSize @6 (context :X.Context, tx :Data) -> (result :Int64);
    isRBFOptIn @7 (context :X.Context, tx :Data) -> (result :Int32);
    hasDescendantsInMempool @8 (context :X.Context, txid :Data) -> (result :Bool);
    relayTransaction @9 (context :X.Context, txid :Data) -> (result :Bool);
    getTransactionAncestry @10 (context :X.Context, txid :Data) -> (ancestors :UInt64, descendants :UInt64);
    checkChainLimits @11 (context :X.Context, tx :Data) -> (result :Bool);
    estimateSmartFee @12 (context :X.Context, numBlocks :Int32, conservative :Bool, wantCalc :Bool) -> (calc :FeeCalculation, result :Data);
    estimateMaxBlocks @13 (context :X.Context) -> (result :Int32);
    poolMinFee @14 (context :X.Context) -> (result :Data);
    getPruneMode @15 (context :X.Context) -> (result :Bool);
    p2pEnabled @16 (context :X.Context) -> (result :Bool);
    isInitialBlockDownload @17 (context :X.Context) -> (result :Bool);
    getAdjustedTime @18 (context :X.Context) -> (result :Int64);
    initMessage @19 (context :X.Context, message :Text) -> ();
    initWarning @20 (context :X.Context, message :Text) -> ();
    initError @21 (context :X.Context, message :Text) -> ();
    loadWallet @22 (context :X.Context, wallet :Wallet) -> ();
    generateBlocks @23 (context :X.Context, coinbaseScript :Data, numBlocks :Int32, maxTries:UInt64, keepScript:Bool) -> (result :UniValue);
    parseConfirmTarget @24 (context :X.Context, value :UniValue) -> (result :UInt32);
    handleNotifications @25 (context :X.Context, notifications :ChainNotifications) -> (result :Handler);
    waitForNotifications @26 (context :X.Context) -> ();
    handleRpc @27 (context :X.Context, command :RPCCommand) -> (result :Handler);
}

interface ChainLock $X.proxy("interfaces::Chain::Lock") {
    destroy @0 (context :X.Context) -> ();
    getHeight @1 (context :X.Context) -> (result :Int32, hasResult :Bool);
    getBlockHeight @2 (context :X.Context, hash :Data) -> (result :Int32, hasResult :Bool);
    getBlockDepth @3 (context :X.Context, hash :Data) -> (result :Int32);
    getBlockHash @4 (context :X.Context, height :Int32) -> (result :Data);
    getBlockTime @5 (context :X.Context, height :Int32) -> (result :Int64);
    getBlockMedianTimePast @6 (context :X.Context, height :Int32) -> (result :Int64);
    haveBlockOnDisk @7 (context :X.Context, height :Int32) -> (result :Bool);
    findFirstBlockWithTime @8 (context :X.Context, time :Int64) -> (result :Int32, hasResult :Bool);
    findFirstBlockWithTimeAndHeight @9 (context :X.Context, time :Int64, startHeight :Int32) -> (result :Int32, hasResult :Bool);
    findPruned @10 (context :X.Context, startHeight: Int32, stopHeight :Int32, hasStopHeight :Bool) -> (result :Int32, hasResult :Bool);
    findFork @11 (context :X.Context, hash :Data, wantHeight :Bool) -> (height :Int32, hasHeight :Int32, result :Int32, hasResult :Bool);
    isPotentialTip @12 (context :X.Context, hash :Data) -> (result :Bool);
    getLocator @13 (context :X.Context) -> (result :Data);
    findLocatorFork @14 (context :X.Context, locator :Data) -> (result :Int32, hasResult :Bool);
    checkFinalTx @15 (context :X.Context, tx :Data) -> (result :Bool);
    acceptToMemoryPool @16 (context :X.Context, tx :Data, state :ValidationState) -> (state :ValidationState, result :Bool);
}

interface ChainNotifications $X.proxy("interfaces::Chain::Notifications") {
    destroy @0 (context :X.Context) -> ();
    transactionAddedToMempool @1 (context :X.Context, tx :Data) -> () $X.name("TransactionAddedToMempool");
    transactionRemovedFromMempool @2 (context :X.Context, tx :Data) -> () $X.name("TransactionRemovedFromMempool");
    blockConnected @3 (context :X.Context, block :Data, blockHash :Data, txConflicted :List(Data)) -> () $X.name("BlockConnected");
    blockDisconnected @4 (context :X.Context, block :Data) -> () $X.name("BlockDisconnected");
    chainStateFlushed @5 (context :X.Context, locator :Data) -> () $X.name("ChainStateFlushed");
    resendWalletTransactions @6 (context :X.Context, bestBlockTime :Int64) -> () $X.name("ResendWalletTransactions");
}

interface ChainClient $X.proxy("interfaces::ChainClient") {
    destroy @0 (context :X.Context) -> ();
    registerRpcs @1 (context :X.Context) -> ();
    verify @2 (context :X.Context) -> (result :Bool);
    load @3 (context :X.Context) -> (result :Bool);
    start @4 (context :X.Context, scheduler :Void) -> ();
    flush @5 (context :X.Context) -> ();
    stop @6 (context :X.Context) -> ();
    setMockTime @7 (context :X.Context, time :Int64) -> ();
    getWallets @8 (context :X.Context) -> (result :List(Wallet));
}

interface Node $X.proxy("interfaces::Node") {
    destroy @0 (context :X.Context) -> ();
    customSetupServerArgs @1 (context :X.Context) -> () $X.name("setupServerArgs");
    customParseParameters @2 (context :X.Context, argv :List(Text) $X.count(2)) -> (errorStr :Text, result :Bool) $X.name("parseParameters");
    customSoftSetArg @3 (context :X.Context, arg :Text, value :Text) -> (result :Bool) $X.name("softSetArg");
    customSoftSetBoolArg @4 (context :X.Context, arg :Text, value :Bool) -> (result :Bool) $X.name("softSetBoolArg");
    customReadConfigFiles @5 (context :X.Context) -> (errorStr :Text, result: Bool) $X.name("readConfigFiles");
    customSelectParams @6 (context :X.Context, network :Text) -> (error :Text $X.exception("std::exception")) $X.name("selectParams");
    getNetwork @7 (context :X.Context) -> (result :Text);
    getArg @8 (context :X.Context, arg :Text, defaultValue: Text) -> (result :Text);
    initLogging @9 (context :X.Context) -> ();
    initParameterInteraction @10 (context :X.Context) -> ();
    getWarnings @11 (context :X.Context, type :Text) -> (result :Text);
    getLogCategories @12 (context :X.Context) -> (result :UInt32);
    baseInitialize @13 (context :X.Context) -> (error :Text $X.exception("std::exception"), result :Bool);
    appInitMain @14 (context :X.Context) -> (error :Text $X.exception("std::exception"), result :Bool);
    appShutdown @15 (context :X.Context) -> ();
    startShutdown @16 (context :X.Context) -> ();
    shutdownRequested @17 (context :X.Context) -> (result :Bool);
    mapPort @18 (context :X.Context, useUPnP :Bool) -> ();
    getProxy @19 (context :X.Context, net :Int32) -> (proxyInfo :Proxy, result :Bool);
    getNodeCount @20 (context :X.Context, flags :Int32) -> (result :UInt64);
    getNodesStats @21 (context :X.Context) -> (stats :List(NodeStats), result :Bool);
    getBanned @22 (context :X.Context) -> (banmap :List(Pair(Data, Data)), result :Bool);
    ban @23 (context :X.Context, netAddr :Data, reason :Int32, banTimeOffset :Int64) -> (result :Bool);
    unban @24 (context :X.Context, ip :Data) -> (result :Bool);
    disconnect @25 (context :X.Context, id :Int64) -> (result :Bool);
    getTotalBytesRecv @26 (context :X.Context) -> (result :Int64);
    getTotalBytesSent @27 (context :X.Context) -> (result :Int64);
    getMempoolSize @28 (context :X.Context) -> (result :UInt64);
    getMempoolDynamicUsage @29 (context :X.Context) -> (result :UInt64);
    getHeaderTip @30 (context :X.Context) -> (height :Int32, blockTime :Int64, result :Bool);
    getNumBlocks @31 (context :X.Context) -> (result :Int32);
    getLastBlockTime @32 (context :X.Context) -> (result :Int64);
    getVerificationProgress @33 (context :X.Context) -> (result :Float64);
    isInitialBlockDownload @34 (context :X.Context) -> (result :Bool);
    getReindex @35 (context :X.Context) -> (result :Bool);
    getImporting @36 (context :X.Context) -> (result :Bool);
    setNetworkActive @37 (context :X.Context, active :Bool) -> ();
    getNetworkActive @38 (context :X.Context) -> (result :Bool);
    getMaxTxFee @39 (context :X.Context) -> (result :Int64);
    estimateSmartFee @40 (context :X.Context, numBlocks :Int32, conservative :Bool, wantReturnedTarget :Bool) -> (returnedTarget :Int32, result :Data);
    getDustRelayFee @41 (context :X.Context) -> (result :Data);
    executeRpc @42 (context :X.Context, command :Text, params :UniValue, uri :Text) -> (error :Text $X.exception("std::exception"), rpcError :UniValue $X.exception("UniValue"), result :UniValue);
    listRpcCommands @43 (context :X.Context) -> (result :List(Text));
    rpcSetTimerInterfaceIfUnset @44 (context :X.Context, iface :Void) -> ();
    rpcUnsetTimerInterface @45 (context :X.Context, iface :Void) -> ();
    getUnspentOutput @46 (context :X.Context, output :Data) -> (coin :Data, result :Bool);
    getWallets @47 (context :X.Context) -> (result :List(Wallet));
    handleInitMessage @48 (context :X.Context, callback :InitMessageCallback) -> (result :Handler);
    handleMessageBox @49 (context :X.Context, callback :MessageBoxCallback) -> (result :Handler);
    handleQuestion @50 (context :X.Context, callback :QuestionCallback) -> (result :Handler);
    handleShowProgress @51 (context :X.Context, callback :ShowNodeProgressCallback) -> (result :Handler);
    handleLoadWallet @52 (context :X.Context, callback :LoadWalletCallback) -> (result :Handler);
    handleNotifyNumConnectionsChanged @53 (context :X.Context, callback :NotifyNumConnectionsChangedCallback) -> (result :Handler);
    handleNotifyNetworkActiveChanged @54 (context :X.Context, callback :NotifyNetworkActiveChangedCallback) -> (result :Handler);
    handleNotifyAlertChanged @55 (context :X.Context, callback :NotifyAlertChangedCallback) -> (result :Handler);
    handleBannedListChanged @56 (context :X.Context, callback :BannedListChangedCallback) -> (result :Handler);
    handleNotifyBlockTip @57 (context :X.Context, callback :NotifyBlockTipCallback) -> (result :Handler);
    handleNotifyHeaderTip @58 (context :X.Context, callback :NotifyHeaderTipCallback) -> (result :Handler);
}

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
    tryGetTxStatus @37 (context :X.Context, txid :Data) -> (txStatus :WalletTxStatus, numBlocks :Int32, adjustedTime :Int64, result :Bool);
    getWalletTxDetails @38 (context :X.Context, txid :Data) -> (txStatus :WalletTxStatus, orderForm :List(Pair(Text, Text)), inMempool :Bool, numBlocks :Int32, adjustedTime :Int64, result :WalletTx);
    getBalances @39 (context :X.Context) -> (result :WalletBalances);
    tryGetBalances @40 (context :X.Context) -> (balances :WalletBalances, numBlocks :Int32, result :Bool);
    getBalance @41 (context :X.Context) -> (result :Int64);
    getAvailableBalance @42 (context :X.Context, coinControl :CoinControl) -> (result :Int64);
    txinIsMine @43 (context :X.Context, txin :Data) -> (result :Int32);
    txoutIsMine @44 (context :X.Context, txout :Data) -> (result :Int32);
    getDebit @45 (context :X.Context, txin :Data, filter :Int32) -> (result :Int64);
    getCredit @46 (context :X.Context, txout :Data, filter :Int32) -> (result :Int64);
    listCoins @47 (context :X.Context) -> (result :List(Pair(TxDestination, List(Pair(Data, WalletTxOut)))));
    getCoins @48 (context :X.Context, outputs :List(Data)) -> (result :List(WalletTxOut));
    getRequiredFee @49 (context :X.Context, txBytes :UInt32) -> (result :Int64);
    getMinimumFee @50 (context :X.Context, txBytes :UInt32, coinControl :CoinControl, wantReturnedTarget :Bool, wantReason :Bool) -> (returnedTarget :Int32, reason :Int32, result :Int64);
    getConfirmTarget @51 (context :X.Context) -> (result :UInt32);
    hdEnabled @52 (context :X.Context) -> (result :Bool);
    privateKeysDisabled @53 (context :X.Context) -> (result :Bool);
    getDefaultAddressType @54 (context :X.Context) -> (result :Int32);
    getDefaultChangeType @55 (context :X.Context) -> (result :Int32);
    handleUnload @56 (context :X.Context, callback :UnloadWalletCallback) -> (result :Handler);
    handleShowProgress @57 (context :X.Context, callback :ShowWalletProgressCallback) -> (result :Handler);
    handleStatusChanged @58 (context :X.Context, callback :StatusChangedCallback) -> (result :Handler);
    handleAddressBookChanged @59 (context :X.Context, callback :AddressBookChangedCallback) -> (result :Handler);
    handleTransactionChanged @60 (context :X.Context, callback :TransactionChangedCallback) -> (result :Handler);
    handleWatchOnlyChanged @61 (context :X.Context, callback :WatchOnlyChangedCallback) -> (result :Handler);
}

interface Handler $X.proxy("interfaces::Handler") {
    destroy @0 (context :X.Context) -> ();
    disconnect @1 (context :X.Context) -> ();
}

interface PendingWalletTx $X.proxy("interfaces::PendingWalletTx") {
    destroy @0 (context :X.Context) -> ();
    customGet @1 (context :X.Context) -> (result :Data) $X.name("get");
    getVirtualSize @2 (context :X.Context) -> (result :Int64);
    commit @3 (context :X.Context, valueMap :List(Pair(Text, Text)), orderForm :List(Pair(Text, Text))) -> (rejectReason :Text, result :Bool);
}

interface InitMessageCallback $X.proxy("ProxyCallback<interfaces::Node::InitMessageFn>") {
    destroy @0 (context :X.Context) -> ();
    call @1 (context :X.Context, message :Text) -> ();
}

interface MessageBoxCallback $X.proxy("ProxyCallback<interfaces::Node::MessageBoxFn>") {
    destroy @0 (context :X.Context) -> ();
    call @1 (context :X.Context, message :Text, caption :Text, style :UInt32) -> (result :Bool);
}

interface QuestionCallback $X.proxy("ProxyCallback<interfaces::Node::QuestionFn>") {
    destroy @0 (context :X.Context) -> ();
    call @1 (context :X.Context, message :Text, nonInteractiveMessage :Text, caption :Text, style :UInt32) -> (result :Bool);
}

interface ShowNodeProgressCallback $X.proxy("ProxyCallback<interfaces::Node::ShowProgressFn>") {
    destroy @0 (context :X.Context) -> ();
    call @1 (context :X.Context, title :Text, progress :Int32, resumePossible :Bool) -> ();
}

interface UnloadWalletCallback $X.proxy("ProxyCallback<interfaces::Wallet::UnloadFn>") {
    destroy @0 (context :X.Context) -> ();
    call @1 (context :X.Context) -> ();
}

interface ShowWalletProgressCallback $X.proxy("ProxyCallback<interfaces::Wallet::ShowProgressFn>") {
    destroy @0 (context :X.Context) -> ();
    call @1 (context :X.Context, title :Text, progress :Int32) -> ();
}

interface LoadWalletCallback $X.proxy("ProxyCallback<interfaces::Node::LoadWalletFn>") {
    destroy @0 (context :X.Context) -> ();
    call @1 (context :X.Context, wallet :Wallet) -> ();
}

interface NotifyNumConnectionsChangedCallback $X.proxy("ProxyCallback<interfaces::Node::NotifyNumConnectionsChangedFn>") {
    destroy @0 (context :X.Context) -> ();
    call @1 (context :X.Context, newNumConnections :Int32) -> ();
}

interface NotifyNetworkActiveChangedCallback $X.proxy("ProxyCallback<interfaces::Node::NotifyNetworkActiveChangedFn>") {
    destroy @0 (context :X.Context) -> ();
    call @1 (context :X.Context, networkActive :Bool) -> ();
}

interface NotifyAlertChangedCallback $X.proxy("ProxyCallback<interfaces::Node::NotifyAlertChangedFn>") {
    destroy @0 (context :X.Context) -> ();
    call @1 (context :X.Context) -> ();
}

interface BannedListChangedCallback $X.proxy("ProxyCallback<interfaces::Node::BannedListChangedFn>") {
    destroy @0 (context :X.Context) -> ();
    call @1 (context :X.Context) -> ();
}

interface NotifyBlockTipCallback $X.proxy("ProxyCallback<interfaces::Node::NotifyBlockTipFn>") {
    destroy @0 (context :X.Context) -> ();
    call @1 (context :X.Context, initialDownload :Bool, height :Int32, blockTime :Int64, verificationProgress :Float64) -> ();
}

interface NotifyHeaderTipCallback $X.proxy("ProxyCallback<interfaces::Node::NotifyHeaderTipFn>") {
    destroy @0 (context :X.Context) -> ();
    call @1 (context :X.Context, initialDownload :Bool, height :Int32, blockTime :Int64, verificationProgress :Float64) -> ();
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

struct Pair(Key, Value) {
    key @0 :Key;
    value @1 :Value;
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
    setSelected @13 :List(Data);
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
    valueMap @9 :List(Pair(Text, Text)) $X.name("value_map");
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
    call @0 (context :X.Context, request :JSONRPCRequest) -> (error :Text $X.exception("std::exception"), rpcError :UniValue $X.exception("UniValue"), result :UniValue);
}

struct JSONRPCRequest $X.proxy("JSONRPCRequest") {
    id @0 :UniValue;
    method @1 :Text $X.name("strMethod");
    params @2 :UniValue;
    help @3 :Bool $X.name("fHelp");
    uri @4 :Text $X.name("URI");
    authUser @5 :Text;
}

struct GlobalArgs $X.proxy("interfaces::capnp::GlobalArgs") $X.count(0) {
   overrideArgs @0 :List(Pair(Text, List(Text))) $X.name("m_override_args");
   configArgs @1 :List(Pair(Text, List(Text))) $X.name("m_config_args");
   network @2 :Text $X.name("m_network");
   networkOnlyArgs @3 :List(Text) $X.name("m_network_only_args");
}
