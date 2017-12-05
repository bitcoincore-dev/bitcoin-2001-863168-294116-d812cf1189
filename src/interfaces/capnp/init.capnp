@0xf2c5cfa319406aa6;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("interfaces::capnp::messages");

using Chain = import "chain.capnp";
using Common = import "common.capnp";
using Node = import "node.capnp";
using Wallet = import "wallet.capnp";
using X = import "proxy.capnp";

interface Init $X.proxy("interfaces::Init") {
    construct @0 (threadMap: X.ThreadMap) -> (threadMap :X.ThreadMap);
    makeNode @1 (context :X.Context) -> (result :Node.Node);
    makeWalletClient @2 (context :X.Context, globalArgs :Common.GlobalArgs, chain :Chain.Chain, walletFilenames :List(Text)) -> (result :Chain.ChainClient);
}
