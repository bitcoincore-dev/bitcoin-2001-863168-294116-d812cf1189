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

* [`interfaces.h`](interfaces.h) currently contains interfaces (`ipc::Node` and
  `ipc::Wallet` allowing the bitcoin GUI to access bitcoin node and wallet
  functionality. More interfaces could be added in the future.
* [`interfaces.h`](interfaces.h) currently only defines a `LOCAL` IPC protocol
  for calling functionality already linked into the current process. The first
  implementation of a remote protocol allowing real interprocess communication
  is added in PR [#10102](https://github.com/bitcoin/bitcoin/pull/10102).
