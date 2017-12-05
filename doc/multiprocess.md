# Multiprocess Bitcoin

On unix platforms, bitcoin can be built with support for running node, wallet, and gui code in separate processes. To enable it, pass a `--with-multiprocess` option to the configure script. This will build new `bitcoin-node`, `bitcoin-wallet`, and `bitcoin-gui` executables that can invoke and communicate with each other over domain sockets. Existing `bitcoind` and `bitcoin-qt` executables are unaffected by the `--with-multiprocess` options.

Currently:

- The `bitcoin-node` executable functions as a drop-in replacement for `bitcoind` providing the same end-user functionality, while invoking a `bitcoin-wallet` process internally to manage the wallet.
- The `bitcoin-gui` executable functions as a drop-in replacement for `bitcoin-qt`, and invokes a `bitcoin-node` process internally.
- The `bitcoin-wallet` executable is only used by the other executables and doesn't do anything if it is run standalone.

Next steps:

- [ ] Adding `-ipcbind` and `-ipcconnect` options to all three executables so they can listen/connect to TCP ports and unix socket paths instead of just unnamed socket descriptors. This will allow processes to be started and stopped independently of each other.
- [ ] Support `-server`, `-rpcbind` etc options on `bitcoin-wallet` executable so wallet processes handle RPC requests directly without going through the node.
- [ ] Support windows subprocesses and pipes.
- [ ] Make build system / code organization improvements so different executables don't link in code they don't need, or accept command line options they would ignore.