@0xa4478fe5ad6d80f5;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("ipc::messages");

interface Node {
    helpMessage @0 (mode :Int32) -> (value :Text);
    handleInitMessage @1 (callback: InitMessageCallback) -> (handler :Handler);
    wallet @2 () -> (wallet :Wallet);
}

interface Wallet {
    getBalance @0 () -> (value :Int64);
}

interface Handler {
    disconnect @0 () -> ();
}

interface InitMessageCallback {
    call @0 (message :Text) -> ();
}
