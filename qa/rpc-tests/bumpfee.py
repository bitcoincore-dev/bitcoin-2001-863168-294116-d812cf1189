#!/usr/bin/env python3
# Copyright (c) 2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *
import time

class BumpFeeTest (BitcoinTestFramework):

    def __init__(self):
        super().__init__()
        self.num_nodes = 2
        self.setup_clean_chain = True

    def setup_network(self, split=False):
        self.nodes = []
        self.nodes.append(start_node(0, self.options.tmpdir, ["-debug"]))
        self.nodes.append(start_node(1, self.options.tmpdir, ["-debug"]))
        connect_nodes_bi(self.nodes,0,1)
        self.is_network_split=False
        self.sync_all()

    # find the output that has amount=value
    def find_output(self, tx, value):
        outputs = tx['vout']
        for i in range(len(outputs)):
            if (outputs[i]['value'] == value):
                return outputs[i]['n']
        return -1

    def create_sign_send(self, node, inputs, outputs):
        rawtx = node.createrawtransaction(inputs, outputs)
        signedtx = node.signrawtransaction(rawtx)
        txid = node.sendrawtransaction(signedtx['hex'])
        return txid

    # sleep until a specified node has a certain tx
    def sync_mempool_tx(self, node, txid, timeout=10):
        while timeout > 0:
            if txid in node.getrawmempool():
                return True
            time.sleep(1)
            timeout-= 1
        return False

    def run_test (self):
        print("Mining blocks...")

        self.nodes[0].generate(5)
        self.sync_all()
        self.nodes[1].generate(2)
        self.sync_all()
        self.nodes[1].generate(100)
        self.sync_all()
        assert_equal(self.nodes[0].getbalance(), 250)
        assert_equal(self.nodes[1].getbalance(), 100)
        a0 = self.nodes[0].getnewaddress()
        a1 = self.nodes[0].getnewaddress()

        # create a send-to-self opt-in RBF transaction
        # this has a "payment" of 49, change of 0.999 and fee of 0.001
        coinbases = self.nodes[0].listunspent()
        assert_equal(len(coinbases), 5)
        utxo = coinbases.pop()
        inputs=[]
        inputs.append({"txid":utxo["txid"],"vout":utxo["vout"],"address":utxo["address"],"sequence":1000})
        outputs = {a0:49, a1:0.999}
        rbfid = self.create_sign_send(self.nodes[0], inputs, outputs)
        rbftx = self.nodes[0].gettransaction(rbfid)
        fulltx = self.nodes[0].getrawtransaction(rbfid, 1)

        # bump fee using the change output (will bump by relay fee)
        sync_mempools(self.nodes)
        assert(rbfid in self.nodes[0].getrawmempool())
        assert(rbfid in self.nodes[1].getrawmempool())
        change_output = self.find_output(fulltx, Decimal('0.999'))
        bumped_tx = self.nodes[0].bumpfee(rbfid, change_output)
        assert(bumped_tx['fee'] - abs(rbftx['fee']) > 0)

        # check that bumped_tx propogates and parent tx was evicted from both mempools
        sync_mempools(self.nodes)
        assert(bumped_tx['txid'] in self.nodes[0].getrawmempool())
        assert(bumped_tx['txid'] in self.nodes[1].getrawmempool())
        assert(rbfid not in self.nodes[0].getrawmempool())
        assert(rbfid not in self.nodes[1].getrawmempool())

        # check that the wallet tx for the original tx has a conflict
        oldwtx = self.nodes[0].gettransaction(rbfid)
        assert(len(oldwtx['walletconflicts']) > 0)

        # check that we cannot replace a non RBF transaction
        utxo = coinbases.pop()
        inputs=[]
        inputs.append({"txid":utxo["txid"],"vout":utxo["vout"],"address":utxo["address"]})
        outputs = {a0:49, a1:0.999}
        txid = self.create_sign_send(self.nodes[0], inputs, outputs)
        assert_raises(JSONRPCException, self.nodes[0].bumpfee, txid, 0)

        # check that we cannot bump fee with a too-small output (100 satoshis won't cover the delta)
        utxo = coinbases.pop()
        inputs=[]
        inputs.append({"txid":utxo["txid"],"vout":utxo["vout"],"address":utxo["address"],"sequence":1000})
        outputs = {a0:49.99, a1:0.00000100}
        rbfid = self.create_sign_send(self.nodes[0], inputs, outputs)
        fulltx = self.nodes[0].getrawtransaction(rbfid, 1)
        change_output = self.find_output(fulltx, Decimal('0.000001'))
        assert_raises(JSONRPCException, self.nodes[0].bumpfee, rbfid, change_output)

        # test a tx with descendants (bumped fee must cover all of the descendants fees)
        #   - create a send-to-self RBF tx with child with large fee
        #   - parent tx fee is .02 btc = 2,000,000 satoshis
        #   - child tx fee is .07 btc = 7,000,000 satoshis
        utxo = coinbases.pop()
        inputs = []
        inputs.append({"txid":utxo["txid"],"vout":utxo["vout"],"address":utxo["address"],"sequence":1000})
        outputs = {a0:24.99, a1:24.99}
        parent_id = self.create_sign_send(self.nodes[0], inputs, outputs)

        # child tx fee: 24.99 - 24.92 = .07 btc = 7,000,000 satoshis
        inputs = [ {'txid':parent_id, 'vout':0} ]
        outputs = {a0:24.92}
        child_id = self.create_sign_send(self.nodes[0], inputs, outputs)

        # try bumpfee on parent - this fails b/c cannot pay for child
        assert_raises(JSONRPCException, self.nodes[0].bumpfee, parent_id, 0)

        # try again, setting totalFee
        # total fee required = parent(2,000,000) + child(7,000,000) + the relay fee (should be about 190 satoshis)
        # note that totalFee must be < 10,000,000 satoshis unless you modify the node's maxTxFee
        # first attempt fails due to low fee, second attempt succeeds
        assert_raises(JSONRPCException, self.nodes[0].bumpfee, parent_id, 0, {"totalFee":9000180})
        bumped_tx = self.nodes[0].bumpfee(parent_id, 0, {"totalFee":9000200})

        # check propogation of bumped_tx, eviction of original parent
        sync_mempools(self.nodes)
        assert(bumped_tx['txid'] in self.nodes[0].getrawmempool())
        assert(bumped_tx['txid'] in self.nodes[1].getrawmempool())
        assert(parent_id not in self.nodes[0].getrawmempool())
        assert(parent_id not in self.nodes[1].getrawmempool())


        # check that calling bumpfee twice on the original txid throws an error (original tx now has a wallet
        # conflict with a later transaction, so the second bumpfee call must be on the bumped txid)
        # original tx: fee of .01
        # bumped tx: fee of .02
        # second bump: fee of .03
        utxo = coinbases.pop()
        inputs=[]
        inputs.append({"txid":utxo["txid"],"vout":utxo["vout"],"address":utxo["address"],"sequence":1000})
        outputs = {a0:49.99}
        rbfid = self.create_sign_send(self.nodes[0], inputs, outputs)
        #rbftx = self.nodes[0].gettransaction(rbfid)
        #fulltx = self.nodes[0].getrawtransaction(rbfid, 1)

        # bump fee to .02
        # we sleep 1 second before bumping so that the bumped wallet tx timestamp will be different,
        # as it would in any real-world scenario
        time.sleep(1)
        bumped_tx = self.nodes[0].bumpfee(rbfid, 0, {"totalFee":2000000})
        bumped_id = bumped_tx['txid']
        sync_mempools(self.nodes)
        assert(bumped_id in self.nodes[1].getrawmempool())

        # bumpfee to 0.3 fails on the original tx, but succeeds on bumped_id
        assert_raises(JSONRPCException, self.nodes[0].bumpfee, rbfid, 0, {"totalFee":3000000})
        succeed_tx = self.nodes[0].bumpfee(bumped_id, 0, {"totalFee":3000000})
        self.sync_all()


        # Test bumpfee on tx's that are not in the mempool
        #
        # First, test a bare tx (no children), this should work as advertised.
        #
        # Then test a tx with a high-fee child with various fees:
        #   1) too low to get into the local mempool (bumpfee fails)
        #   2) high enough to get into mempool, but does not pay for child so will not propogate to
        #   peers that still have the child_tx in their mempool
        #   3) high enough to pay for child.  To do this call bumpfee on the result in (2) with a
        #   higher fee, this will propogate to peers

        # create two inputs, one with 100,000 satoshis, one with 200,000 satoshis, both with low priority
        a0 = self.nodes[1].getnewaddress()
        a1 = self.nodes[1].getnewaddress()
        a2 = self.nodes[1].getnewaddress()
        coinbases = self.nodes[1].listunspent()
        assert_equal(len(coinbases), 2)
        utxo = coinbases.pop()
        inputs=[{"txid":utxo["txid"],"vout":utxo["vout"],"address":utxo["address"],"sequence":1000}]
        outputs = {a0:49.996, a1:0.001, a2:0.002}
        bigtx_id = self.create_sign_send(self.nodes[1], inputs, outputs)
        bigtx_full = self.nodes[1].getrawtransaction(bigtx_id, 1)
        change_1 = self.find_output(bigtx_full, Decimal('0.001'))
        change_2 = self.find_output(bigtx_full, Decimal('0.002'))
        self.nodes[1].generate(1)  # mine bigtx to clear it from the mempool
        assert(len(self.nodes[1].getrawmempool()) == 0)
        self.sync_all()

        # create "bare" (no children) rbf tx with 1,000 satoshi fee
        inputs=[{"txid":bigtx_id,"vout":change_1,"sequence":1000}]
        outputs = {a0:0.00099}
        bare_id = self.create_sign_send(self.nodes[1], inputs, outputs)
        sync_mempools(self.nodes)

        # create a parent rbf tx with low fee but child tx with high fee
        # parent tx:  200,000 satoshis in, 100,000 to child, 99,000 to change, 1,000 to fee
        inputs=[{"txid":bigtx_id,"vout":change_2,"sequence":1000}]
        outputs = {a0:0.001, a1:0.00099}
        parent_id = self.create_sign_send(self.nodes[1], inputs, outputs)
        parent_full = self.nodes[1].getrawtransaction(parent_id, 1)
        child_output = self.find_output(parent_full, Decimal('0.001'))
        change_output = self.find_output(parent_full, Decimal('0.00099'))
        sync_mempools(self.nodes)

        # child: 60,000 satoshis to self, 40,000 satoshi fee
        inputs=[{"txid":parent_id,"vout":child_output,"sequence":1000}]
        outputs = {a2:0.00060}
        child_id = self.create_sign_send(self.nodes[1], inputs, outputs)
        sync_mempools(self.nodes)

        # The easiest way to remove a tx from mempool is to stop the node and re-start with a
        # higher mempool min fee (so the wallet will not add it to the mempool). Note that the 
        # removed tx must not pass the AllowFree test (priority), so must use a low-value tx.
        #
        # Here, we restart node1 with min fee = 10,000 satoshis
        # No tx's should make it back into the mempool: bare tx and parent_tx are low-fee, 
        # and child_tx is an orphan
        #
        stop_node(self.nodes[1],1)
        self.nodes[1]=start_node(1, self.options.tmpdir, ["-debug", "-minrelaytxfee=0.0001"])
        connect_nodes_bi(self.nodes,0,1)
        assert_equal(0, len(self.nodes[1].getrawmempool()))
        assert_equal(3, len(self.nodes[0].getrawmempool()))

        # now call bumpfee
        # bumpfee on bare_tx using smartfees succeeds and propogates
        bumped_tx = self.nodes[1].bumpfee(bare_id, 0)
        assert(bumped_tx['txid'] in self.nodes[1].getrawmempool())
        self.sync_mempool_tx(self.nodes[0], bumped_tx['txid'])

        # bumpfee on parent with 2,000 satoshis fails, because parent_fee was 1,000 and the
        # current relay fee is about 1,900 satoshis
        assert_raises(JSONRPCException, self.nodes[1].bumpfee, parent_id, change_output, {"totalFee":2000})

        # bumpfee on parent with 20,000 satoshis will "succeed" in the sense that it gets into the local mempool,
        # but will be rejected by peers because it does not pay enough fee to cover the child
        # note that the bumpfee command will warn the user that if the tx has children, it may be rejected by peers
        # if the fee does not pay for the children
        #
        bumped_1 = self.nodes[1].bumpfee(parent_id, change_output, {"totalFee":20000})
        assert(bumped_1["txid"] in self.nodes[1].getrawmempool())
        result = self.sync_mempool_tx(self.nodes[0], bumped_1['txid'], 5)
        assert_equal(result, False)

        # here, the user figured out that the bumped tx didn't propogate, so he/she calls bumpfee again on the bumped_tx
        # this should succeed and propogate
        bumped_2 = self.nodes[1].bumpfee(bumped_1['txid'], 0, {"totalFee":50000})
        assert(bumped_1["txid"] not in self.nodes[1].getrawmempool())
        assert(bumped_2["txid"] in self.nodes[1].getrawmempool())
        result = self.sync_mempool_tx(self.nodes[0], bumped_2['txid'])
        assert_equal(result, True)

        print("Success")


if __name__ == '__main__':
    BumpFeeTest ().main ()
