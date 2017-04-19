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
* [`interfaces.h`](interfaces.h) currently defines a `LOCAL` IPC protocol for
  calling functionality linked into the current process, and a `CAPNP` protocol
  for calling functionality in a different process across a socket. Details
  about the `CAPNP` implementation can be found in
  [src/ipc/capnp/README.md](capnp/README.md). Support for other protocols
  could be added in the future.
* [`server.h`](server.h) implements a `StartServer` function that can listen
  on a pre-exisiting socket and forward commands to an `ipc::Node` interface.
  Support for accepting external TCP or unix socket connections could be added
  in the future.

FAQ
---

### Does `bitcoin-qt` / `bitcoind` process separation allow the GUI and daemon processes to be started and stopped independently, or run across the network on different machines, or allow the same daemon process to be controlled by multiple GUI processes?

Not yet, but these are obvious next steps. To enable this, `bitcoind` needs a
new configuration option that will tell it to create a listening socket and bind
to a TCP or domain socket address, and `bitcoin-qt` will need a new option for
connecting to an existing `bitcoind` instead of spawning its own.

### How can IPC framework be used to implement Node / Wallet separation?

The main change that needs to happen is porting [src/wallet/](../wallet) code to
call an IPC interface instead of accessing bitcoin daemon global variable and
functions directly. As with the Qt IPC port,
[hide-globals.py](https://github.com/ryanofsky/home/blob/master/src/2017/hide-globals/hide-globals.py)
and
[replace-syms.py](https://github.com/ryanofsky/home/blob/master/src/2017/hide-globals/replace-syms.py)
scripts could be used to identify the call locations that need to be updated
(though they should be more obvious in wallet code than they were in Qt code
because they are more concentrated). The wallet code could be given access to
the existing [`ipc::Node`](interfaces.h) interface, but it would be better to
define a new, more minimal interface (perhaps `ipc::BlockChain`) for the wallet
to be able to query blockchain state and register for notifications without
having access to other node functionality.

Once wallet code is able to function over an IPC interface, different
multiprocess arrangements are possible. For backwards compatibility, the
`bitcoind` process could spawn one or more wallet processes on startup. Or it
could listen on a TCP or unix domain socket address allowing new wallet
processes to be started independently and connect. New `bitcoin-qt` options
could be added for opening GUIs connected to independent wallet processes.

### Does having an IPC layer make Qt model classes redundant?

No. The IPC layer is intended only to be a simple shim between different parts
of bitcoin code. No real functionality is implemented there, and the most
complicated thing any IPC method in
[src/ipc/local/interfaces.cpp](local/interfaces.cpp) should do is a call single
bitcoin function, or batch together a few function calls for
efficiency or atomicity and group together their results.

Any real functionality should be implemented outside of the IPC layer. The Qt
model files ([src/qt/walletmodel.cpp](../qt/walletmodel.cpp)),
([src/qt/clientmodel.cpp](../qt/clientmodel.cpp), etc) are the best place to
implement functionality that's useful to GUI code but not useful to other parts
of bitcoin.

Examples:

* `WalletImpl::getBalances()` in
  [src/ipc/local/interfaces.cpp](local/interfaces.cpp) shows a typical IPC
  method implementation which groups together the results of a few lower-level
  bitcoin calls, but does not implement any real functionality of its own.

* PR [#10231](https://github.com/bitcoin/bitcoin/pull/10231), which adds caching
  of header information to GUI, shows an example of the type of functionality
  that belongs in Qt model classes. The caching it adds could have been
  implemented at a lower level, but belongs in Qt because the information it
  caches is only useful to the GUI.

* PR [#10251](https://github.com/bitcoin/bitcoin/pull/10251), which adds caching
  of balance information, is useful to both the GUI and the JSON-RPC interfaces,
  so was implemented at a lower level.
