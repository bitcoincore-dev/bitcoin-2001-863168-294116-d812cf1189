#!/usr/bin/env python3
# Copyright (c) 2020 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test bitcoind with 'onion'-tagged peers.

Tested outbound and inbound peers.
"""

import os

from test_framework.p2p import P2PInterface
from test_framework.socks5 import Socks5Configuration, Socks5Server
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    PORT_MIN,
    PORT_RANGE,
    assert_equal,
    wait_until_helper,
)


class OnionTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True

    def setup_network(self):
        self.proxy_conf = Socks5Configuration()
        self.proxy_conf.addr = ('127.0.0.1', PORT_MIN + 2 * PORT_RANGE + (os.getpid() % 1000))
        self.proxy_conf.unauth = True

        self.proxy = Socks5Server(self.proxy_conf)
        self.proxy.start()

        args = [
            ['-bind=127.0.0.2:18445=onion', '-onion=%s:%i' % (self.proxy_conf.addr)],
            [],
            ]
        self.add_nodes(self.num_nodes, extra_args=args)
        self.start_nodes()

    def run_test(self):
        self.log.info("Checking a node without any peers...")
        network_info = self.nodes[0].getnetworkinfo()
        assert_equal(network_info["connections"], 0)
        assert_equal(network_info["connections_in"], 0)
        assert_equal(network_info["connections_out"], 0)
        assert_equal(network_info["connections_onion_only"], False)

        self.log.info("Accepting an inbound 'onion'-tagged connection, and checking...")
        self.nodes[1].addnode("127.0.0.2:18445", "onetry")
        wait_until_helper(lambda: all(peer['version'] != 0 for peer in self.nodes[1].getpeerinfo()))
        wait_until_helper(lambda: all(peer['bytesrecv_per_msg'].pop('verack', 0) == 24 for peer in self.nodes[1].getpeerinfo()))
        network_info = self.nodes[0].getnetworkinfo()
        assert_equal(network_info["connections"], 1)
        assert_equal(network_info["connections_in"], 1)
        assert_equal(network_info["connections_out"], 0)
        assert_equal(network_info["connections_onion_only"], True)

        self.log.info("Creating an outbound onion connection, and checking...")
        self.nodes[0].addnode("bitcoinostk4e4re.onion:8333", "onetry")
        network_info = self.nodes[0].getnetworkinfo()
        assert_equal(network_info["connections"], 2)
        assert_equal(network_info["connections_in"], 1)
        assert_equal(network_info["connections_out"], 1)
        assert_equal(network_info["connections_onion_only"], True)

        self.log.info("Adding an outbound non-onion peer, and checking...")
        self.nodes[0].add_p2p_connection(P2PInterface())
        network_info = self.nodes[0].getnetworkinfo()
        assert_equal(network_info["connections"], 2)
        assert_equal(network_info["connections_in"], 2)
        assert_equal(network_info["connections_out"], 0)
        assert_equal(network_info["connections_onion_only"], False)


if __name__ == '__main__':
    OnionTest().main()
