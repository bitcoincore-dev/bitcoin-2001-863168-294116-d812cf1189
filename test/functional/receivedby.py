#!/usr/bin/env python3
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the listreceivedbyaddress RPC."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

def get_sub_array_from_array(object_array, to_match):
    '''
        Finds and returns a sub array from an array of arrays.
        to_match should be a unique idetifier of a sub array
    '''
    for item in object_array:
        all_match = True
        for key,value in to_match.items():
            if item[key] != value:
                all_match = False
        if not all_match:
            continue
        return item
    return []

class ReceivedByTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.enable_mocktime()

    def run_test(self):
        '''
        listreceivedbyaddress Test
        '''
        # Send from node 0 to 1
        addr = self.nodes[1].getnewaddress()
        txid = self.nodes[0].sendtoaddress(addr, 0.1)
        self.sync_all()

        #Check not listed in listreceivedbyaddress because has 0 confirmations
        assert_array_result(self.nodes[1].listreceivedbyaddress(),
                           {"address":addr},
                           { },
                           True)
        #Bury Tx under 10 block so it will be returned by listreceivedbyaddress
        self.nodes[1].generate(10)
        self.sync_all()
        assert_array_result(self.nodes[1].listreceivedbyaddress(),
                           {"address":addr},
                           {"address":addr, "account":"", "amount":Decimal("0.1"), "confirmations":10, "txids":[txid,]})
        #With min confidence < 10
        assert_array_result(self.nodes[1].listreceivedbyaddress(5),
                           {"address":addr},
                           {"address":addr, "account":"", "amount":Decimal("0.1"), "confirmations":10, "txids":[txid,]})
        #With min confidence > 10, should not find Tx
        assert_array_result(self.nodes[1].listreceivedbyaddress(11),{"address":addr},{ },True)

        #Empty Tx
        empty_addr = self.nodes[1].getnewaddress()
        assert_array_result(self.nodes[1].listreceivedbyaddress(0,True),
                           {"address":empty_addr},
                           {"address":empty_addr, "account":"", "amount":0, "confirmations":0, "txids":[]})

        #Test Address filtering
        #Only on addr
        expected = {"address":addr, "account":"", "amount":Decimal("0.1"), "confirmations":10, "txids":[txid,]}
        res = self.nodes[1].listreceivedbyaddress(minconf=0, include_empty=True, include_watchonly=True, only_address=addr)
        assert_array_result(res, {"address":addr}, expected)
        assert_equal(len(res), 1)
        #Another address receive money
        res = self.nodes[1].listreceivedbyaddress(0, True, True)
        assert_equal(len(res), 3) #Right now 3 entries
        other_addr = self.nodes[1].getnewaddress()
        txid2 = self.nodes[0].sendtoaddress(other_addr, 0.1)
        self.nodes[0].generate(1)
        self.sync_all()
        #Same test as above should still pass
        expected = {"address":addr, "account":"", "amount":Decimal("0.1"), "confirmations":11, "txids":[txid,]}
        res = self.nodes[1].listreceivedbyaddress(0, True, True, addr)
        assert_array_result(res, {"address":addr}, expected)
        assert_equal(len(res), 1)
        #Same test as above but with other_addr should still pass
        expected = {"address":other_addr, "account":"", "amount":Decimal("0.1"), "confirmations":1, "txids":[txid2,]}
        res = self.nodes[1].listreceivedbyaddress(0, True, True, other_addr)
        assert_array_result(res, {"address":other_addr}, expected)
        assert_equal(len(res), 1)
        #Should be two entries though without filter
        res = self.nodes[1].listreceivedbyaddress(0, True, True)
        assert_equal(len(res), 4) #Became 4 entries

        #Not on random addr
        other_addr = self.nodes[0].getnewaddress() # note on node[0]! just a random addr
        res = self.nodes[1].listreceivedbyaddress(0, True, True, other_addr)
        assert_equal(len(res), 0)

        '''
            getreceivedbyaddress Test
        '''
        # Send from node 0 to 1
        addr = self.nodes[1].getnewaddress()
        txid = self.nodes[0].sendtoaddress(addr, 0.1)
        self.sync_all()

        #Check balance is 0 because of 0 confirmations
        balance = self.nodes[1].getreceivedbyaddress(addr)
        if balance != Decimal("0.0"):
            raise AssertionError("Wrong balance returned by getreceivedbyaddress, %0.2f"%(balance))

        #Check balance is 0.1
        balance = self.nodes[1].getreceivedbyaddress(addr,0)
        if balance != Decimal("0.1"):
            raise AssertionError("Wrong balance returned by getreceivedbyaddress, %0.2f"%(balance))

        #Bury Tx under 10 block so it will be returned by the default getreceivedbyaddress
        self.nodes[1].generate(10)
        self.sync_all()
        balance = self.nodes[1].getreceivedbyaddress(addr)
        if balance != Decimal("0.1"):
            raise AssertionError("Wrong balance returned by getreceivedbyaddress, %0.2f"%(balance))

        '''
            listreceivedbyaccount + getreceivedbyaccount Test
        '''
        #set pre-state
        addrArr = self.nodes[1].getnewaddress()
        account = self.nodes[1].getaccount(addrArr)
        received_by_account_json = get_sub_array_from_array(self.nodes[1].listreceivedbyaccount(),{"account":account})
        if len(received_by_account_json) == 0:
            raise AssertionError("No accounts found in node")
        balance_by_account = self.nodes[1].getreceivedbyaccount(account)

        txid = self.nodes[0].sendtoaddress(addr, 0.1)
        self.sync_all()

        # listreceivedbyaccount should return received_by_account_json because of 0 confirmations
        assert_array_result(self.nodes[1].listreceivedbyaccount(),
                           {"account":account},
                           received_by_account_json)

        # getreceivedbyaddress should return same balance because of 0 confirmations
        balance = self.nodes[1].getreceivedbyaccount(account)
        if balance != balance_by_account:
            raise AssertionError("Wrong balance returned by getreceivedbyaccount, %0.2f"%(balance))

        self.nodes[1].generate(10)
        self.sync_all()
        # listreceivedbyaccount should return updated account balance
        assert_array_result(self.nodes[1].listreceivedbyaccount(),
                           {"account":account},
                           {"account":received_by_account_json["account"], "amount":(received_by_account_json["amount"] + Decimal("0.1"))})

        # getreceivedbyaddress should return updates balance
        balance = self.nodes[1].getreceivedbyaccount(account)
        if balance != balance_by_account + Decimal("0.1"):
            raise AssertionError("Wrong balance returned by getreceivedbyaccount, %0.2f"%(balance))

        #Create a new account named "mynewaccount" that has a 0 balance
        self.nodes[1].getaccountaddress("mynewaccount")
        received_by_account_json = get_sub_array_from_array(self.nodes[1].listreceivedbyaccount(0,True),{"account":"mynewaccount"})
        if len(received_by_account_json) == 0:
            raise AssertionError("No accounts found in node")

        # Test includeempty of listreceivedbyaccount
        if received_by_account_json["amount"] != Decimal("0.0"):
            raise AssertionError("Wrong balance returned by listreceivedbyaccount, %0.2f"%(received_by_account_json["amount"]))

        # Test getreceivedbyaccount for 0 amount accounts
        balance = self.nodes[1].getreceivedbyaccount("mynewaccount")
        if balance != Decimal("0.0"):
            raise AssertionError("Wrong balance returned by getreceivedbyaccount, %0.2f"%(balance))

if __name__ == '__main__':
    ReceivedByTest().main()
