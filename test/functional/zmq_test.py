#!/usr/bin/env python3
# Copyright (c) 2015-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the ZMQ notification interface."""
import collections
import configparser
import os
import struct

from test_framework.test_framework import BitcoinTestFramework, SkipTest
from test_framework.util import (assert_equal,
                                 bytes_to_hex_str,
                                 hash256,
                                )

class ZMQTest (BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2

    def setup_nodes(self):
        # Try to import python3-zmq. Skip this test if the import fails.
        try:
            import zmq
        except ImportError:
            raise SkipTest("python3-zmq module not available.")

        # Check that bitcoin has been built with ZMQ enabled.
        config = configparser.ConfigParser()
        if not self.options.configfile:
            self.options.configfile = os.path.abspath(os.path.join(os.path.dirname(__file__), "../config.ini"))
        config.read_file(open(self.options.configfile))

        if not config["components"].getboolean("ENABLE_ZMQ"):
            raise SkipTest("bitcoind has not been built with zmq enabled.")

        # Initialize ZMQ context and socket.
        address = "tcp://127.0.0.1:28332"
        self.zmq_context = zmq.Context()
        self.socket = self.zmq_context.socket(zmq.SUB)
        self.socket.set(zmq.RCVTIMEO, 60000)
        self.socket.connect(address)

        # Subscribe to all available topics.
        self.topics = [b"hashblock", b"hashtx", b"rawblock", b"rawtx"]
        self.topic_counts = collections.defaultdict(int)
        for topic in self.topics:
            self.socket.setsockopt(zmq.SUBSCRIBE, topic)

        self.extra_args = [["-zmqpub{}={}".format(topic.decode("ascii"), address) for topic in self.topics], []]
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def run_test(self):
        try:
            self._zmq_test()
        finally:
            # Destroy the ZMQ context.
            self.log.debug("Destroying ZMQ context")
            self.zmq_context.destroy(linger=None)

    def _zmq_test(self):
        num_blocks = 5
        self.log.info("Generate %(n)d blocks (and %(n)d coinbase txes)" % {"n": num_blocks})
        genhashes = self.nodes[0].generate(num_blocks)
        self.sync_all()

        messages = self._receive(num_blocks * len(self.topics))
        hashblock, hashtx, rawblock, rawtx = (messages[topic] for topic in self.topics)

        for x in range(num_blocks):
            # Should receive the coinbase txid.
            txid = hashtx[x]

            # Should receive the coinbase raw transaction.
            hex = rawtx[x]
            assert_equal(hash256(hex), txid)

            # Should receive the generated block hash.
            hash = bytes_to_hex_str(hashblock[x])
            assert_equal(genhashes[x], hash)
            # The block should only have the coinbase txid.
            assert_equal([bytes_to_hex_str(txid)], self.nodes[1].getblock(hash)["tx"])

            # Should receive the generated raw block.
            block = rawblock[x]
            assert_equal(genhashes[x], bytes_to_hex_str(hash256(block[:80])))

        self.log.info("Wait for tx from second node")
        payment_txid = self.nodes[1].sendtoaddress(self.nodes[0].getnewaddress(), 1.0)
        self.sync_all()
        messages = self._receive(2)

        # Should receive the broadcasted txid.
        txid, = messages[b"hashtx"]
        assert_equal(payment_txid, bytes_to_hex_str(txid))

        # Should receive the broadcasted raw transaction.
        hex, = messages[b"rawtx"]
        assert_equal(payment_txid, bytes_to_hex_str(hash256(hex)))

    def _receive(self, n=1):
        """Receive n messages and return lists of messages by topic."""
        result = collections.defaultdict(list)
        for _ in range(n):
            topic, body, seq = self.socket.recv_multipart()
            assert_equal((self.topic_counts[topic],), struct.unpack('<I', seq))
            self.topic_counts[topic] += 1
            result[topic].append(body)
        return result

if __name__ == '__main__':
    ZMQTest().main()
