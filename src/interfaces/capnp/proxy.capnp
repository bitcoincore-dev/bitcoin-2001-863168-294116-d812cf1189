@0xcc316e3f71a040fb;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("interfaces::capnp");

annotation proxy(interface, struct): Text;
annotation count(param, struct): Int32;
annotation exception(param): Text;
annotation name(field, method): Text;
annotation skip(field): Void;

interface Init {
    makeThread @0 (name :Text) -> (result :Thread);
}

interface Thread {
    getName @0 () -> (result: Text);
}

struct Context $count(0) {
   thread @0 : Thread;
   callbackThread @1 : Thread;
}