src/ipc -- Bitcoin Interprocess Communication
=============================================

The IPC functions and classes in [`src/ipc`](.) define interfaces that allow
bitcoin code running in separate processes to communicate.

The interfaces are designed to be protocol agnostic, so support for different
IPC mechanisms (Capn'Proto, gRPC, JSON-RPC, custom protocols) could be added
without having to change application code outside the [`src/ipc`](.) directory.

The interfaces are also expected to evolve over time, to allow gradually
increasing bitcoin code modularity, starting with a simple division between
bitcoin GUI (`bitcoin-qt`) and bitcoind daemon (`bitcoind`) code, and
supporting more subdivisions in the future, such as subdivisions between
bitcoin wallet code and bitcoin P2P networking code, or between P2P networking
and database storage code.

Current Status
--------------

* [`interfaces.h`](interface.h) currently contains interfaces (`ipc::Node` and
  `ipc::Wallet` allowing the bitcoin GUI to access bitcoin node and wallet
  functionality. More interfaces could be added in the future.
* [`server.h`](server.h) implements a `StartServer` function that can listen
  on a pre-exisiting socket and forward commands to an `ipc::Node` interface.
  Support for accepting external TCP or unix socket connections could be added
  in the future.
