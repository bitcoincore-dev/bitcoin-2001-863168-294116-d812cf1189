# blocktools.py - utilities for manipulating blocks and transactions
#
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#

from binascii import a2b_hex, b2a_hex
import cStringIO
import struct

from mininode import *
from script import CScript, CScriptOp, OP_TRUE, OP_CHECKSIG

# Create a block (with regtest difficulty)
def create_block(hashprev=None, coinbase=None, nTime=None, tmpl=None, txlist=None):
    block = CBlock()
    if tmpl:
        block.nVersion = tmpl.get('version', block.nVersion)
    if not nTime is None:
        block.nTime = nTime
    elif tmpl and not tmpl.get('curtime') is None:
        block.nTime = tmpl['curtime']
    else:
        import time
        block.nTime = int(time.time()+600)
    if not hashprev is None:
        block.hashPrevBlock = hashprev
    else:
        block.hashPrevBlock = int ("0x" + tmpl['previousblockhash'] + "L", 0)
    if tmpl and not tmpl.get('bits') is None:
        block.nBits = struct.unpack('>I', a2b_hex(tmpl['bits']))[0]
    else:
        block.nBits = 0x207fffff # Will break after a difficulty adjustment...
    if not coinbase is None:
        block.vtx.append(coinbase)
    if txlist:
        for tx in txlist:
            if not hasattr(tx, 'calc_sha256'):
                txo = CTransaction()
                txo.deserialize(cStringIO.StringIO(tx))
                tx = txo
            block.vtx.append(tx)
    block.hashMerkleRoot = block.calc_merkle_root()
    block.calc_sha256()
    return block

def serialize_script_num(value):
    r = bytearray(0)
    if value == 0:
        return r
    neg = value < 0
    absvalue = -value if neg else value
    while (absvalue):
        r.append(chr(absvalue & 0xff))
        absvalue >>= 8
    if r[-1] & 0x80:
        r.append(0x80 if neg else 0)
    elif neg:
        r[-1] |= 0x80
    return r

# Create a coinbase transaction, assuming no miner fees.
# If pubkey is passed in, the coinbase output will be a P2PK output;
# otherwise an anyone-can-spend output.
def create_coinbase(height, pubkey = None):
    coinbase = CTransaction()
    coinbase.vin.append(CTxIn(COutPoint(0, 0xffffffff), 
                ser_string(serialize_script_num(height)), 0xffffffff))
    coinbaseoutput = CTxOut()
    coinbaseoutput.nValue = 50*100000000
    halvings = int(height/150) # regtest
    coinbaseoutput.nValue >>= halvings
    if (pubkey != None):
        coinbaseoutput.scriptPubKey = CScript([pubkey, OP_CHECKSIG])
    else:
        coinbaseoutput.scriptPubKey = CScript([OP_TRUE])
    coinbase.vout = [ coinbaseoutput ]
    coinbase.calc_sha256()
    return coinbase

# Create a transaction with an anyone-can-spend output, that spends the
# nth output of prevtx.
def create_transaction(prevtx, n, sig, value):
    tx = CTransaction()
    assert(n < len(prevtx.vout))
    tx.vin.append(CTxIn(COutPoint(prevtx.sha256, n), sig, 0xffffffff))
    tx.vout.append(CTxOut(value, ""))
    tx.calc_sha256()
    return tx
