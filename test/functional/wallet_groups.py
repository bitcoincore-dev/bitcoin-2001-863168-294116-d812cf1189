#!/usr/bin/env python3
# Copyright (c) 2018 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test wallet group functionality."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
)

def assert_approx(v, vexp, vspan=0.00001):
    if v < vexp - vspan:
        raise AssertionError("%s < [%s..%s]" % (str(v), str(vexp - vspan), str(vexp + vspan)))
    if v > vexp + vspan:
        raise AssertionError("%s > [%s..%s]" % (str(v), str(vexp - vspan), str(vexp + vspan)))

class WalletGroupTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 3
        self.extra_args = [[], [], ['-avoidpartialspends']]

    def run_test (self):
        tmpdir = self.options.tmpdir

        # Mine some coins
        self.nodes[0].generate(110)

        # Get some addresses from the two nodes
        addr1 = [self.nodes[1].getnewaddress() for i in range(3)]
        addr2 = [self.nodes[2].getnewaddress() for i in range(3)]
        addrs = addr1 + addr2

        # Send 1 + 0.5 coin to each address
        [self.nodes[0].sendtoaddress(addr, 1.0) for addr in addrs]
        [self.nodes[0].sendtoaddress(addr, 0.5) for addr in addrs]

        self.nodes[0].generate(1)
        self.sync_all()

        # For each node, send 0.2 coins back to 0;
        # - node[1] should pick one 0.5 UTXO and leave the rest
        # - node[2] should pick one (1.0 + 0.5) UTXO group corresponding to a
        #   given address, and leave the rest
        txid1 = self.nodes[1].sendtoaddress(self.nodes[0].getnewaddress(), 0.2)
        tx1 = self.nodes[1].getrawtransaction(txid1, True)
        # txid1 should have 1 input and 2 outputs
        assert_equal(1, len(tx1["vin"]))
        assert_equal(2, len(tx1["vout"]))
        # one output should be 0.2, the other should be ~0.3
        v = [vout["value"] for vout in tx1["vout"]]
        v.sort()
        assert_approx(v[0], 0.2)
        assert_approx(v[1], 0.3, 0.0001)

        txid2 = self.nodes[2].sendtoaddress(self.nodes[0].getnewaddress(), 0.2)
        tx2 = self.nodes[2].getrawtransaction(txid2, True)
        # txid2 should have 2 inputs and 2 outputs
        assert_equal(2, len(tx2["vin"]))
        assert_equal(2, len(tx2["vout"]))
        # one output should be 0.2, the other should be ~1.3
        v = [vout["value"] for vout in tx2["vout"]]
        v.sort()
        assert_approx(v[0], 0.2)
        assert_approx(v[1], 1.3, 0.0001)

        # Test 'avoid partial if warranted, even if disabled'
        self.sync_all()
        self.nodes[0].generate(1)
        # Nodes 1-2 now have confirmed UTXOs:
        # Node #1:      Node #2:
        # - A  1.0      - D0 1.0
        # - B0 1.0      - D1 0.5
        # - B1 0.5      - E0 1.0
        # - C0 1.0      - E1 0.5
        # - C1 0.5      - F  ~1.3
        # - D ~0.3
        utxos = self.nodes[1].listunspent()
        assert_approx(self.nodes[1].getbalance(), 4.3, 0.0001)
        assert_approx(self.nodes[2].getbalance(), 4.3, 0.0001)
        # Sending 1.1 btc should pick one 1.0 + one more. For node #1,
        # this could be (A / B0 / C0) + (B1 / C1 / D). We ensure that it is
        # B0 + B1 or C0 + C1, because this avoids partial spends while not being
        # detrimental to transaction cost
        txid3 = self.nodes[1].sendtoaddress(self.nodes[0].getnewaddress(), 1.1)
        tx3 = self.nodes[1].getrawtransaction(txid3, True)
        # tx3 should have 2 inputs and 2 outputs
        assert_equal(2, len(tx3["vin"]))
        assert_equal(2, len(tx3["vout"]))
        # the accumulated value should be 1.5, so the outputs should be
        # ~0.4 and 1.1 (TODO: this COULD be A and B1/C1 or B0/C1, etc, which
        # would be wrong; the destinations should be checked!)
        v = [vout["value"] for vout in tx3["vout"]]
        v.sort()
        assert_approx(v[0], 0.4, 0.0001)
        assert_approx(v[1], 1.1)
        # Node 2 enforces avoidpartialspends so needs no checking here

if __name__ == '__main__':
    WalletGroupTest().main ()
