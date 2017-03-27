Bitcoin Interprocess Communication
==================================

The IPC functions and classes in `src/ipc/` allow the `bitcoin-qt` process to
fork a `bitcoind` process and communicate with it over a socketpair. This gives
`bitcoin-qt` a code a bit of modularity, avoiding a monolithic architecture
where UI, P2P, wallet, consensus, and database code all runs in a single
process.

In the future, the IPC mechanism could be extended to allow other subdivisions
of functionality. For example, wallet code could run in a separate process from
P2P code. Also in the future, IPC sockets could also be exposed more publicly.
For example, exposed IPC server sockets could allow a `bitcoin-qt` process to
control a `bitcoind` process other than the one it spawns internally. These
changes would be straightforward to implement, but would create security,
backwards-compatibility, complexity, and maintainablity concerns, so would
require further discussion.

The IPC protocol used is [Cap'n Proto](https://capnproto.org/), but the protocol
could be swapped out for another protocol (such as JSON-RPC with long polling)
without changing any code outside of the `src/ipc/` directory. No Cap'n Proto
types are used or headers included in any public interface.

Here are the important files in the `src/ipc/` directory:

Public Interfaces
-----------------

* `interfaces.h` – `Node`, `Wallet`, `Handler` interface definitions

* `client.h` – `StartClient` function to spawn a `bitcoind` process and
                return a `Node` object controlling it through a socketpair.

* `server.h` – `StartServer` function to allow a `bitcoind` process to
                open a socket file descriptor and respond to remote commands.

Other Files
-----------

* `interfaces.cpp – In-process implementations of `Node`, `Wallet`, `Handler`
                    interfaces.

* `client.cpp – IPC implementations of `Node`, `Wallet`, `Handler`
                interfaces that forward calls to a socket.

* `server.cpp – IPC implementation responding to client requests.

* `messages.capnp – IPC interface definition.

* `serialize.{h,cpp}` – Helper functions for translating IPC messages.

* `util.{h,cpp}` – Helper functions for making and receiving IPC calls.
