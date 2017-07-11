src/ipc/capnp -- Cap'n Proto IPC Protocol Implementation
========================================================

Here are the important files in the [`src/ipc`](..) directory:

#### Public headers ####

- [`ipc/interfaces.h`](../interfaces.h)
  – Defines `ipc::Node`, `ipc::Wallet`, `ipc::Handler` interfaces.
    Declares `ipc::MakeNode` function which spawns a `bitcoind` process and
    returns an `ipc::Node` object controlling it over a socket.

- [`ipc/server.h`](../server.h)
  – Declares `ipc::StartServer` function which allows a `bitcoind` process to
    read requests from socket file descriptor and execute commands on an
    `ipc::Node` interface.

#### Private headers and source files ####

- [`ipc/local/interfaces.cpp`](../local/interfaces.cpp)
  – In-process implementations of `ipc::Node`, `ipc::Wallet`, and `ipc::Handler`
  interfaces that call libbitcoin functions directly.

- [`ipc/capnp/interfaces.cpp`](interfaces.cpp)
  – Out-of-process implementations of `ipc::Node`, `ipc::Wallet`, `ipc::Handler`
    interfaces that forward calls to a `bitcoind` process over a socket.

- [`ipc/capnp/server.cpp`](server.cpp)
  – Server IPC code responding to requests.

- [`ipc/capnp/messages.capnp`](messages.capnp)
  – Internal _Cap'n Proto_ interface definitions defining the socket protocol.

- [`ipc/capnp/types.h`](types.h), [`ipc/capnp/types.cpp`](types.cpp)
  – Helper functions for translating IPC messages.

- [`ipc/capnp/util.h`](util.h), [`ipc/capnp/util.cpp`](util.cpp)
  – Helper functions for making and receiving IPC calls.


### IPC Classes ###

* `ipc::Node`
  – Public abstract base class describing an interface for controlling a
    `bitcoind` node. Defined in [`ipc/interfaces.h`](../interfaces.h).

* `ipc::local::NodeImpl`
  – Private concrete subclass of `ipc::Node` which implements the node
    interface by calling `libbitcoin` functions in the current `bitcoind`
    process. Defined in [`ipc/local/interfaces.cpp`](../local/interfaces.cpp).

* `ipc::capnp::NodeImpl`
  – Private concrete subclass of `ipc::Node` which implements the node
    interface by communicating with a `bitcoind` instance over a socket.
    Defined in [`ipc/capnp/interfaces.cpp`](interfaces.cpp).

* `ipc::capnp::Node`
  – Private [capnp interface](https://capnproto.org/language.html#interfaces)
    describing communication across the IPC socket. Defined in
    [`ipc/capnp/messages.capnp`](messages.capnp).

* `ipc::capnp::NodeServer`
  – Private [capnp object](https://capnproto.org/rpc.html#distributed-objects)
    implementing the `ipc::messages::Node` interface and handling incoming
    requests by forwarding them to an `ipc::NodeImpl` object. Defined in
    [`ipc/capnp/server.cpp`](server.cpp).

`ipc::Wallet` and `ipc::Handler` are two other abstract interfaces similar to
`ipc::Node`, which allow an IPC client to keep track of wallets and callback
handlers, respectively. Both of these interfaces have corresponding private
`Impl` and `Server` subclasses just like `ipc::Node`.


### IPC Connection Setup Sequence ###

* IPC connection setup begins with an `ipc::MakeNode(CAPNP)` call in
  `bitcoin-qt`. This creates a [socket
  pair](https://linux.die.net/man/3/socketpair) and spawns a `bitcoind` process
  to listen to one socket and a capnp event loop thread to listen to the other
  socket. Before returning, `MakeNode` constructs an `ipc::capnp::NodeImpl` object,
  which it returns to the caller so it can make IPC calls across the socket.

* When the `bitcoind` process starts, it calls `ipc::StartServer()`, which
  parses the command line to determine the socket file descriptor to listen for
  IPC requests from, then instantiates `ipc::capnp::NodeServer` and
  `ipc::local::NodeImpl` objects to handle the requests. After this, it invokes
  the capnp event loop, which just sits idle until the first request arrives.

### IPC Call Sequence Example ###

* When an IPC client calls the `ipc::capnp::NodeImpl::getNodesStats()` method,
  the method will send a request over the IPC socket, triggering a call to
  `ipc::capnp::NodeServer::getNodesStats()` on the server, which will call
  `ipc::local::NodeImpl::getNodeStats()` to do the actual work.

### IPC Callback Sequence Example ###

* When an IPC client calls the `ipc::capnp::NodeImpl::handleNotifyBlockTip()`
  method with a `std::function` argument, the method will first construct an
  `ipc::capnp::NotifyBlockTipCallbackServer` [capnp
  object](https://capnproto.org/rpc.html#distributed-objects) wrapping the
  `std::function`. It will then include a reference to this object inside the
  IPC request it sends to the server, so the server can call back to it.

* On the server side, the incoming IPC request will trigger a call to
  `ipc::capnp::NodeServer::handleNotifyBlockTip()` which will call
  `ipc::local::NodeImpl::handleNotifyBlockTip()`, passing the latter method a
  `std::function` will get registered to receive notifications. This server-side
  `std::function` will remotely invoke the
  `ipc::capnp::NotifyBlockTipCallbackServer::call()` method running in the
  client, which will invoke the original `std::function` passed to
  `ipc::capnp::NodeImpl::handleNotifyBlockTip()` whenever there is a
  notification.

### Threading Notes ###

* Client methods like `ipc::capnp::NodeImpl::getNodesStats()` will post work to
  the capnp event loop thread to send RPC requests and process responses, but
  will leave the event loop free to do other work between the sending the
  request and receiving the response. This means that multiple client IPC
  requests can execute simultaneously, and server callbacks can continue to be
  processed even while client IPC requests are pending.

* Server methods like `ipc::capnp::NodeServer::getNodesStats()` are called by
  _Cap'n Proto_ from the event loop thread, so it is important that they either
  run very quickly without blocking, or use an asynchronous mechanism like the
  `ipc::util::EventLoop::async()` helper to avoid tying up the server while the
  call is running. `ipc::capnp::NodeServer::appInit()` is an example of a slow
  IPC method that runs asynchronously using the async helper.

* Callback methods like `ipc::capnp::NotifyBlockTipCallbackServer::call()` are
  similar to server methods, and are also called from the event loop thread by
  _Cap'n Proto_. If they are blocking or slow, they also need to be implemented
  using an async mechanism to avoid slowing down other calls and callbacks.
