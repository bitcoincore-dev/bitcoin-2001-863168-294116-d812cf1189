#!/usr/bin/env python3
# Copyright (c) 2020 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test bitcoind with 'onion'-tagged peers.

Tested inbound and outbound peers.
"""

import os

from test_framework.p2p import P2PInterface
from test_framework.socks5 import Socks5Configuration, Socks5Server
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    PORT_MIN,
    PORT_RANGE,
    assert_equal,
)


class OnionTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True

    def setup_network(self):
        self.add_nodes(self.num_nodes)
        self.start_nodes()

    def test_connections(self, in_clearnet=0, in_onion=0, out_onion=0):
        self.restart_node(0, ['-bind={}=onion'.format(self.bind_onion), self.arg_onion])
        self.restart_node(1)

        for i in range(in_clearnet):
            self.nodes[0].add_p2p_connection(P2PInterface())

        if in_onion > 0:
            if in_onion > 1:
                self.log.warning("Maximum 1 inbound 'onion'-tagged connection is supported.")
                in_onion = 1
            self.nodes[1].addnode(self.bind_onion, 'onetry')

        # Create a connection to a dummy Socks5 server just before
        # the getnetworkinfo RPC, as such connections are short-lived.
        if out_onion > 0:
            if out_onion > 1:
                self.log.warning("Maximum 1 outbound onion connection is supported.")
                out_onion = 1
            self.nodes[0].addnode('bitcoinostk4e4re.onion:18444', 'onetry')

        network_info = self.nodes[0].getnetworkinfo()
        assert_equal(network_info["connections"], in_clearnet + in_onion + out_onion)
        assert_equal(network_info["connections_in"], in_clearnet + in_onion)
        assert_equal(network_info["connections_out"], out_onion)
        assert_equal(network_info["connections_onion_only"], in_clearnet == 0 and (in_onion + out_onion) > 0)

    def run_test(self):
        port = PORT_MIN + 2 * PORT_RANGE + (os.getpid() % 1000)
        self.bind_onion = '127.0.0.1:{}'.format(port)

        proxy_conf = Socks5Configuration()
        port += 1
        proxy_conf.addr = ('127.0.0.1', port)
        proxy_conf.unauth = True
        Socks5Server(proxy_conf).start()
        self.arg_onion = '-onion={0[0]}:{0[1]}'.format(proxy_conf.addr)

        self.log.info("Checking a node without any peers...")
        self.test_connections(in_clearnet=0, in_onion=0, out_onion=0)

        self.log.info("Checking onion only connections...")
        self.test_connections(in_clearnet=0, in_onion=0, out_onion=1)
        self.test_connections(in_clearnet=0, in_onion=1, out_onion=0)
        self.test_connections(in_clearnet=0, in_onion=1, out_onion=1)

        self.log.info("Checking clearnet only connections...")
        self.test_connections(in_clearnet=1, in_onion=0, out_onion=0)
        self.test_connections(in_clearnet=2, in_onion=0, out_onion=0)

        self.log.info("Checking mixed type connections...")
        self.test_connections(in_clearnet=1, in_onion=0, out_onion=1)
        self.test_connections(in_clearnet=1, in_onion=1, out_onion=0)
        self.test_connections(in_clearnet=1, in_onion=1, out_onion=1)
        self.test_connections(in_clearnet=2, in_onion=0, out_onion=1)
        self.test_connections(in_clearnet=2, in_onion=1, out_onion=0)
        self.test_connections(in_clearnet=2, in_onion=1, out_onion=1)


if __name__ == '__main__':
    OnionTest().main()
