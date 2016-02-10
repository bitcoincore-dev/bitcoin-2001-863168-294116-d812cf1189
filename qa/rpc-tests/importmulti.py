#!/usr/bin/env python3
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

class ImportMultiTest (BitcoinTestFramework):
    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 4)

    def setup_network(self, split=False):
        self.nodes = start_nodes(2, self.options.tmpdir)
        connect_nodes_bi(self.nodes,0,1)
        self.is_network_split=False
        self.sync_all()

    def run_test (self):
        import time
        begintime = int(time.time())

        print("Mining blocks...")
        self.nodes[0].generate(1)

        # sync
        self.sync_all()

        # keyword definition
        PRIV_KEY = 'privkey'
        PUB_KEY = 'pubkey'
        ADDRESS_KEY = 'address'
        SCRIPT_KEY = 'script'

        # address
        address1 = self.nodes[0].getnewaddress()
        # pubkey
        address2 = self.nodes[0].getnewaddress()
        address2_pubkey = self.nodes[0].validateaddress(address2)['pubkey']                 # Using pubkey
        # privkey
        address3 = self.nodes[0].getnewaddress()
        address3_privkey = self.nodes[0].dumpprivkey(address3)                              # Using privkey
        # scriptPubKey
        address4 = self.nodes[0].getnewaddress()
        address4_scriptpubkey = self.nodes[0].validateaddress(address4)['scriptPubKey']     # Using scriptpubkey


        #Check only one address
        address_info = self.nodes[0].validateaddress(address1)
        assert_equal(address_info['ismine'], True)

        self.sync_all()

        #Node 1 sync test
        assert_equal(self.nodes[1].getblockcount(),1)

        #Address Test - before import
        address_info = self.nodes[1].validateaddress(address1)
        assert_equal(address_info['iswatchonly'], False)
        assert_equal(address_info['ismine'], False)

        address_info = self.nodes[1].validateaddress(address2)
        assert_equal(address_info['iswatchonly'], False)
        assert_equal(address_info['ismine'], False)

        address_info = self.nodes[1].validateaddress(address3)
        assert_equal(address_info['iswatchonly'], False)
        assert_equal(address_info['ismine'], False)

        # import multi
        result1 = self.nodes[1].importmulti( [
            { "type": ADDRESS_KEY, "value": address1 , "label":"new account 1" , "timestamp": begintime } ,
            { "type": PUB_KEY , "value": address2_pubkey , "label":"new account 1", "timestamp": begintime},
            { "type": PRIV_KEY , "value": address3_privkey , "timestamp": begintime},
            { "type": SCRIPT_KEY , "value": address4_scriptpubkey , "timestamp": begintime},
            ])

        #Addresses Test - after import
        address_info = self.nodes[1].validateaddress(address1)
        assert_equal(address_info['iswatchonly'], True)
        assert_equal(address_info['ismine'], False)
        address_info = self.nodes[1].validateaddress(address2)
        assert_equal(address_info['iswatchonly'], True)
        assert_equal(address_info['ismine'], False)
        address_info = self.nodes[1].validateaddress(address3)
        assert_equal(address_info['iswatchonly'], False)
        assert_equal(address_info['ismine'], True)
        address_info = self.nodes[1].validateaddress(address4)
        assert_equal(address_info['iswatchonly'], True)
        assert_equal(address_info['ismine'], False)

        assert_equal(result1[0]['result'], True)
        assert_equal(result1[1]['result'], True)
        assert_equal(result1[2]['result'], True)
        assert_equal(result1[3]['result'], True)

        #importmulti without rescan
        result2 = self.nodes[1].importmulti( [
            { "type": ADDRESS_KEY, "value": self.nodes[0].getnewaddress() } ,
            { "type": ADDRESS_KEY, "value": self.nodes[0].getnewaddress() } ,
            { "type": ADDRESS_KEY, "value": self.nodes[0].getnewaddress() , "label":"random account" } ,
            { "type": PUB_KEY, "value": self.nodes[0].validateaddress(self.nodes[0].getnewaddress())['pubkey'] } ,
            { "type": SCRIPT_KEY, "value": self.nodes[0].validateaddress(self.nodes[0].getnewaddress())['scriptPubKey'] },
            ], { "rescan":False } )

        # all succeed
        assert_equal(result2[0]['result'], True)
        assert_equal(result2[1]['result'], True)
        assert_equal(result2[2]['result'], True)
        assert_equal(result2[3]['result'], True)
        assert_equal(result2[4]['result'], True)

        # empty json case
        try:
            self.nodes[1].importmulti()
            raise
        except:
            pass

        # parcial success case
        result3 = self.nodes[1].importmulti( [
            { "type": ADDRESS_KEY, "value": self.nodes[0].getnewaddress() } ,
            { "type": PUB_KEY} ] )

        assert_equal(result3[0]['result'], True)
        assert_equal(result3[1]['result'], False)


if __name__ == '__main__':
    ImportMultiTest ().main ()
