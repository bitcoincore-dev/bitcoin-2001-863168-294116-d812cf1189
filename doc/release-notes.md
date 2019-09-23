Bitcoin Knots version *0.18.1.knots20190920* is now available from:

  <https://bitcoinknots.org/files/0.18.x/0.18.1.knots20190920/>

This is a new minor version release, including new features, various bug
fixes and performance improvements, as well as updated translations.

Please report bugs using the issue tracker at GitHub:

  <https://github.com/bitcoinknots/bitcoin/issues>

How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has
completely shut down (which might take a few minutes for older
versions), then run the installer (on Windows) or just copy over
`/Applications/Bitcoin-Qt` (on Mac) or `bitcoind`/`bitcoin-qt` (on
Linux).

The first time you run version 0.15.0 or newer, your chainstate database
will be converted to a new format, which will take anywhere from a few
minutes to half an hour, depending on the speed of your machine.

Note that the block database format also changed in version 0.8.0 and
there is no automatic upgrade code from before version 0.8 to version
0.15.0 or later. Upgrading directly from 0.7.x and earlier without
redownloading the blockchain is not supported.  However, as usual, old
wallet versions are still supported.

Compatibility
==============

Bitcoin Knots is supported on operating systems using the Linux kernel,
macOS 10.10+, and Windows 7 and newer. It is not recommended to use
Bitcoin Knots on unsupported systems.

From 0.17.0 onwards, macOS <10.10 is no longer supported. 0.17.0 is
built using Qt 5.9.x, which doesn't support versions of macOS older than
10.10. Additionally, Bitcoin Knots does not yet change appearance when
macOS "dark mode" is activated.

Known issues
============

Wallet GUI
----------

For advanced users who have both (1) enabled coin control features, and
(2) are using multiple wallets loaded at the same time: The coin control
input selection dialog can erroneously retain wrong-wallet state when
switching wallets using the dropdown menu. For now, it is recommended
not to use coin control features with multiple wallets loaded.

Notable changes
===============

Graphical User Interface (GUI)
------------------------------

- Attempting to send to a Bitcoin address you have paid already will now be
  detected and show an error (which can be overridden). (Bitcoin addresses are
  intended to only be used only once ever.)

Node whitelisting feature
-------------------------

The `whitelist` and `whitebind` configuration options now allow specifying more
fine-grained permissions for each network address by prefixing it with
`permissions@`, where `permissions` can be any combination of:

- `bloomfilter`: allow requesting BIP37 filtered blocks and transactions even
  if `peerbloomfilters` is disabled.
- `noban`: do not ban for misbehavior.
- `forcerelay`: relay even non-standard transactions.
- `relay`: relay even in `-blocksonly` mode.
- `mempool`: allow requesting BIP35 mempool contents

Multiple permissions can be specified by separating them with commas.

RPC changes
-----------
- Exposed transaction version numbers are now treated as unsigned 32-bit integers
  instead of signed 32-bit integers. This matches their treatment in consensus
  logic. Versions greater than 2 continue to be non-standard (matching previous
  behavior of smaller than 1 or greater than 2 being non-standard). Note that
  this includes the joinpsbt command, which combines partially-signed
  transactions by selecting the highest version number.
- `getdescriptorinfo` now adds a checksum to output descriptors.
- The `getblock` RPC will now return a `fee` key for each (non-generation)
  transaction in verbosity level 2 or higher.
- The `getblock` RPC now includes a verbosity level 3 which will decode
  information about inputs for each (non-generation) transaction in the block,
  including `height`, `value`, `generated`, `scriptPubKey`, and `prevout`.
- The output of `getaddressinfo` now includes a `use_txids` key, which is a
  simple list of transaction ids this wallet knows to have used the address
  already. Usually this will only have zero or one transaction id, but may
  have more in cases of transaction replacement or address reuse.
- The `gettransaction` RPC now accepts a third (boolean) argument `verbose`. If
  set to `true`, a new `decoded` field will be added to the response containing
  the decoded transaction. This field is equivalent to RPC `decoderawtransaction`,
  or RPC `getrawtransaction` when `verbose` is passed.
- The `getchaintxstats` RPC now returns the additional key of
  `window_final_block_height`.
- Decoded raw transactions now include a `desc` key with an inferred Output
  Descriptor. (See doc/descriptors.md for more information on these.)
- The `getnetworkinfo` and `getpeerinfo` commands now contain a new field with
  decoded network service flags.
- The `getblockstats` RPC is faster for fee calculation by using BlockUndo
  data. Also, `-txindex` is no longer required and `getblockstats` works for
  all non-pruned blocks.
- The `confTarget` option for the `bumpfee` RPC has been renamed to
  `conf_target`. Backward compatibility is provided for now, but may be
  removed in the future.

0.18.1 change log
=================

### Consensus
- n/a    *Update chain params and add a new checkpoint at block 596,000 (luke-jr)

### Block and transaction handling
- #15861 *Bugfix: Initialise versionbit counters for warnings (luke-jr)

### P2P protocol and network code
- #15990 Add tests and documentation for blocksonly (MarcoFalke)
- #16021 Avoid logging transaction decode errors to stderr (MarcoFalke)
- #16405 fix: tor: Call `event_base_loopbreak` from the event's callback (promag)
- #16412 Make poll in InterruptibleRecv only filter for POLLIN events (tecnovert)
- #15558 *net: When -forcednsseed is provided, query all DNS seeds at once (sipa)

### Wallet
- #15913 Add -ignorepartialspends to list of ignored wallet options (luke-jr)
- n/a    *Add "use_txids" to output of getaddressinfo (luke-jr)
- #16185 *gettransaction: add an argument to decode the transaction (darosior)
- #16866 *Rename 'decode' argument in gettransaction method to 'verbose' (jnewbery)

### RPC and other APIs
- #15991 Bugfix: fix pruneblockchain returned prune height (jonasschnelli)
- #15899 Document iswitness flag and fix bug in converttopsbt (MarcoFalke)
- #16026 Ensure that uncompressed public keys in a multisig always returns a legacy address (achow101)
- #14039 Disallow extended encoding for non-witness transactions (sipa)
- #16210 add 2nd arg to signrawtransactionwithkey examples (dooglus)
- #16250 signrawtransactionwithkey: report error when missing redeemScript/witnessScript (ajtowns)
- #16525 *Dump transaction version as an unsigned integer in RPC/TxToUniv (BlueMatt)
- #11413 *rpc/wallet: add conf_target as alias to confTarget in bumpfee (kallewoof)
- #14802 *rpc: faster getblockstats using BlockUndo data (Felix Weis)
- #15986 *Add checksum to getdescriptorinfo (sipa)
- #16083 *getblock: tx fee calculation for verbosity 2 via Undo data, new verbosity level 3 showing prevout info for inputs (Felix Weis)
- #16248 *Make whitebind/whitelist permissions more flexible (NicolasDorier)
- #16695 *Add window final block height to getchaintxstats (Jonathan "Duke" Leto)
- #16787 *Human readable network services (darosior)
- #16795 *have raw transaction decoding infer output descriptors (instagibbs)

### GUI
- #16044 fix the bug of OPEN CONFIGURATION FILE on Mac (shannon1916)
- #15957 Show "No wallets available" in open menu instead of nothing (meshcollider)
- #16118 Enable open wallet menu on setWalletController (promag)
- #16135 Set progressDialog to nullptr (promag)
- #16231 Fix open wallet menu initialization order (promag) 
- #16254 Set `AA_EnableHighDpiScaling` attribute early (hebasto) 
- #16122 Enable console line edit on setClientModel (promag) 
- #16348 Assert QMetaObject::invokeMethod result (promag)
- #16090 *Add vertical spacer (Josu Goñi)
- #16826 *Do additional character escaping for wallet names and address labels (achow101)
- #15987 *Warn when sending to already-used Bitcoin addresses (luke-jr)
- #16852 *When BIP70 is disabled, get PaymentRequest merchant using string search (achow101)
- #16858 *advise users not to switch wallets when opening a BIP70 URI (Lightsword)
- #16153 *Add antialiasing to traffic graph widget (Josu Goñi)

### Build system
- #15985 Add test for GCC bug 90348 (sipa)
- #15947 Install bitcoin-wallet manpage (domob1812)
- #15983 build with -fstack-reuse=none (MarcoFalke)

### Tests and QA
- #15826 Pure python EC (sipa)
- #15893 Add test for superfluous witness record in deserialization (instagibbs)
- #15831 Add test that addmultisigaddress fails for watchonly addresses (MarcoFalke)
- #16564 *Always define the raii_event_tests test suite (luke-jr)
- #15911 *walletcreatefundedpsbt: check RBF is disabled when -walletrbf=0 (Sjors Provoost)
- #16646 *Run tests with UPnP disabled (luke-jr)
- #16850 *add a test for the 'servicesnames' RPC field (darosior)
- #16936 *Fix service flag comparison check in rpc_net test (luke-jr)

### Documentation
- #15890 Remove text about txes always relayed from -whitelist (harding)

### Miscellaneous
- #16095 Catch by reference not value in wallettool (kristapsk)
- #16205 Replace fprintf with tfm::format (MarcoFalke)
- #15970 *fix static_assert for macro HAVE_THREAD_LOCAL (orientye)
- REVERT #15600 *Revert "lockedpool: When possible, use madvise to avoid including sensitive information in core dumps or forked process memory spaces" (luke-jr)
- #16212 *addrdb: Remove temporary files created in SerializeFileDB (practicalswift)
- #16578 *Give QApplication dummy arguments (achow101)
- #15623 *refactor: Expose UndoReadFromDisk in header (MarcoFalke)
- #16760 *Change uninstall icon on Windows (GChuf)

Credits
=======

Thanks to everyone who directly contributed to this release:

- Andrew Chow
- Anthony Towns
- Chris Moore
- Daniel Kraft
- darosior
- David A. Harding
- fanquake
- Felix Weis
- GChuf
- Gregory Sanders
- Hennadii Stepanov
- James Hilliard
- John Newbery
- Jonas Schnelli
- Jonathan "Duke" Leto
- Josu Goñi
- João Barbosa
- Karl-Johan Alm
- Kristaps Kaupe
- Luke Dashjr
- MarcoFalke
- Matt Corallo
- MeshCollider
- Nicolas Dorier
- orientye
- Pieter Wuille
- practicalswift
- shannon1916
- Sjors Provoost
- tecnovert
- Wladimir J. van der Laan

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/bitcoin/).
