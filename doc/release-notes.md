Bitcoin Knots version 0.13.2.knots20170102 is now available from:

  <https://bitcoinknots.org/files/0.13.x/0.13.2.knots20170102/>

This is a new minor version release, including new features, various bugfixes
and performance improvements, as well as updated translations.

Please report bugs using the issue tracker at github:

  <https://github.com/bitcoinknots/bitcoin/issues>

Compatibility
==============

Microsoft ended support for Windows XP on [April 8th, 2014](https://www.microsoft.com/en-us/WindowsForBusiness/end-of-xp-support),
an OS initially released in 2001. This means that not even critical security
updates will be released anymore. Without security updates, using a bitcoin
wallet on a XP machine is irresponsible at least.

In addition to that, with 0.12.x there have been varied reports of Bitcoin Knots
randomly crashing on Windows XP. It is [not clear](https://github.com/bitcoin/bitcoin/issues/7681#issuecomment-217439891)
what the source of these crashes is, but it is likely that upstream
libraries such as Qt are no longer being tested on XP.

We do not have time nor resources to provide support for an OS that is
end-of-life. From 0.13.0 on, Windows XP is no longer supported. Users are
suggested to upgrade to a newer version of Windows, or install an alternative OS
that is supported.

No attempt is made to prevent installing or running the software on Windows XP,
you can still do so at your own risk, but do not expect it to work: do not
report issues about Windows XP to the issue tracker.

From 0.13.1 onwards OS X 10.7 is no longer supported. 0.13.0 was intended to work on 10.7+, 
but severe issues with the libc++ version on 10.7.x keep it from running reliably. 
0.13.1 now requires 10.8+, and will communicate that to 10.7 users, rather than crashing unexpectedly.

Notable changes
===============

importmulti is no longer stored in debug console history
--------------------------------------------------------

Since v0.12.0, Knots has supported importing private keys via the `importmulti`
RPC method. However, the logic to exclude sensitive commands from the debug
console history did not include this RPC method.

This security issue is fixed as of this release. Additionally, when you start
Knots, it will scan through your current persisted history and filter out any
`importmulti` uses it finds. This is implemented such that the sensitive info
is erased as securely as possible, but due to the nature of configuration
storage, it is unlikely to succeed in entirely removing it from your disk.

Change to wallet handling of mempool rejection
-----------------------------------------------

When a newly created transaction failed to enter the mempool due to
the limits on chains of unconfirmed transactions the sending RPC
calls would return an error.  The transaction would still be queued
in the wallet and, once some of the parent transactions were
confirmed, broadcast after the software was restarted.

This behavior has been changed to return success and to reattempt
mempool insertion at the same time transaction rebroadcast is
attempted, avoiding a need for a restart.

Transactions in the wallet which cannot be accepted into the mempool
can be abandoned with the previously existing abandontransaction RPC
(or in the GUI via a context menu on the transaction).

Bumpfee RPC method improvements
-------------------------------

Knots 0.13.1 added a new RPC method `bumpfee` to make use of opt-in RBF for
increasing transaction fees. This release updates the `bumpfee` method to the
latest version, including a number of improvements.

Specifying the change output index is now optional, and requires that if it is
specified, it is in fact a real change output. Support for specifying the
change output is deprecated, and will be removed no later than 0.14.

A new `maxFee` option is added to limit the maximum fee that `bumpfee` will
attempt to use.

Finally, there are new restrictions: a transaction can only be bumped once (its
replacement may be bumped for similar behaviour), and transactions with spent
outputs may not be bumped (to avoid breaking them).

RPC method to sweep private keys
--------------------------------

Private keys in WIF can now be "swept" into the local wallet using the new
`sweepprivkeys` RPC method. Unlike importing private keys, this should be safe
for even untrusted private keys (although you should wait for confirmation of
the sweep), and does not require a rescan. See `help sweepprivkeys` for usage.

Memory pool statistics
----------------------

The memory pool can now keep up to 10 MB of statistical information. To enable
this, use the `-statsenable` option; it is enabled default for the GUI and can
be disabled using `-statsenable=0`. The stats can be accessed with either the
new `getmempoolstats` RPC method, or the "Mempool Statistics" GUI on the "Help"
menu.

BIP 67 sorting for multisig addresses
-------------------------------------

The `addmultisigaddress` and `createmultisig` RPC methods now accept an
optional parameter to sort the keys according to BIP 67. The interface to
enable this is considered temporary and is likely to be changed without notice
(or backward compatibility) in future releases.

Saving and restoring of node memory pool
----------------------------------------

When cleanly shutdown, Knots will now store a copy of the node's memory pool
to disk. This will then be restored at startup, allowing for restarting the
node without redownloading unconfirmed transactions.

Nested calls in debug console
-----------------------------

The debug console will now interpret nested calls specified using parenthesis
syntax. For example, since `getblock` accepts a block hash, you can use
`getblockhash` to specify it from a block height:

    getblock(getblockhash(1))


0.13.2 Change log
=================

Detailed release notes follow. This overview includes changes that affect
behavior, not code moves, refactors and string updates. For convenience in locating
the code changes and accompanying discussion, both the pull request and
git merge commit are mentioned. Changes specific to Bitcoin Knots (beyond Core)
are flagged with an asterisk ('*') before the description.

### Consensus
- #9293 `e591c10` [0.13 Backport #9053] IBD using chainwork instead of height and not using header timestamp (gmaxwell)
- #9053 `5b93eee` IBD using chainwork instead of height and not using header timestamps (gmaxwell)
- n/a   `ce96c2a` *Add a new checkpoint at block 438,784 (luke-jr)

### RPC and other APIs
- #9041 `87fbced` keypoololdest denote Unix epoch, not GMT (s-matthew-english)
- #9122 `f82c81b` fix getnettotals RPC description about timemillis (visvirial)
- #9042 `5bcb05d` [rpc] ParseHash: Fail when length is not 64 (MarcoFalke)
- #9194 `f26dab7` Add option to return non-segwit serialization via rpc (instagibbs)
- #9347 `b711390` [0.13.2] wallet/rpc backports (MarcoFalke)
- #9292 `c365556` Complain when unknown rpcserialversion is specified (sipa)
- #9322 `49a612f` [qa] Don't set unknown rpcserialversion (MarcoFalke)
- #8456 `60d8e8c` *[RPC] bumpfee (mrbandrews)
- n/a   `0716303` *wallet/rpc: Tolerate former change output index parameter to bumpfee so long as it is actually change (luke-jr)
- #8704 `645b21c` *[RPC] Transaction details in getblock (achow101)
- #8751 `0b0f4f7` *RPC: addmultisigaddress / createmultisig: parameterize _createmultisig_redeemScript to allow sorting of public keys (BIP67) (Thomas Kerin)
- #8992 `f2eb1cf` *Enable pubkey lookup for p2sh-p2wpkh in validateaddress (instagibbs)
- #8952 `ed5bc96` *Add query options to listunspent rpc call (Pedro Branco)
- #9025 `b979006` *getrawtransaction should take a bool for verbose (jnewbery)
- #9152 `db7634e` *Wallet/RPC: sweepprivkeys method to scan UTXO set and send to local wallet (luke-jr)
- #9222 `274975f` *Add 'subtractFeeFromOutputs' option to 'fundrawtransaction'. (Chris Moore)
- #8501 `039d1d5` *Add mempool statistics collector (jonasschnelli)
- #8775 `41383de` *RPC: Pass on CRPCRequestInfo metadata (wallet) for "help" method (luke-jr)

### Block and transaction handling
- #9267 `0a4aa87` [0.13 backport #9239] Disable fee estimates for a confirm target of 1 block (morcos)
- #9196 `0c09d9f` Send tip change notification from invalidateblock (ryanofsky)

### P2P protocol and network code
- #8995 `9ef3875` Add missing cs_main lock to ::GETBLOCKTXN processing (TheBlueMatt)
- #9234 `94531b5` torcontrol: Explicitly request RSA1024 private key (laanwj)
- #8637 `2cad5db` Compact Block Tweaks (rebase of #8235) (sipa)
- #9058 `286e548` Fixes for p2p-compactblocks.py test timeouts on travis (#8842) (ryanofsky)
- #8865 `4c71fc4` Decouple peer-processing-logic from block-connection-logic (TheBlueMatt)
- #9117 `6fe3981` net: don't send feefilter messages before the version handshake is complete (theuni)
- #9188 `ca1fd75` Make orphan parent fetching ask for witnesses (gmaxwell)
- #9052 `3a3bcbf` Use RelevantServices instead of node_network in AttemptToEvict (gmaxwell)
- #9048 `9460771` [0.13 backport #9026] Fix handling of invalid compact blocks (sdaftuar)
- #9357 `03b6f62` [0.13 backport #9352] Attempt reconstruction from all compact block announcements (sdaftuar)
- #9189 `b96a8f7` Always add default_witness_commitment with GBT client support (sipa)
- #9253 `28d0f22` Fix calculation of number of bound sockets to use (TheBlueMatt)
- #9199 `da5a16b` Always drop the least preferred HB peer when adding a new one (gmaxwell)
- #9245 `294b8f5` *Drop IO priority to idle while reading blocks for getblock requests (luke-jr)

### Build system
- #9169 `d1b4da9` build: fix qt5.7 build under macOS (theuni)
- #9326 `a0f7ece` Update for OpenSSL 1.1 API (gmaxwell)
- #9224 `396c405` Prevent FD_SETSIZE error building on OpenBSD (ivdsangen)
- #7522 `ed1fcdc` *Bugfix: Detect genbuild.sh in repo correctly (luke-jr)
- n/a   `8f93b0c` *Makefile: Rename RES_RENDERED_ICON_SOURCES to RES_RENDERED_ICON_SRC to avoid automake giving it special meaning (luke-jr)

### GUI
- #8972 `6f86b53` Make warnings label selectable (jonasschnelli) (MarcoFalke)
- #9185 `6d70a73` Fix coincontrol sort issue (jonasschnelli)
- #9094 `5f3a12c` Use correct conversion function for boost::path datadir (laanwj)
- #8908 `4a974b2` Update bitcoin-qt.desktop (s-matthew-english)
- #9190 `dc46b10` Plug many memory leaks (laanwj)
- #8877 `19321e1` *GUI/RPCConsole: Include importmulti in history sensitive-command filter (luke-jr)
- #8985 `6b474fa` *Use pindexBestHeader instead of setBlockIndexCandidates for NotifyHeaderTip() (jonasschnelli)
- #8371 `d81ec3d` *[qt] sync-overlay: Don't show progress twice (MarcoFalke)
- #9130 `b26febe` *Mention the new network toggle functionality in the tooltip. (paveljanik)
- #9131 `ea2d47c` *fNetworkActive is not protected by a lock, use an atomic (jonasschnelli)
- #9145 `58e922e` *[qt] Make network disabled icon 50% opaque (MarcoFalke)
- #7783 `9db5e22` *[Qt] RPC-Console: support nested commands and simple value queries (jonasschnelli)
- #9329 `d63d8ab` *[Qt] Console: allow empty arguments (jonasschnelli)
- n/a   `84e423e` *gui: add NodeID to the peer table (cfields)
- #8874 `642641c` *Multiple Selection for peer and ban tables (achow101)
- #8925 `9667f76` *Partial: Display minimum ping in debug window. (rebroad)
- n/a   `9994958` *Qt/Options: Configure minrelaytxfee using rwconf (luke-jr)
- #8550 `71bfbf0` *[Qt] Add interactive mempool graph (jonasschnelli)
- n/a   `e4987a8` *Qt/Stats: Use native widgets (luke-jr)
- #8694 `9f8717c` *Qt: QComboBox::setVisible doesn't work in toolbars, so defer adding it at all until needed (luke-jr)

### Wallet
- #9290 `35174a0` Make RelayWalletTransaction attempt to AcceptToMemoryPool (gmaxwell)
- #9295 `43bcfca` Bugfix: Fundrawtransaction: don't terminate when keypool is empty (jonasschnelli)
- #9302 `f5d606e` Return txid even if ATMP fails for new transaction (sipa)
- #9262 `fe39f26` Prefer coins that have fewer ancestors, sanity check txn before ATMP (instagibbs)
- #9167 `844f0e1` *[wallet] Add IsAllFromMe: true if all inputs are from wallet (sdaftuar)

### Tests and QA
- #9159 `eca9b46` Wait for specific block announcement in p2p-compactblocks (ryanofsky)
- #9186 `dccdc3a` Fix use-after-free in scheduler tests (laanwj)
- #9168 `3107280` Add assert_raises_message to check specific error message (mrbandrews)
- #9191 `29435db` 0.13.2 Backports (MarcoFalke)
- #9077 `1d4c884` Increase wallet-dump RPC timeout (ryanofsky)
- #9098 `ecd7db5` Handle zombies and cluttered tmpdirs (MarcoFalke)
- #8927 `387ec9d` Add script tests for FindAndDelete in pre-segwit and segwit scripts (jl2012)
- #9200 `eebc699` bench: Fix subtle counting issue when rescaling iteration count (laanwj)
- #9097 `f52b8b6` *[qa] preciousblock: Use assert_equal and BitcoinTestFramework.__init__ (MarcoFalke)
- #9329 `4fce7fc` *Qt/Test: Check handling of empty arguments in RPC debug console (luke-jr)
- #8450 `77b324e` *Account wallet feature RPC tests. (phantomcircuit)

### Miscellaneous
- #8838 `094848b` Calculate size and weight of block correctly in CreateNewBlock() (jnewbery)
- #8920 `40169dc` Set minimum required Boost to 1.47.0 (fanquake)
- #9251 `a710a43` Improvement of documentation of command line parameter 'whitelist' (wodry)
- #8932 `106da69` Allow bitcoin-tx to create v2 transactions (btcdrak)
- #8929 `12428b4` add software-properties-common (sigwo)
- #9120 `08d1c90` bug: Missed one "return false" in recent refactoring in #9067 (UdjinM6)
- #9067 `f85ee01` Fix exit codes (UdjinM6)
- #9340 `fb987b3` [0.13] Update secp256k1 subtree (MarcoFalke)
- #9229 `b172377` Remove calls to getaddrinfo_a (TheBlueMatt)
- #8813 `377ab84` *bitcoind: Daemonize using daemon(3) (Matthew King)
- #8848 `7e4b10b` *Store mempool and prioritization data to disk (sipa)
- #8936 `046e4da` *Report NodeId in misbehaving debug (rebroad)

Credits
=======

Thanks to everyone who directly contributed to this release:

- Alex Morcos
- Andrew Chow
- BtcDrak
- Chris Moore
- Cory Fields
- fanquake
- Gregory Maxwell
- Gregory Sanders
- instagibbs
- Ivo van der Sangen
- John Newbery
- Johnson Lau
- Jonas Schnelli
- Luke Dashjr
- maiiz
- MarcoFalke
- Masahiko Hyuga
- Matt Corallo
- Matthew King
- matthias
- mrbandrews
- Patrick Strateman
- Pavel Janík
- Pedro Branco
- Pieter Wuille
- randy-waterhouse
- R E Broadley
- Russell Yanofsky
- S. Matthew English
- Steven
- Suhas Daftuar
- Thomas Kerin
- UdjinM6
- Wladimir J. van der Laan
- wodry

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/bitcoin/).
