#!/bin/bash

set -x
set -e

if false; then
cf="-O0 -ggdb"
lf=""
#cf="$cf -fsanitize=address -fno-omit-frame-pointer"
#lf="$lf -fsanitize=address"
make clean
./configure CXX=clang++ CXXFLAGS="$cf" LDFLAGS="$lf" --with-capnp=$HOME/src/capnp/c++/prefix --enable-werror
fi
OPT=()
OPT+=(-ex "dir /home/russ/Downloads/tmp/db5.3-5.3.28/debian")
#OPT+=(-ex "b fnop")
#OPT+=(-ex "b interfaces::capnp::LoggingErrorHandler::taskFailed")
#OPT+=(-ex "b interfaces::capnp::BreakPoint")
#OPT+=(-ex "b capnp::(anonymous namespace)::newNullCap()")
#OPT+=(-ex "set follow-fork-mode child")
OPT+=(-ex "catch throw")
kill -9 $(pgrep -f bitcoin-node) $(pgrep -f bitcoin-wallet) $(pgrep -f bitcoin-gui) $(pgrep -f gdb) || true
stty sane
reset
time make -j12 -C src bitcoin-node bitcoin-gui bitcoin-wallet qt/bitcoin-qt bitcoin-cli

#export ASAN_OPTIONS=abort_on_error=1
#export STOP="wallet"
#export STOP=wallet

sudo sysctl kernel.core_pattern=core
ulimit -c

#src/bitcoin-node -regtest -printtoconsole -debug
#src/bitcoin-gui -regtest -printtoconsole -debug

echo src/bitcoin-gui -regtest -printtoconsole -debug -ipcconnect=unix
#src/bitcoin-node -regtest -printtoconsole -debug -ipcbind=unix

#gdb "${OPT[@]}" -ex run --args src/bitcoin-node -regtest -printtoconsole -debug
#gdb "${OPT[@]}" -ex run --args src/bitcoin-gui -regtest -printtoconsole -debug
#gdb "${OPT[@]}" -ex run --args src/qt/bitcoin-qt -regtest -printtoconsole -debug

#BITCOIND=/home/russ/cc/bitcoin-gdb-screen test/functional/wallet_hd.py
#BITCOIND=/home/russ/cc/bitcoin-gdb-screen test/functional/wallet_labels.py
#BITCOIND=/home/russ/cc/bitcoin-gdb-screen test/functional/wallet_keypool.py
#BITCOIND=/home/russ/cc/bitcoin-gdb-screen test/functional/wallet_listtransactions.py --nocleanup
#BITCOIND=/home/russ/cc/bitcoin-gdb-screen test/functional/mempool_limit.py
#BITCOIND=$PWD/src/bitcoin-node strace -o strace -ff test/functional/mempool_limit.py
#BITCOIND=$PWD/src/bitcoin-node test/functional/wallet_labels.py --nocleanup
#BITCOIND=$PWD/src/bitcoin-node test/functional/interface_bitcoin_cli.py --nocleanup
#BITCOIND=$PWD/src/bitcoin-node test/functional/rpc_rawtransaction.py --nocleanup
#BITCOIND=$PWD/src/bitcoin-node test/functional/rpc_signrawtransaction.py --nocleanup
BITCOIND=$PWD/src/bitcoin-node test/functional/test_runner.py
#rm -rf test/cache && BITCOIND=/home/russ/cc/bitcoin-gdb-screen test/functional/create_cache.py --cachedir=test/cache --configfile=test/config.ini

#test/functional/combine_logs.py $(ls -1trd $TMPDIR/test* | tail -n1) -c | less -r

