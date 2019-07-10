# Multiprocess Bitcoin

On unix systems, the `--enable-multiprocess` build option can be passed to `./configure` to build new `bitcoin-node`, `bitcoin-wallet`, and `bitcoin-gui` executables alongside existing `bitcoind` and `bitcoin-qt` executables.

`bitcoin-node` is a drop-in replacement for `bitcoind`, and `bitcoin-gui` is a drop-in replacement for `bitcoin-qt`, and there are no differences in use or external behavior between the executables. But internally (after [#10102](https://github.com/bitcoin/bitcoin/pull/10102)), `bitcoin-gui` will spawn a `bitcoin-node` process to run P2P and RPC code, communicating with it across a socket pair, and `bitcoin-node` will spawn `bitcoin-wallet` to run wallet code, also communicating over a socket pair. This will let node, wallet, and GUI code run in separate address spaces for better isolation, and allow future improvements like being able to start and stop components independently on different machines and environments.

## Next steps

Specific next steps after [#10102](https://github.com/bitcoin/bitcoin/pull/10102) will be:

- [ ] Adding `-ipcbind` and `-ipcconnect` options to `bitcoin-node`, `bitcoin-wallet`, and `bitcoin-gui` executables so they can listen and connect to TCP ports and unix socket paths. This will allow separate processes to be started and stopped any time and connect to each other.
- [ ] Adding `-server` and `-rpcbind` options to the `bitcoin-wallet` executable so wallet processes can handle RPC requests directly without going through the node.
- [ ] Supporting windows, not just unix systems. The existing socket code is already cross-platform, so the only windows-specific code that needs to be written is code spawning a process and passing a socket descriptor. This can be implemented with `CreateProcess` and `WSADuplicateSocket`. Example: https://memset.wordpress.com/2010/10/13/win32-api-passing-socket-with-ipc-method/.
- [ ] Adding sandbox features, restricting subprocess access to resources and data. See [https://eklitzke.org/multiprocess-bitcoin](https://eklitzke.org/multiprocess-bitcoin).

## Debugging

After [#10102](https://github.com/bitcoin/bitcoin/pull/10102), the `-debug=ipc` command line option can be used to see requests and responses between processes.
