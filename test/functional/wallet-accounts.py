#!/usr/bin/env python3
# Copyright (c) 2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test account RPCs.

RPCs tested are:
    - getaccountaddress
    - getaddressesbyaccount
    - listaddressgroupings
    - setaccount
    - sendfrom (with account arguments)
    - move (with account arguments)
"""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_raises_jsonrpc

class WalletAccountsTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.extra_args = [["-maxorphantx=1000", "-whitelist=127.0.0.1"]]

    def test_sort_multisig(self, node):
        node.importprivkey("cSJUMwramrFYHKPfY77FH94bv4Q5rwUCyfD6zX3kLro4ZcWsXFEM")
        node.importprivkey("cSpQbSsdKRmxaSWJ3TckCFTrksXNPbh8tfeZESGNQekkVxMbQ77H")
        node.importprivkey("cRNbfcJgnvk2QJEVbMsxzoprotm1cy3kVA2HoyjSs3ss5NY5mQqr")

        addresses = [
            "muRmfCwue81ZT9oc3NaepefPscUHtP5kyC",
            "n12RzKwqWPPA4cWGzkiebiM7Gu6NXUnDW8",
            "n2yWMtx8jVbo8wv9BK2eN1LdbaakgKL3Mt",
        ]

        sorted_default = node.addmultisigaddress(2, addresses)
        sorted_false = node.addmultisigaddress(2, addresses, {"sort": False})
        sorted_true = node.addmultisigaddress(2, addresses, {"sort": True})

        assert_equal(sorted_default, sorted_false)
        assert_equal("2N6dne8yzh13wsRJxCcMgCYNeN9fxKWNHt8", sorted_default)
        assert_equal("2MsJ2YhGewgDPGEQk4vahGs4wRikJXpRRtU", sorted_true)

    def test_sort_multisig_with_uncompressed_hash160(self, node):
        node.importpubkey("02632b12f4ac5b1d1b72b2a3b508c19172de44f6f46bcee50ba33f3f9291e47ed0")
        node.importpubkey("04dd4fe618a8ad14732f8172fe7c9c5e76dd18c2cc501ef7f86e0f4e285ca8b8b32d93df2f4323ebb02640fa6b975b2e63ab3c9d6979bc291193841332442cc6ad")
        address = "2MxvEpFdXeEDbnz8MbRwS23kDZC8tzQ9NjK"

        addresses = [
            "msDoRfEfZQFaQNfAEWyqf69H99yntZoBbG",
            "myrfasv56W7579LpepuRy7KFhVhaWsJYS8",
        ]
        default = self.nodes[0].addmultisigaddress(2, addresses)
        assert_equal(address, default)

        unsorted = self.nodes[0].addmultisigaddress(2, addresses, {"sort": False})
        assert_equal(address, unsorted)

        assert_raises_jsonrpc(-1, "Compressed key required for BIP67: myrfasv56W7579LpepuRy7KFhVhaWsJYS8", node.addmultisigaddress, 2, addresses, {"sort": True})

    def run_test (self):
        node = self.nodes[0]
        # Check that there's no UTXO on any of the nodes
        assert_equal(len(node.listunspent()), 0)

        # Note each time we call generate, all generated coins go into
        # the same address, so we call twice to get two addresses w/50 each
        node.generate(1)
        node.generate(101)
        assert_equal(node.getbalance(), 100)

        # there should be 2 address groups
        # each with 1 address with a balance of 50 Bitcoins
        address_groups = node.listaddressgroupings()
        assert_equal(len(address_groups), 2)
        # the addresses aren't linked now, but will be after we send to the
        # common address
        linked_addresses = set()
        for address_group in address_groups:
            assert_equal(len(address_group), 1)
            assert_equal(len(address_group[0]), 2)
            assert_equal(address_group[0][1], 50)
            linked_addresses.add(address_group[0][0])

        # send 50 from each address to a third address not in this wallet
        # There's some fee that will come back to us when the miner reward
        # matures.
        common_address = "msf4WtN1YQKXvNtvdFYt9JBnUD2FB41kjr"
        txid = node.sendmany(
            fromaccount="",
            amounts={common_address: 100},
            subtractfeefrom=[common_address],
            minconf=1,
        )
        tx_details = node.gettransaction(txid)
        fee = -tx_details['details'][0]['fee']
        # there should be 1 address group, with the previously
        # unlinked addresses now linked (they both have 0 balance)
        address_groups = node.listaddressgroupings()
        assert_equal(len(address_groups), 1)
        assert_equal(len(address_groups[0]), 2)
        assert_equal(set([a[0] for a in address_groups[0]]), linked_addresses)
        assert_equal([a[1] for a in address_groups[0]], [0, 0])

        node.generate(1)

        # we want to reset so that the "" account has what's expected.
        # otherwise we're off by exactly the fee amount as that's mined
        # and matures in the next 100 blocks
        node.sendfrom("", common_address, fee)
        accounts = ["a", "b", "c", "d", "e"]
        amount_to_send = 1.0
        account_addresses = dict()
        for account in accounts:
            address = node.getaccountaddress(account)
            account_addresses[account] = address
            
            node.getnewaddress(account)
            assert_equal(node.getaccount(address), account)
            assert(address in node.getaddressesbyaccount(account))
            
            node.sendfrom("", address, amount_to_send)
        
        node.generate(1)
        
        for i in range(len(accounts)):
            from_account = accounts[i]
            to_account = accounts[(i+1) % len(accounts)]
            to_address = account_addresses[to_account]
            node.sendfrom(from_account, to_address, amount_to_send)
        
        node.generate(1)
        
        for account in accounts:
            address = node.getaccountaddress(account)
            assert(address != account_addresses[account])
            assert_equal(node.getreceivedbyaccount(account), 2)
            node.move(account, "", node.getbalance(account))

        node.generate(101)
        
        expected_account_balances = {"": 5200}
        for account in accounts:
            expected_account_balances[account] = 0
        
        assert_equal(node.listaccounts(), expected_account_balances)
        
        assert_equal(node.getbalance(""), 5200)
        
        for account in accounts:
            address = node.getaccountaddress("")
            node.setaccount(address, account)
            assert(address in node.getaddressesbyaccount(account))
            assert(address not in node.getaddressesbyaccount(""))
        
        for account in accounts:
            addresses = []
            for x in range(10):
                addresses.append(node.getnewaddress())
            multisig_address = node.addmultisigaddress(5, addresses, account)
            node.sendfrom("", multisig_address, 50)
        
        node.generate(101)
        
        for account in accounts:
            assert_equal(node.getbalance(account), 50)
        self.test_sort_multisig(node)
        self.test_sort_multisig_with_uncompressed_hash160(node)

if __name__ == '__main__':
    WalletAccountsTest().main()
