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
                           {"address":addr, "label":"", "amount":Decimal("0.1"), "confirmations":10, "txids":[txid,]})
        #With min confidence < 10
        assert_array_result(self.nodes[1].listreceivedbyaddress(5),
                           {"address":addr},
                           {"address":addr, "label":"", "amount":Decimal("0.1"), "confirmations":10, "txids":[txid,]})
        #With min confidence > 10, should not find Tx
        assert_array_result(self.nodes[1].listreceivedbyaddress(11),{"address":addr},{ },True)

        #Empty Tx
        addr = self.nodes[1].getnewaddress()
        assert_array_result(self.nodes[1].listreceivedbyaddress(0,True),
                           {"address":addr},
                           {"address":addr, "label":"", "amount":0, "confirmations":0, "txids":[]})

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
            listreceivedbylabel + getreceivedbylabel Test
        '''
        #set pre-state
        addrArr = self.nodes[1].getnewaddress()
        label = self.nodes[1].getaccount(addrArr)
        received_by_label_json = get_sub_array_from_array(self.nodes[1].listreceivedbylabel(),{"label":label})
        if len(received_by_label_json) == 0:
            raise AssertionError("No labels found in node")
        balance_by_label = self.nodes[1].getreceivedbylabel(label)

        txid = self.nodes[0].sendtoaddress(addr, 0.1)
        self.sync_all()

        # listreceivedbylabel should return received_by_label_json because of 0 confirmations
        assert_array_result(self.nodes[1].listreceivedbylabel(),
                           {"label":label},
                           received_by_label_json)

        # getreceivedbyaddress should return same balance because of 0 confirmations
        balance = self.nodes[1].getreceivedbylabel(label)
        if balance != balance_by_label:
            raise AssertionError("Wrong balance returned by getreceivedbylabel, %0.2f"%(balance))

        self.nodes[1].generate(10)
        self.sync_all()
        # listreceivedbylabel should return updated label balance
        assert_array_result(self.nodes[1].listreceivedbylabel(),
                           {"label":label},
                           {"label":received_by_label_json["label"], "amount":(received_by_label_json["amount"] + Decimal("0.1"))})

        # getreceivedbyaddress should return updates balance
        balance = self.nodes[1].getreceivedbylabel(label)
        if balance != balance_by_label + Decimal("0.1"):
            raise AssertionError("Wrong balance returned by getreceivedbylabel, %0.2f"%(balance))

        #Create a new label named "mynewlabel" that has a 0 balance
        self.nodes[1].getlabeladdress("mynewlabel")
        received_by_label_json = get_sub_array_from_array(self.nodes[1].listreceivedbylabel(0,True),{"label":"mynewlabel"})
        if len(received_by_label_json) == 0:
            raise AssertionError("No labels found in node")

        # Test includeempty of listreceivedbylabel
        if received_by_label_json["amount"] != Decimal("0.0"):
            raise AssertionError("Wrong balance returned by listreceivedbylabel, %0.2f"%(received_by_label_json["amount"]))

        # Test getreceivedbylabel for 0 amount labels
        balance = self.nodes[1].getreceivedbylabel("mynewlabel")
        if balance != Decimal("0.0"):
            raise AssertionError("Wrong balance returned by getreceivedbylabel, %0.2f"%(balance))

if __name__ == '__main__':
    ReceivedByTest().main()
