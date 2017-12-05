@0xebd8f46e2f369076;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("interfaces::capnp::messages");

using X = import "proxy.capnp";

interface Handler $X.proxy("interfaces::Handler") {
    destroy @0 (context :X.Context) -> ();
    disconnect @1 (context :X.Context) -> ();
}