#!/usr/bin/env python3
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

#
# Test node handling
#
import time

from test_framework.mininode import wait_until
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (assert_equal,
                                 assert_raises_jsonrpc,
                                 connect_nodes_bi,
                                 start_node,
                                 stop_node,
                                 )

class NodeHandlingTest(BitcoinTestFramework):

    def __init__(self):
        super().__init__()
        self.num_nodes = 2
        self.setup_clean_chain = False

    def setup_network(self):
        self.nodes = self.setup_nodes()
        connect_nodes_bi(self.nodes, 0, 1)

    def run_test(self):
        ###########################
        # setban/listbanned tests #
        ###########################
        assert_equal(len(self.nodes[1].getpeerinfo()), 2)  # node1 should have 2 connections to node0 at this point
        self.nodes[1].setban("127.0.0.1", "add")
        assert wait_until(lambda: len(self.nodes[1].getpeerinfo()) == 0, timeout=10)
        assert_equal(len(self.nodes[1].getpeerinfo()), 0)  # all nodes must be disconnected at this point
        assert_equal(len(self.nodes[1].listbanned()), 1)
        self.nodes[1].clearbanned()
        assert_equal(len(self.nodes[1].listbanned()), 0)
        self.nodes[1].setban("127.0.0.0/24", "add")
        assert_equal(len(self.nodes[1].listbanned()), 1)
        # This will throw an exception because 127.0.0.1 is within range 127.0.0.0/24
        assert_raises_jsonrpc(-23, "IP/Subnet already banned", self.nodes[1].setban, "127.0.0.1", "add")
        # This will throw an exception because 127.0.0.1/42 is not a real subnet
        assert_raises_jsonrpc(-30, "Error: Invalid IP/Subnet", self.nodes[1].setban, "127.0.0.1/42", "add")
        assert_equal(len(self.nodes[1].listbanned()), 1)  # still only one banned ip because 127.0.0.1 is within the range of 127.0.0.0/24
        # This will throw an exception because 127.0.0.1 was not added above
        assert_raises_jsonrpc(-30, "Error: Unban failed", self.nodes[1].setban, "127.0.0.1", "remove")
        assert_equal(len(self.nodes[1].listbanned()), 1)
        self.nodes[1].setban("127.0.0.0/24", "remove")
        assert_equal(len(self.nodes[1].listbanned()), 0)
        self.nodes[1].clearbanned()
        assert_equal(len(self.nodes[1].listbanned()), 0)

        # test persisted banlist
        self.nodes[1].setban("127.0.0.0/32", "add")
        self.nodes[1].setban("127.0.0.0/24", "add")
        # Set the mocktime so we can control when bans expire
        old_time = int(time.time())
        self.nodes[1].setmocktime(old_time)
        self.nodes[1].setban("192.168.0.1", "add", 1)  # ban for 1 seconds
        self.nodes[1].setban("2001:4d48:ac57:400:cacf:e9ff:fe1d:9c63/19", "add", 1000)  # ban for 1000 seconds
        listBeforeShutdown = self.nodes[1].listbanned()
        assert_equal("192.168.0.1/32", listBeforeShutdown[2]['address'])
        # Move time forward by 3 seconds so the third ban has expired
        self.nodes[1].setmocktime(old_time + 3)
        assert_equal(len(self.nodes[1].listbanned()), 3)

        stop_node(self.nodes[1], 1)

        self.nodes[1] = start_node(1, self.options.tmpdir)
        listAfterShutdown = self.nodes[1].listbanned()
        assert_equal("127.0.0.0/24", listAfterShutdown[0]['address'])
        assert_equal("127.0.0.0/32", listAfterShutdown[1]['address'])
        assert_equal("/19" in listAfterShutdown[2]['address'], True)

        # Clear ban lists
        self.nodes[1].clearbanned()
        connect_nodes_bi(self.nodes, 0, 1)

        ###########################
        # RPC disconnectnode test #
        ###########################
        # fail to disconnect when calling with address and nodeid
        address1 = self.nodes[0].getpeerinfo()[0]['addr']
        node1 = self.nodes[0].getpeerinfo()[0]['addr']
        assert_raises_jsonrpc(-32602, "Only one of address and nodeid should be provided.", self.nodes[0].disconnectnode, address=address1, nodeid=node1)

        # fail to disconnect when calling with junk address
        assert_raises_jsonrpc(-29, "Node not found in connected nodes", self.nodes[0].disconnectnode, address="221B Baker Street")

        address1 = self.nodes[0].getpeerinfo()[0]['addr']
        self.nodes[0].disconnectnode(address=address1)
        assert wait_until(lambda: len(self.nodes[0].getpeerinfo()) == 1, timeout=10)
        assert not [node for node in self.nodes[0].getpeerinfo() if node['addr'] == address1]

        connect_nodes_bi(self.nodes, 0, 1)  # reconnect the node
        assert_equal(len(self.nodes[0].getpeerinfo()), 2)
        assert [node for node in self.nodes[0].getpeerinfo() if node['addr'] == address1]

        # successfully disconnect node by node id")
        id1 = self.nodes[0].getpeerinfo()[0]['id']
        self.nodes[0].disconnectnode(nodeid=id1)
        wait_until(lambda: len(self.nodes[0].getpeerinfo()) == 1)
        assert not [node for node in self.nodes[0].getpeerinfo() if node['id'] == id1]

if __name__ == '__main__':
    NodeHandlingTest().main()
