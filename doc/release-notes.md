26.1 Release Notes
==================

Bitcoin Knots version 26.1.knots20240325 is now available from:

  <https://bitcoinknots.org/files/26.x/26.1.knots20240325/>

This release includes new features, various bug fixes and performance
improvements, as well as updated translations.

Please report bugs using the issue tracker at GitHub:

  <https://github.com/bitcoinknots/bitcoin/issues>

To receive security and update notifications, please subscribe to:

  <https://bitcoinknots.org/list/announcements/join/>

How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has completely
shut down (which might take a few minutes in some cases), then run the
installer (on Windows) or just copy over `/Applications/Bitcoin-Qt` (on macOS)
or `bitcoind`/`bitcoin-qt` (on Linux).

Upgrading directly from very old versions of Bitcoin Core or Knots is
possible, but it might take some time if the data directory needs to be migrated. Old
wallet versions of Bitcoin Knots are generally supported.

Compatibility
==============

Bitcoin Knots is supported on operating systems using the Linux kernel,
macOS 11.0+, and Windows 7 and newer. It is not recommended to use
Bitcoin Knots on unsupported systems.

Known Bugs
==========

In various locations, including the GUI's transaction details dialog and the
"vsize" result in many RPC results, transaction virtual sizes may not account
for an unusually high number of sigops (ie, as determined by the
`-bytespersigop` policy). This could result in reporting a lower virtual size
than is actually used for mempool or mining purposes.

In the interest of time, this release does not include updated translations.
Please comment on GitHub <https://github.com/bitcoinknots/bitcoin/issues/73>
if this is of particular importance to you or anyone you know. If nobody
expresses interest in localization, it may continue to be skipped going
forward.

Notable changes
===============

Node policy changes
-------------------

- Spam is sometimes disguised to appear as if it is a deprecated pay-to-IP
  (bare pubkey) transaction, where the "key" is actually arbitrary data (not a
  real key) instead. Due to the nature of this spam, it pollutes not only the
  blockchain, but also permanently bloats the "UTXO set" all nodes must retain
  (it cannot be pruned). Real pay-to-IP was only ever supported by Satoshi's
  early Bitcoin wallet, which has been abandoned since 2011. This version of
  Bitcoin Knots filters out this kind of spam by default. A new setting,
  `-permitbarepubkey` (also available in the GUI), is available for users who
  desire to relay these. (#29309)

- Non-standard datacarrier formats are now filtered out by default. If you wish
  to re-enable relaying and mining them, you can set
  `-acceptnonstddatacarrier=1`

- There is now experimental (and disabled by default) support for dynamically
  adjusting the "dust" spam filter threshold. It can be enabled in the GUI, or
  by configuring `-dustdynamic=target:N` to adjust based on a fee estimate for
  confirmation within N blocks, or `-dustdynamic=mempool:N` to adjust based on
  the worst fee in the best N kvB of your mempool. In either mode, the fee will
  be adjusted every 15 minutes (the first time not for 15 minutes after
  startup, in an effort to ensure estimators have sufficient data to work
  with). Implementation details (including how often adjustment occurs) may be
  changed in future releases. If you use this feature, please leave a comment
  about your experience on GitHub:
  <https://github.com/bitcoinknots/bitcoin/issues/74>

P2P and network changes
-----------------------

- For several years now, miners have been regularly violating the Bitcoin
  protocol as it pertains to the block version bits, triggering warnings in
  Bitcoin Knots about softforks that aren't happening. While it would be
  inappropriate to simply concede a protocol change without community
  support, it's also confusing for users to see warnings that are inaccurate.
  To mitigate this, therefore, Bitcoin Knots is now more specific about when
  it warns about potential protocol changes, and only logs simple versionbit
  protocol violations in the debug log once for the specific block violating
  it.

- Support for the v2 transport protocol defined in
  [BIP324](https://github.com/bitcoin/bips/blob/master/bip-0324.mediawiki) was added.
  It is on by default and will be negotiated on a per-connection basis with other peers
  that support it, but can be disabled using `-v2transport=0` if desired. The existing
  v1 transport protocol remains fully supported. (#28331)

- Nodes with multiple reachable networks will actively try to have at least one
  outbound connection to each network. This improves individual resistance to
  eclipse attacks and network level resistance to partition attacks. Users no
  longer need to perform active measures to ensure being connected to multiple
  enabled networks. (#27213)

- Support for I2P sessions encrypted using the newer and faster ECIES-X25519
  algorithm has been added. (#29200)

Pruning
-------

- When using assumeutxo with `-prune`, the prune budget may be exceeded if it is set
  lower than 1100MB (i.e. `MIN_DISK_SPACE_FOR_BLOCK_FILES * 2`). Prune budget is normally
  split evenly across each chainstate, unless the resulting prune budget per chainstate
  is beneath `MIN_DISK_SPACE_FOR_BLOCK_FILES` in which case that value will be used. (#27596)

Updated RPCs
------------

- The `maxfeerate` option for `sendrawtransaction` and `testmempoolaccept` is
  now limited to below 100000 sats/vB to avoid overflow bugs. (#29434)

- Setting `-rpcserialversion=0` is deprecated and will be removed in
  a future release. It can currently still be used by also adding
  the `-deprecatedrpc=serialversion` option. (#28448)

- The `hash_serialized_2` value has been removed from `gettxoutsetinfo` since the value it
  calculated contained a bug and did not take all data into account. It is superseded by
  `hash_serialized_3` which provides the same functionality but serves the correctly calculated hash. (#28685)

- For RPC methods which accept `options` parameters (`importmulti`, `listunspent`,
  `fundrawtransaction`, `bumpfee`, `scanblocks`, `send`, `sendall`, `walletcreatefundedpsbt`,
  `simulaterawtransaction`), it is now possible to pass the options as named
  parameters without the need for a nested object. (#26485)

This means it is possible make calls like:

```sh
src/bitcoin-cli -named bumpfee txid fee_rate=100
```

instead of

```sh
src/bitcoin-cli -named bumpfee txid options='{"fee_rate": 100}'
```

- New fields `transport_protocol_type` and `session_id` were added to the `getpeerinfo` RPC to indicate
  whether the v2 transport protocol is in use, and if so, what the session id is. (#28331)

- A new argument `v2transport` was added to the `addnode` RPC to indicate whether a v2 transaction connection
  is to be attempted with the peer. This breaks compatibility with the old
  `privileged` parameter used with Bitcoin Knots v0.16.0.knots20180322 through
  v0.20.1.knots20200815. If you were relying on this deprecated parameter, you
  can maintain the previous behaviour by specifying "outbound-full-relay" to
  the "connection_type" parameter. (#28331)

- [Miniscript](https://bitcoin.sipa.be/miniscript/) expressions can now be used in Taproot descriptors for all RPCs working with descriptors. (#27255)

- `finalizepsbt` is now able to finalize a PSBT with inputs spending [Miniscript](https://bitcoin.sipa.be/miniscript/)-compatible Taproot leaves. (#27255)

- The result from `getpeerinfo` now includes "misbehavior_score". This number
  is increased (by varying amounts) when a peer doesn't follow the protocol
  correctly, and when it reaches 100, the peer is disconnected. It is mostly
  only useful for testing purposes. (#29530)

Changes to wallet related RPCs can be found in the Wallet section below.

New RPCs
--------

- `loadtxoutset` has been added, which allows loading a UTXO snapshot of the format
  generated by `dumptxoutset`. Once this snapshot is loaded, its contents will be
  deserialized into a second chainstate data structure, which is then used to sync to
  the network's tip.

  Meanwhile, the original chainstate will complete the initial block download process in
  the background, eventually validating up to the block that the snapshot is based upon.

  The result is a usable bitcoind instance that is current with the network tip in a
  matter of minutes rather than hours. However, until the full background sync completes,
  the node and any wallets using it remain insecure and should not be trusted or relied
  on for confirmation of payment. (#27596)

  You can find more information on this process in the `assumeutxo` design
  document (<https://github.com/bitcoin/bitcoin/blob/master/doc/design/assumeutxo.md>).

  `getchainstates` has been added to aid in monitoring the assumeutxo sync process.

- A new RPC, `submitpackage`, has been added. It can be used to submit a list of raw hex
transactions to the mempool to be evaluated as a package using consensus and mempool policy rules.
These policies include package CPFP, allowing a child with high fees to bump a parent below the
mempool minimum feerate (but not minimum relay feerate). (#27609)

  - Warning: successful submission does not mean the transactions will propagate throughout the
    network, as package relay is not supported.

  - Not all features are available. The package is limited to a child with all of its
    unconfirmed parents, and no parent may spend the output of another parent.  Also, package
    RBF is not supported. Refer to doc/policy/packages.md for more details on package policies
    and limitations.

  - This RPC is experimental. Its interface may change.

- A new `importmempool` RPC has been added. It loads a valid `mempool.dat` file and attempts to
  add its contents to the mempool. This can be useful to import mempool data from another node
  without having to modify the datadir contents and without having to restart the node. (#27460)
    - Warning: Importing untrusted files is dangerous, especially if metadata from the file is taken over.
    - Since coin-age priority deltas are stored in a separate `mempool-knots.dat` file, it is not supported by this new `importmempool` method.
    - If you want to apply priority deltas, it is recommended to use the `getprioritisedtransactions` and
      `prioritisetransaction` RPCs instead. This supports both coin-age priority and fee deltas.

- `listmempooltransactions` has been added to efficiently monitor transactions
  as they are added to the node's mempool, using sequence numbers. (#29016)

Updated settings
----------------

- Passing an invalid `-debug`, `-debugexclude`, or `-loglevel` logging configuration
  option now raises an error, rather than logging an easily missed warning. (#27632)

Changes to GUI or wallet related settings can be found in the GUI or Wallet section below.

Tools and Utilities
-------------------

- A new `bitcoinconsensus_verify_script_with_spent_outputs` function is available in libconsensus which optionally accepts the spent outputs of the transaction being verified. (#28539)
- A new `bitcoinconsensus_SCRIPT_FLAGS_VERIFY_TAPROOT` flag is available in libconsensus that will verify scripts with the Taproot spending rules. (#28539)
- `bitcoin-tx`'s `replacable` option is now optional. If omitted, inputs will be created opting into Replace-By-Fee. (#29022)

Wallet
------

- Wallet loading has changed in this release. Wallets with some corrupted records that could be
  previously loaded (with warnings) may no longer load. For example, wallets with corrupted
  address book entries may no longer load. If this happens, it is recommended
  load the wallet in a previous version of Bitcoin Core and import the data into a new wallet.
  Please also report an issue to help improve the software and make wallet loading more robust
  in these cases. (#24914)

- The `gettransaction`, `listtransactions`, `listsinceblock` RPCs now return
  the `abandoned` field for all transactions. Previously, the "abandoned" field
  was only returned for sent transactions. (#25158)

- The `listdescriptors`, `decodepsbt` and similar RPC methods now show `h` rather than apostrophe (`'`) to indicate
  hardened derivation. This does not apply when using the `private` parameter, which
  matches the marker used when descriptor was generated or imported. Newly created
  wallets use `h`. This change makes it easier to handle descriptor strings manually.
  E.g. the `importdescriptors` RPC call is easiest to use `h` as the marker: `'["desc": ".../0h/..."]'`.
  With this change `listdescriptors` will use `h`, so you can copy-paste the result,
  without having to add escape characters or switch `'` to 'h' manually.
  Note that this changes the descriptor checksum.
  For legacy wallets the `hdkeypath` field in `getaddressinfo` is unchanged,
  nor is the serialization format of wallet dumps. (#26076)

- Coin selection and transaction building now accounts for unconfirmed low-feerate ancestor transactions. When it is necessary to spend unconfirmed outputs, the wallet will add fees to ensure that the new transaction with its ancestors will achieve a mining score equal to the feerate requested by the user. (#26152)

- The `deprecatedrpc=walletwarningfield` configuration option has been removed.
  The `createwallet`, `loadwallet`, `restorewallet` and `unloadwallet` RPCs no
  longer return the "warning" string field. The same information is provided
  through the "warnings" field added in v25.0, which returns a JSON array of
  strings. The "warning" string field was deprecated also in v25.0. (#27757)

- The `signrawtransactionwithkey`, `signrawtransactionwithwallet`,
  `walletprocesspsbt` and `descriptorprocesspsbt` calls now return the more
  specific RPC_INVALID_PARAMETER error instead of RPC_MISC_ERROR if their
  sighashtype argument is malformed. (#28113)

- It's now possible to use [Miniscript](https://bitcoin.sipa.be/miniscript/) inside Taproot leaves for descriptor wallets. (#27255)

- If `send` or `sendall` methods are used with the `locktime` parameter is
  omitted, the wallet will automatically populate it with an appropriate value
  to deter fee sniping. (#28944)

GUI changes
-----------

- A new menu option allows migrating a legacy wallet based on keys and implied output script types stored in BerkeleyDB (BDB) to a modern wallet that uses descriptors stored in SQLite. (gui#738)

- The PSBT operations dialog marks outputs paying your own wallet with "own address". (gui#740)

Low-level changes
=================

Tests
-----

- Non-standard transactions are now disabled by default on testnet
  for relay and mempool acceptance. The previous behaviour can be
  re-enabled by setting `-acceptnonstdtxn=1`. (#28354)

Credits
=======

Thanks to everyone who directly contributed to this release:

- 0xb10c
- Amiti Uttarwar
- Andrew Toth
- Anthony Towns
- Antoine Poinsot
- Antoine Riard
- Ari
- Aurèle Oulès
- Ava Chow
- Ayush Singh
- Ben Woosley
- Brandon Odiwuor
- Brotcrunsher
- brunoerg
- Bufo
- Carl Dong
- Casey Carter
- Cory Fields
- David Álvarez Rosa
- dergoegge
- dhruv
- dimitaracev
- Erik Arvstedt
- Erik McKelvey
- Fabian Jahr
- fanquake
- furszy
- glozow
- Greg Sanders
- Harris
- Hennadii Stepanov
- Hernan Marino
- ishaanam
- ismaelsadeeq
- Jake Rawsthorne
- James O'Beirne
- John Moffett
- Jon Atack
- josibake
- Kashif Smith
- kevkevin
- Kiminuo
- Larry Ruane
- Lőrinc
- Luke Dashjr
- marco
- MarcoFalke
- Mark Friedenbach
- Marnix
- Martin Leitner-Ankerl
- Martin Zumsande
- Matthew Zipkin
- Michael Ford
- Michael Tidwell
- mruddy
- Murch
- niftynei
- ns-xvrn
- pablomartin4btc
- Peter Todd
- Pieter Wuille
- Reese Russell
- Rhythm Garg
- Roman Zeyde
- Ryan Ofsky
- Sebastian Falbesoner
- Sjors Provoost
- stickies-v
- stratospher
- Suhas Daftuar
- TheCharlatan
- Tim Neubauer
- Tim Ruffing
- Torkel Rogstad
- UdjinM6
- Vasil Dimov
- virtu
- Vojtěch Strnad
- vuittont60
- willcl-ark
- Yusuf Sahin HAMZA
- zzzi2p

As well as to everyone that helped with translations on
[Transifex](https://www.transifex.com/bitcoin/bitcoin/).
