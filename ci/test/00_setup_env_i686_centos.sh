#!/usr/bin/env bash
#
# Copyright (c) 2020 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

export HOST=i686-pc-linux-gnu
export CONTAINER_NAME=ci_i686_centos_7
export DOCKER_NAME_TAG=centos:7
export GOAL="install"
export BITCOIN_CONFIG="--enable-zmq --with-gui=qt5 --enable-reduce-exports"
export CONFIG_SHELL="/bin/dash"
