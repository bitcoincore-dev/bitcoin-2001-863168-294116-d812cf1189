@0x92546c47dc734b2e;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("interfaces::capnp::messages");

using X = import "proxy.capnp";
using Common = import "common.capnp";
using Wallet = import "wallet.capnp";
using Handler = import "handler.capnp";

interface Node $X.proxy("interfaces::Node") {
    destroy @0 (context :X.Context) -> ();
    customSetupServerArgs @1 (context :X.Context) -> () $X.name("setupServerArgs");
    customParseParameters @2 (context :X.Context, argv :List(Text) $X.count(2)) -> (errorStr :Text, result :Bool) $X.name("parseParameters");
    customSoftSetArg @3 (context :X.Context, arg :Text, value :Text) -> (result :Bool) $X.name("softSetArg");
    customSoftSetBoolArg @4 (context :X.Context, arg :Text, value :Bool) -> (result :Bool) $X.name("softSetBoolArg");
    customReadConfigFiles @5 (context :X.Context) -> (errorStr :Text, result: Bool) $X.name("readConfigFiles");
    customSelectParams @6 (context :X.Context, network :Text) -> (error :Text $X.exception("std::exception")) $X.name("selectParams");
    customBaseInitialize @7 (context :X.Context) -> (error :Text $X.exception("std::exception"), result :Bool) $X.name("baseInitialize");
    getAssumedBlockchainSize @8 (context :X.Context) -> (result :UInt64);
    getAssumedChainStateSize @9 (context :X.Context) -> (result :UInt64);
    getNetwork @10 (context :X.Context) -> (result :Text);
    initLogging @11 (context :X.Context) -> ();
    initParameterInteraction @12 (context :X.Context) -> ();
    getWarnings @13 (context :X.Context, type :Text) -> (result :Text);
    getLogCategories @14 (context :X.Context) -> (result :UInt32);
    appInitMain @15 (context :X.Context) -> (error :Text $X.exception("std::exception"), result :Bool);
    appShutdown @16 (context :X.Context) -> ();
    startShutdown @17 (context :X.Context) -> ();
    shutdownRequested @18 (context :X.Context) -> (result :Bool);
    mapPort @19 (context :X.Context, useUPnP :Bool) -> ();
    getProxy @20 (context :X.Context, net :Int32) -> (proxyInfo :Proxy, result :Bool);
    getNodeCount @21 (context :X.Context, flags :Int32) -> (result :UInt64);
    getNodesStats @22 (context :X.Context) -> (stats :List(NodeStats), result :Bool);
    getBanned @23 (context :X.Context) -> (banmap :List(Common.Pair(Data, Data)), result :Bool);
    ban @24 (context :X.Context, netAddr :Data, reason :Int32, banTimeOffset :Int64) -> (result :Bool);
    unban @25 (context :X.Context, ip :Data) -> (result :Bool);
    disconnectByAddress @26 (context :X.Context, address :Data) -> (result :Bool);
    disconnectById @27 (context :X.Context, id :Int64) -> (result :Bool);
    getTotalBytesRecv @28 (context :X.Context) -> (result :Int64);
    getTotalBytesSent @29 (context :X.Context) -> (result :Int64);
    getMempoolSize @30 (context :X.Context) -> (result :UInt64);
    getMempoolDynamicUsage @31 (context :X.Context) -> (result :UInt64);
    getHeaderTip @32 (context :X.Context) -> (height :Int32, blockTime :Int64, result :Bool);
    getNumBlocks @33 (context :X.Context) -> (result :Int32);
    getLastBlockTime @34 (context :X.Context) -> (result :Int64);
    getVerificationProgress @35 (context :X.Context) -> (result :Float64);
    isInitialBlockDownload @36 (context :X.Context) -> (result :Bool);
    getReindex @37 (context :X.Context) -> (result :Bool);
    getImporting @38 (context :X.Context) -> (result :Bool);
    setNetworkActive @39 (context :X.Context, active :Bool) -> ();
    getNetworkActive @40 (context :X.Context) -> (result :Bool);
    estimateSmartFee @41 (context :X.Context, numBlocks :Int32, conservative :Bool, wantReturnedTarget :Bool) -> (returnedTarget :Int32, result :Data);
    getDustRelayFee @42 (context :X.Context) -> (result :Data);
    executeRpc @43 (context :X.Context, command :Text, params :Common.UniValue, uri :Text) -> (error :Text $X.exception("std::exception"), rpcError :Common.UniValue $X.exception("UniValue"), result :Common.UniValue);
    listRpcCommands @44 (context :X.Context) -> (result :List(Text));
    rpcSetTimerInterfaceIfUnset @45 (context :X.Context, iface :Void) -> ();
    rpcUnsetTimerInterface @46 (context :X.Context, iface :Void) -> ();
    getUnspentOutput @47 (context :X.Context, output :Data) -> (coin :Data, result :Bool);
    getWalletDir @48 (context :X.Context) -> (result :Text);
    listWalletDir @49 (context :X.Context) -> (result :List(Text));
    getWallets @50 (context :X.Context) -> (result :List(Wallet.Wallet));
    loadWallet @51 (context :X.Context, name :Text) -> (error :Text, warning: Text, result :Wallet.Wallet);
    handleInitMessage @52 (context :X.Context, callback :InitMessageCallback) -> (result :Handler.Handler);
    handleMessageBox @53 (context :X.Context, callback :MessageBoxCallback) -> (result :Handler.Handler);
    handleQuestion @54 (context :X.Context, callback :QuestionCallback) -> (result :Handler.Handler);
    handleShowProgress @55 (context :X.Context, callback :ShowNodeProgressCallback) -> (result :Handler.Handler);
    handleLoadWallet @56 (context :X.Context, callback :LoadWalletCallback) -> (result :Handler.Handler);
    handleNotifyNumConnectionsChanged @57 (context :X.Context, callback :NotifyNumConnectionsChangedCallback) -> (result :Handler.Handler);
    handleNotifyNetworkActiveChanged @58 (context :X.Context, callback :NotifyNetworkActiveChangedCallback) -> (result :Handler.Handler);
    handleNotifyAlertChanged @59 (context :X.Context, callback :NotifyAlertChangedCallback) -> (result :Handler.Handler);
    handleBannedListChanged @60 (context :X.Context, callback :BannedListChangedCallback) -> (result :Handler.Handler);
    handleNotifyBlockTip @61 (context :X.Context, callback :NotifyBlockTipCallback) -> (result :Handler.Handler);
    handleNotifyHeaderTip @62 (context :X.Context, callback :NotifyHeaderTipCallback) -> (result :Handler.Handler);
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

interface LoadWalletCallback $X.proxy("ProxyCallback<interfaces::Node::LoadWalletFn>") {
    destroy @0 (context :X.Context) -> ();
    call @1 (context :X.Context, wallet :Wallet.Wallet) -> ();
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
    sendBytesPerMsgCmd @14 :List(Common.PairStr64) $X.name("mapSendBytesPerMsgCmd");
    recvBytes @15 :UInt64 $X.name("nRecvBytes");
    recvBytesPerMsgCmd @16 :List(Common.PairStr64) $X.name("mapRecvBytesPerMsgCmd");
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