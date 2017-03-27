src/ipc -- Bitcoin Interprocess Communication
=============================================

The IPC functions and classes in [`src/ipc`](.) define interfaces that allow
bitcoin code running in separate processes to communicate.

The interfaces are designed to be protocol agnostic, so support for different
IPC mechanisms (Capn'Proto, gRPC, JSON-RPC, or custom protocols) could be added
without having to change application code outside the [`src/ipc`](.) directory.

The interfaces are also expected to evolve over time, to allow gradually
increasing bitcoin code modularity, starting with a simple division between
bitcoin GUI (`bitcoin-qt`) and bitcoind daemon (`bitcoind`) code, and
supporting more subdivisions in the future, such as subdivisions between
bitcoin wallet code and bitcoin P2P networking code, or between P2P networking
and database storage code.

Current Status
--------------

* [`src/ipc/interfaces.h`](interfaces.h) currently contains interfaces
  `ipc::Node` and `ipc::Wallet` allowing the bitcoin GUI to access bitcoin
  node and wallet functionality. More interfaces could be added in the future.
* [`src/ipc/interfaces.h`](interfaces.h) currently only defines a `LOCAL` IPC protocol
  for calling functionality already linked into the current process. The first
  implementation of a remote protocol allowing real interprocess communication
  is added in PR [#10102](https://github.com/bitcoin/bitcoin/pull/10102).

FAQ
---

### Does having an IPC layer make Qt model classes redundant?

The IPC layer is complementary to the Qt model classes. The IPC layer is only a
simple shim wrapping a selection of node and wallet functions that the GUI
needs access to. No real functionality is implemented there, and the most
complicated thing any IPC method in
[src/ipc/local/interfaces.cpp](local/interfaces.cpp) should do is a call a
single bitcoin function, or batch a few function calls together and group their
results.

All real functionality is implemented outside of the IPC layer, either above it
in Qt model classes, or below it in bitcoin node and wallet code. The Qt model
classes in ([src/qt/walletmodel.cpp](../qt/walletmodel.cpp)),
([src/qt/clientmodel.cpp](../qt/clientmodel.cpp), etc) are the best place to
implement functionality that's useful to GUI code but not useful to other parts
of bitcoin.

Examples of expected code organization:

* `WalletImpl::getBalances()` in
  [src/ipc/local/interfaces.cpp](local/interfaces.cpp) shows a typical IPC
  method implementation which groups together the results of a few lower-level
  bitcoin calls, but does not implement any real functionality of its own.

* PR [#10231](https://github.com/bitcoin/bitcoin/pull/10231), which adds caching
  of header information to GUI, shows an example of the type of functionality
  that belongs in Qt model classes. The caching it adds could have been
  implemented at a lower level, but it belongs in Qt because the information it
  tracks is only useful to the GUI.

* PR [#10251](https://github.com/bitcoin/bitcoin/pull/10251), which adds caching
  of balance information, is useful to both the GUI and the JSON-RPC interfaces,
  so was implemented at a lower level.
