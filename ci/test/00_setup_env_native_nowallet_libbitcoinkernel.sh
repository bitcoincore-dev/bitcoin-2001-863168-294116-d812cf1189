#!/usr/bin/env bash
#
# Copyright (c) 2019-2021 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

export CONTAINER_NAME=ci_native_nowallet_libbitcoinkernel
export DOCKER_NAME_TAG=ubuntu:22.04
export PACKAGES="python3-zmq clang-9 llvm-9 libc++abi-9-dev libc++-9-dev cmake"
export DEP_OPTS="NO_WALLET=1 CC=clang-9 CXX='clang++-9 -stdlib=libc++'"
export GOAL="install"
export BITCOIN_CONFIG="--enable-reduce-exports CC=clang-8 CXX='clang++-8 -stdlib=libc++' --enable-experimental-util-chainstate --with-experimental-kernel-lib --enable-shared"
