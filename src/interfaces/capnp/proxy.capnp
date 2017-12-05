@0xcc316e3f71a040fb;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("interfaces::capnp");

annotation proxy(interface, struct): Text;
annotation count(param): Int32;
annotation exception(param): Text;
annotation async(interface, method): Void;
annotation name(field, method): Text;
annotation skip(field): Void;