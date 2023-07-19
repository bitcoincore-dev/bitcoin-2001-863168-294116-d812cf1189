#!/usr/bin/env bash
#
# Copyright (c) 2019-2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

export CI_IMAGE_NAME_TAG="debian:bookworm"
export CONTAINER_NAME=ci_native_valgrind
export PACKAGES="valgrind clang llvm libclang-rt-dev python3-zmq"
export USE_VALGRIND=1
export DEP_OPTS="DEBUG=1 CC='clang -gdwarf-4' CXX='clang++ -gdwarf-4'"  # Temporarily pin dwarf 4, until using Valgrind 3.20 or later
export TEST_RUNNER_EXTRA="--exclude feature_init,rpc_bind,feature_bind_extra"  # Excluded for now, see https://github.com/bitcoin/bitcoin/issues/17765#issuecomment-602068547
export GOAL="install"
export BITCOIN_CONFIG="--enable-zmq --with-gui=no CPPFLAGS='-DABORT_ON_FAILED_ASSUME -DBOOST_MULTI_INDEX_ENABLE_SAFE_MODE'"  # TODO enable GUI
