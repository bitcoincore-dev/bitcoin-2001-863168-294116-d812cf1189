@0xe102a54b33a43a20;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("interface::capnp::test::messages");

using X = import "../proxy.capnp";

interface FooInterface $X.proxy("interface::capnp::test::FooInterface") {
    add @0 (a :Int32, b :Int32) -> (result :Int32);
    mapSize @1 (map :List(X.Pair(Text, Text))) -> (result :UInt64);
    pass @2 (arg :FooStruct) -> (result :FooStruct);
}

struct FooStruct $X.proxy("interface::capnp::test::FooStruct") {
    name @0 :Text;
}