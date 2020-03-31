#!/usr/bin/env python3
# Copyright (c) 2020 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test mempool descendants/ancestors information update.

Test mempool update of transaction descendants/ancestors information (count, size)
when transactions have been re-added from a disconnected block to the mempool.
"""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


class MempoolUpdateFromBlockTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        # Create N txs: tx0..tx{N-1}, such as txK depends on tx0..tx{K-1} directly.
        # If all of these txs reside in the mempool, the following holds:
        # the txK has K+1 ancestors (including this one), and N-K descendants (including this one).
        # Let N=5.
        inputs0 = []
        inputs0.append({'txid': self.nodes[0].getblock(self.nodes[0].getblockhash(1))['tx'][0], 'vout': 0})
        addresses0 = [self.nodes[0].getnewaddress() for n in range(0, 4)]
        outputs0 = {addresses0[0]: 12.495, addresses0[1]: 12.495, addresses0[2]: 12.495, addresses0[3]: 12.495}
        tx0_raw = self.nodes[0].signrawtransactionwithwallet(self.nodes[0].createrawtransaction(inputs0, outputs0))
        tx0_id = self.nodes[0].sendrawtransaction(tx0_raw['hex'])
        tx0_size = self.nodes[0].getrawmempool(True)[tx0_id]['vsize']

        inputs1 = []
        inputs1.append({'txid': tx0_id, 'vout': 0})
        addresses1 = [self.nodes[0].getnewaddress() for n in range(0, 3)]
        outputs1 = {addresses1[0]: 4.16, addresses1[1]: 4.16, addresses0[2]: 4.16}
        tx1_raw = self.nodes[0].signrawtransactionwithwallet(self.nodes[0].createrawtransaction(inputs1, outputs1))
        tx1_id = self.nodes[0].sendrawtransaction(tx1_raw['hex'])
        tx1_size = self.nodes[0].getrawmempool(True)[tx1_id]['vsize']

        inputs2 = []
        inputs2.append({'txid': tx0_id, 'vout': 1})
        inputs2.append({'txid': tx1_id, 'vout': 0})
        addresses2 = [self.nodes[0].getnewaddress() for n in range(0, 2)]
        outputs2 = {addresses2[0]: 8.327, addresses2[1]: 8.327}
        tx2_raw = self.nodes[0].signrawtransactionwithwallet(self.nodes[0].createrawtransaction(inputs2, outputs2))
        tx2_id = self.nodes[0].sendrawtransaction(tx2_raw['hex'])
        tx2_size = self.nodes[0].getrawmempool(True)[tx2_id]['vsize']

        self.log.info('The first batch of {} txs has been accepted into the mempool.'.format(len(self.nodes[0].getrawmempool())))

        self.nodes[0].generate(1)
        assert_equal(len(self.nodes[0].getrawmempool()), 0)
        self.log.info('All of the txs from the first batch have been mined into a block.')

        inputs3 = []
        inputs3.append({'txid': tx0_id, 'vout': 2})
        inputs3.append({'txid': tx1_id, 'vout': 1})
        inputs3.append({'txid': tx2_id, 'vout': 0})
        addresses3 = [self.nodes[0].getnewaddress() for n in range(0, 1)]
        outputs3 = {addresses3[0]: 24.98}
        tx3_raw = self.nodes[0].signrawtransactionwithwallet(self.nodes[0].createrawtransaction(inputs3, outputs3))
        tx3_id = self.nodes[0].sendrawtransaction(tx3_raw['hex'])
        tx3_size = self.nodes[0].getrawmempool(True)[tx3_id]['vsize']

        inputs4 = []
        inputs4.append({'txid': tx0_id, 'vout': 3})
        inputs4.append({'txid': tx1_id, 'vout': 2})
        inputs4.append({'txid': tx2_id, 'vout': 1})
        inputs4.append({'txid': tx3_id, 'vout': 0})
        addresses4 = [self.nodes[0].getnewaddress() for n in range(0, 1)]
        outputs4 = {addresses4[0]: 49.96}
        tx4_raw = self.nodes[0].signrawtransactionwithwallet(self.nodes[0].createrawtransaction(inputs4, outputs4))
        tx4_id = self.nodes[0].sendrawtransaction(tx4_raw['hex'])
        tx4_size = self.nodes[0].getrawmempool(True)[tx4_id]['vsize']

        self.log.info('The second batch of {} txs has been accepted into the mempool.'.format(len(self.nodes[0].getrawmempool())))

        self.nodes[0].invalidateblock(self.nodes[0].getbestblockhash())
        self.log.info('The latest block has been invalidated, and the mempool contains txs from all of the batches now.')

        assert_equal(self.nodes[0].getrawmempool(True)[tx0_id]['descendantcount'], 5)
        assert_equal(self.nodes[0].getrawmempool(True)[tx1_id]['descendantcount'], 4)
        assert_equal(self.nodes[0].getrawmempool(True)[tx2_id]['descendantcount'], 3)
        assert_equal(self.nodes[0].getrawmempool(True)[tx3_id]['descendantcount'], 2)
        assert_equal(self.nodes[0].getrawmempool(True)[tx4_id]['descendantcount'], 1)

        assert_equal(self.nodes[0].getrawmempool(True)[tx0_id]['descendantsize'], tx4_size + tx3_size + tx2_size + tx1_size + tx0_size)
        assert_equal(self.nodes[0].getrawmempool(True)[tx1_id]['descendantsize'], tx4_size + tx3_size + tx2_size + tx1_size)
        assert_equal(self.nodes[0].getrawmempool(True)[tx2_id]['descendantsize'], tx4_size + tx3_size + tx2_size)
        assert_equal(self.nodes[0].getrawmempool(True)[tx3_id]['descendantsize'], tx4_size + tx3_size)
        assert_equal(self.nodes[0].getrawmempool(True)[tx4_id]['descendantsize'], tx4_size)

        assert_equal(self.nodes[0].getrawmempool(True)[tx0_id]['ancestorcount'], 1)
        assert_equal(self.nodes[0].getrawmempool(True)[tx1_id]['ancestorcount'], 2)
        assert_equal(self.nodes[0].getrawmempool(True)[tx2_id]['ancestorcount'], 3)
        assert_equal(self.nodes[0].getrawmempool(True)[tx3_id]['ancestorcount'], 4)
        assert_equal(self.nodes[0].getrawmempool(True)[tx4_id]['ancestorcount'], 5)

        assert_equal(self.nodes[0].getrawmempool(True)[tx0_id]['ancestorsize'], tx0_size)
        assert_equal(self.nodes[0].getrawmempool(True)[tx1_id]['ancestorsize'], tx0_size + tx1_size)
        assert_equal(self.nodes[0].getrawmempool(True)[tx2_id]['ancestorsize'], tx0_size + tx1_size + tx2_size)
        assert_equal(self.nodes[0].getrawmempool(True)[tx3_id]['ancestorsize'], tx0_size + tx1_size + tx2_size + tx3_size)
        assert_equal(self.nodes[0].getrawmempool(True)[tx4_id]['ancestorsize'], tx0_size + tx1_size + tx2_size + tx3_size + tx4_size)


if __name__ == '__main__':
    MempoolUpdateFromBlockTest().main()
