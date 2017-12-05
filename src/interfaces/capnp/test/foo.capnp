@0xe102a54b33a43a20;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("interfaces::capnp::test::messages");

using X = import "../proxy.capnp";

interface FooInterface $X.proxy("interfaces::capnp::test::FooInterface") {
    add @0 (a :Int32, b :Int32) -> (result :Int32);
    mapSize @1 (map :List(Pair(Text, Text))) -> (result :UInt64);
    pass @2 (arg :FooStruct) -> (result :FooStruct);
}

struct FooStruct $X.proxy("interfaces::capnp::test::FooStruct") {
    name @0 :Text;
}

struct Pair(Key, Value) {
    key @0 :Key;
    value @1 :Value;
}
