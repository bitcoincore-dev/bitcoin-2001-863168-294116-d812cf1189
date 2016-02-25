#!/usr/bin/env python2
# Copyright (c) 2015 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

#
# Test PreciousBlock code
#

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

class PreciousTest(BitcoinTestFramework):
    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 3)

    def setup_network(self):
        self.nodes = []
        self.is_network_split = False
        self.nodes.append(start_node(0, self.options.tmpdir, ["-debug"]))
        self.nodes.append(start_node(1, self.options.tmpdir, ["-debug"]))
        self.nodes.append(start_node(2, self.options.tmpdir, ["-debug"]))

    def run_test(self):
        print "Mine blocks A-B-C on Node 0"
        self.nodes[0].generate(3)
        assert(self.nodes[0].getblockcount() == 3)
        hashC = self.nodes[0].getbestblockhash()
        print "Mine competing blocks E-F-G on Node 1"
        self.nodes[1].generate(3)
        assert(self.nodes[1].getblockcount() == 3)
        hashG = self.nodes[1].getbestblockhash()
        assert(hashC != hashG)
        print "Connect nodes and check no reorg occurs"
        connect_nodes_bi(self.nodes,0,1)
        sync_blocks(self.nodes[0:2])
        assert(self.nodes[0].getbestblockhash() == hashC)
        assert(self.nodes[1].getbestblockhash() == hashG)
        print "Make Node0 prefer block G"
        self.nodes[0].preciousblock(hashG)
        assert(self.nodes[0].getbestblockhash() == hashG)
        print "Make Node0 prefer block C again"
        self.nodes[0].preciousblock(hashC)
        assert(self.nodes[0].getbestblockhash() == hashC)
        print "Make Node1 prefer block C"
        self.nodes[1].preciousblock(hashC)
        sync_chain(self.nodes[0:2]) # wait because node 1 may not have downloaded hashC
        assert(self.nodes[1].getbestblockhash() == hashC)
        print "Make Node1 prefer block G again"
        self.nodes[1].preciousblock(hashG)
        assert(self.nodes[1].getbestblockhash() == hashG)
        print "Make Node0 prefer block G again"
        self.nodes[0].preciousblock(hashG)
        assert(self.nodes[0].getbestblockhash() == hashG)
        print "Make Node1 prefer block C again"
        self.nodes[1].preciousblock(hashC)
        assert(self.nodes[1].getbestblockhash() == hashC)
        print "Mine another block (E-F-G-)H on Node 0 and reorg Node 1"
        self.nodes[0].generate(1)
        assert(self.nodes[0].getblockcount() == 4)
        sync_blocks(self.nodes[0:2])
        hashH = self.nodes[0].getbestblockhash()
        assert(self.nodes[1].getbestblockhash() == hashH)
        print "Node1 should not be able to prefer block C anymore"
        self.nodes[1].preciousblock(hashC)
        assert(self.nodes[1].getbestblockhash() == hashH)
        print "Mine competing blocks I-J-K-L on Node 2"
        self.nodes[2].generate(4)
        assert(self.nodes[2].getblockcount() == 4)
        hashL = self.nodes[2].getbestblockhash()
        print "Connect nodes and check no reorg occurs"
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,0,2)
        sync_blocks(self.nodes[0:3])
        assert(self.nodes[0].getbestblockhash() == hashH)
        assert(self.nodes[1].getbestblockhash() == hashH)
        assert(self.nodes[2].getbestblockhash() == hashL)
        print "Make Node1 prefer block L"
        self.nodes[1].preciousblock(hashL)
        assert(self.nodes[1].getbestblockhash() == hashL)
        print "Make Node2 prefer block H"
        self.nodes[2].preciousblock(hashH)
        assert(self.nodes[2].getbestblockhash() == hashH)

if __name__ == '__main__':
    PreciousTest().main()
