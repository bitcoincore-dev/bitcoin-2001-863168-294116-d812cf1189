@0xcc316e3f71a040fb;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("interfaces::capnp");

struct AsyncOptions {
    thread @0 :Text;
}

annotation proxy(interface, struct): Text;
annotation count(param): Int32;
annotation exception(param): Text;
annotation async(interface, method): AsyncOptions;
annotation name(field, method): Text;
annotation skip(field): Void;