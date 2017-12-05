# Multiprocess Bitcoin

Bitcoin can be built with support for running node, wallet, and gui code in separate processes. To enable, pass with `--with-multiprocess` option to the configure script. This will build build new `bitcoin-node`, `bitcoin-wallet`, and `bitcoin-gui` executables with node, wallet, and gui functionality isolated into different processes.

FIXME
