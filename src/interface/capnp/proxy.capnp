@0xcc316e3f71a040fb;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("interface::capnp");

annotation proxy(interface, struct): Text;
annotation count(param): Int32;
annotation async(method): Void;
annotation name(field, method): Text;
annotation skip(field): Void;

struct Pair(Key, Value) {
    key @0 :Key;
    value @1 :Value;
}