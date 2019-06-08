@0xf2c5cfa319406aa6;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("interfaces::capnp::messages");

using Chain = import "chain.capnp";
using Common = import "common.capnp";
using Node = import "node.capnp";
using Wallet = import "wallet.capnp";
using Proxy = import "proxy.capnp";

interface Init $Proxy.wrap("interfaces::Init") {
    construct @0 (threadMap: Proxy.ThreadMap) -> (threadMap :Proxy.ThreadMap);
    makeNode @1 (context :Proxy.Context) -> (result :Node.Node);
    makeWalletClient @2 (context :Proxy.Context, globalArgs :Common.GlobalArgs, chain :Chain.Chain, walletFilenames :List(Text)) -> (result :Chain.ChainClient);
}
