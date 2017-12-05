@0x94f21a4864bd2c65;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("interfaces::capnp::messages");

using X = import "proxy.capnp";
using Common = import "common.capnp";
using Handler = import "handler.capnp";
using Wallet = import "wallet.capnp";

interface Chain $X.proxy("interfaces::Chain") {
    destroy @0 (context :X.Context) -> ();
    lock @1 (context :X.Context, tryLock :Bool) -> (result :ChainLock);
    findBlock @2 (context :X.Context, hash :Data, wantBlock :Bool, wantTime :Bool, wantMaxTime :Bool) -> (block :Data, time :Int64, maxTime :Int64, result :Bool);
    findCoins @3 (context :X.Context, coins :List(Common.Pair(Data, Data))) -> (coins :List(Common.Pair(Data, Data)));
    guessVerificationProgress @4 (context :X.Context, blockHash :Data) -> (result :Float64);
    isRBFOptIn @5 (context :X.Context, tx :Data) -> (result :Int32);
    hasDescendantsInMempool @6 (context :X.Context, txid :Data) -> (result :Bool);
    relayTransaction @7 (context :X.Context, txid :Data) -> ();
    getTransactionAncestry @8 (context :X.Context, txid :Data) -> (ancestors :UInt64, descendants :UInt64);
    checkChainLimits @9 (context :X.Context, tx :Data) -> (result :Bool);
    estimateSmartFee @10 (context :X.Context, numBlocks :Int32, conservative :Bool, wantCalc :Bool) -> (calc :FeeCalculation, result :Data);
    estimateMaxBlocks @11 (context :X.Context) -> (result :UInt32);
    mempoolMinFee @12 (context :X.Context) -> (result :Data);
    relayMinFee @13 (context :X.Context) -> (result :Data);
    relayIncrementalFee @14 (context :X.Context) -> (result :Data);
    relayDustFee @15 (context :X.Context) -> (result :Data);
    havePruned @16 (context :X.Context) -> (result :Bool);
    p2pEnabled @17 (context :X.Context) -> (result :Bool);
    isReadyToBroadcast @18 (context :X.Context) -> (result :Bool);
    isInitialBlockDownload @19 (context :X.Context) -> (result :Bool);
    shutdownRequested @20 (context :X.Context) -> (result :Bool);
    getAdjustedTime @21 (context :X.Context) -> (result :Int64);
    initMessage @22 (context :X.Context, message :Text) -> ();
    initWarning @23 (context :X.Context, message :Text) -> ();
    initError @24 (context :X.Context, message :Text) -> ();
    loadWallet @25 (context :X.Context, wallet :Wallet.Wallet) -> ();
    showProgress @26 (context :X.Context, title :Text, progress :Int32, resumePossible :Bool) -> ();
    handleNotifications @27 (context :X.Context, notifications :ChainNotifications) -> (result :Handler.Handler);
    waitForNotificationsIfNewBlocksConnected @28 (context :X.Context, oldTip :Data) -> ();
    handleRpc @29 (context :X.Context, command :RPCCommand) -> (result :Handler.Handler);
    rpcEnableDeprecated @30 (context :X.Context, method :Text) -> (result :Bool);
    rpcRunLater @31 (context :X.Context, name :Text, fn: RunLaterCallback, seconds: Int64) -> ();
    rpcSerializationFlags @32 (context :X.Context) -> (result :Int32);
    requestMempoolTransactions @33 (context :X.Context, notifications :ChainNotifications) -> ();
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
    findFirstBlockWithTimeAndHeight @8 (context :X.Context, time :Int64, startHeight :Int32) -> (hash :Data, result :Int32, hasResult :Bool);
    findPruned @9 (context :X.Context, startHeight: Int32, stopHeight :Int32, hasStopHeight :Bool) -> (result :Int32, hasResult :Bool);
    findFork @10 (context :X.Context, hash :Data, wantHeight :Bool) -> (height :Int32, hasHeight :Int32, result :Int32, hasResult :Bool);
    getTipLocator @11 (context :X.Context) -> (result :Data);
    findLocatorFork @12 (context :X.Context, locator :Data) -> (result :Int32, hasResult :Bool);
    checkFinalTx @13 (context :X.Context, tx :Data) -> (result :Bool);
    submitToMemoryPool @14 (context :X.Context, tx :Data, absurdFee: Int64, state :ValidationState) -> (state :ValidationState, result :Bool);
}

interface ChainNotifications $X.proxy("interfaces::Chain::Notifications") {
    destroy @0 (context :X.Context) -> ();
    transactionAddedToMempool @1 (context :X.Context, tx :Data) -> () $X.name("TransactionAddedToMempool");
    transactionRemovedFromMempool @2 (context :X.Context, tx :Data) -> () $X.name("TransactionRemovedFromMempool");
    blockConnected @3 (context :X.Context, block :Data, txConflicted :List(Data)) -> () $X.name("BlockConnected");
    blockDisconnected @4 (context :X.Context, block :Data) -> () $X.name("BlockDisconnected");
    chainStateFlushed @5 (context :X.Context, locator :Data) -> () $X.name("ChainStateFlushed");
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
    getWallets @8 (context :X.Context) -> (result :List(Wallet.Wallet));
}

struct FeeCalculation $X.proxy("FeeCalculation") {
    est @0 :EstimationResult;
    reason @1 :Int32;
    desiredTarget @2 :Int32;
    returnedTarget @3 :Int32;
}

struct EstimationResult $X.proxy("EstimationResult")
{
    pass @0 :EstimatorBucket;
    fail @1 :EstimatorBucket;
    decay @2 :Float64;
    scale @3 :UInt32;
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

struct RPCCommand $X.proxy("CRPCCommand") {
   category @0 :Text;
   name @1 :Text;
   actor @2 :ActorCallback;
   argNames @3 :List(Text);
   uniqueId @4 :Int64 $X.name("unique_id");
}

interface ActorCallback $X.proxy("ProxyCallback<CRPCCommand::Actor>") {
    call @0 (context :X.Context, request :JSONRPCRequest, response :Common.UniValue, lastCallback :Bool) -> (error :Text $X.exception("std::exception"), rpcError :Common.UniValue $X.exception("UniValue"), response :Common.UniValue, result: Bool);
}

struct JSONRPCRequest $X.proxy("JSONRPCRequest") {
    id @0 :Common.UniValue;
    method @1 :Text $X.name("strMethod");
    params @2 :Common.UniValue;
    help @3 :Bool $X.name("fHelp");
    uri @4 :Text $X.name("URI");
    authUser @5 :Text;
}

interface RunLaterCallback $X.proxy("ProxyCallback<std::function<void()>>") {
    destroy @0 (context :X.Context) -> ();
    call @1 (context :X.Context) -> ();
}

struct ValidationState {
    valid @0 :Bool;
    error @1 :Bool;
    reason @2 :Int32;
    rejectCode @3 :UInt32;
    rejectReason @4 :Text;
    debugMessage @5 :Text;
}
