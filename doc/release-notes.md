25.1 Release Notes
==================

Bitcoin Knots version 25.1.knots20231115 is now available from:

  <https://bitcoinknots.org/files/25.x/25.1.knots20231115/>

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
macOS 10.15+, and Windows 7 and newer. It is not recommended to use
Bitcoin Knots on unsupported systems.

Known Bugs
==========

In various locations, including the GUI's transaction details dialog, transaction
virtual sizes may not account for an unusually high number of sigops (ie, as
determined by the `-bytespersigop` policy). This could result in reporting a
lower virtual size than is actually used for mempool or mining purposes.

In the interest of time, this release does not include updated translations.
Please open an issue on GitHub if this is of particular importance to you or
anyone you know. If nobody expresses interest in localization, it may continue
to be skipped going forward.

Notable changes
===============

Security vulnerabilities fixed
------------------------------

Several security issues (CVEs CVE-2023-40257, CVE-2023-40258, and CVE-2023-40259)
were identified in the extended `rpcauth` wallet-restriction syntax, which is
intended to enable semi-trusted local applications using the Bitcoin Knots API to
access only specific wallets (or none at all) and not others. The full details of
these issues will be disclosed at a future time, but they have all been fixed in
this release.

P2P and network changes
-----------------------

- To address a potential denial-of-service, the logic to download headers from peers
  has been reworked.  This is particularly relevant for nodes starting up for the
  first time (or for nodes which are starting up after being offline for a long time).

  Whenever headers are received from a peer that have a total chainwork that is either
  less than the node's `-minimumchainwork` value or is sufficiently below the work at
  the node's tip, a "presync" phase will begin, in which the node will download the
  peer's headers and verify the cumulative work on the peer's chain, prior to storing
  those headers permanently. Once that cumulative work is verified to be sufficiently high,
  the headers will be redownloaded from that peer and fully validated and stored.

  This may result in initial headers sync taking longer for new nodes starting up for
  the first time, both because the headers will be downloaded twice, and because the effect
  of a peer disconnecting during the presync phase (or while the node's best headers chain has less
  than `-minimumchainwork`), will result in the node needing to use the headers presync mechanism
  with the next peer as well (downloading the headers twice, again). (#25717)

- Enable parallel compact block downloads to mitigate temporary stalls from a slow peer. (#27626)

- The rate limit on transaction relay has been doubled to 14 transactions per
  second (35 tx/s for outbound connections) to better adapt to present network
  conditions. (#28592)

- Transactions of non-witness size 65 and above are now allowed by mempool
  and relay policy. This is to better reflect the actual afforded protections
  against CVE-2017-12842 and open up additional use-cases of smaller transaction sizes. (#26265)

- With I2P connections, a new, transient address is used for each outbound
  connection if `-i2pacceptincoming=0`. (#25355)

- A new net permission `forceinbound` (set with `-whitelist=forceinbound@...`
  or `-whitebind=forceinbound@...`) is introduced that extends `noban` permissions.
  Inbound connections from hosts protected by `forceinbound` will now be more
  likely to connect even if `maxconnections` is reached and the node's inbound
  slots are full. This is achieved by attempting to force the eviction of a random,
  inbound, otherwise unprotected peer. RPC `getpeerinfo` will also now indicate
  `forced_inbound`. (#27600)

- The `-datacarriersize` policy limit has been updated to match newer style datacarrier
  transactions. (#28408)

- An additional `-datacarriercost` option has also been added to avoid giving the "segwit
  discount" to aribitrary data (and can be increased to require datacarrier transactions
  to pay higher fees).

- The spam filters limiting smart contract code sizes for pre-Segwit and Segwit
  "v0" scripts have been expanded to also cover Taproot. Since the sizes were
  inconsistent, the lower size of 1650 which actually had a rationale has been
  used as the default for this release. The size limit can be adjusted with the
  new `-maxscriptsize` option. If you know of any legitimate scripts that are
  larger than the default, please report a bug.

New RPCs
--------

- A new `getprioritisedtransactions` RPC has been added. It returns a map of all fee deltas created by the
  user with prioritisetransaction, indexed by txid. The map also indicates whether each transaction is
  present in the mempool.

- `getaddrmaninfo` has been added to view the distribution of addresses in the
  new and tried table of the node's address manager across different networks
  (ipv4, ipv6, onion, i2p, cjdns). The RPC returns count of addresses in new
  and tried table as well as their sum for all networks. (#27511, #28565)

Updated RPCs
------------

- The `verifychain` RPC will now return `false` if the checks didn't fail,
  but couldn't be completed at the desired depth and level. This could be due
  to missing data while pruning, due to an insufficient dbcache or due to
  the node being shutdown before the call could finish. (#25574)

- `sendrawtransaction` has a new, optional argument, `maxburnamount` with a default value of `0`.
  Any transaction containing an unspendable output with a value greater than `maxburnamount` will
  not be submitted. At present, the outputs deemed unspendable are those with scripts that begin
  with an `OP_RETURN` code (known as 'datacarriers'), scripts that exceed the maximum script size,
  and scripts that contain invalid opcodes.

- The `testmempoolaccept` RPC now returns 2 additional results within the "fees" result:
  "effective-feerate" is the feerate including fees and sizes of transactions validated together if
  package validation was used, and also includes any modified fees from prioritisetransaction. The
  "effective-includes" result lists the wtxids of transactions whose modified fees and sizes were used
  in the effective-feerate (#26646).

- `decodescript` may now infer a Miniscript descriptor under P2WSH context if it is not lacking
  information. (#27037)

- `finalizepsbt` is now able to finalize a transaction with inputs spending Miniscript-compatible
  P2WSH scripts. (#24149)

- The `-deprecatedrpc=softforks` configuration option has been removed.  The
  RPC `getblockchaininfo` no longer returns the `softforks` field, which was
  previously deprecated in 23.0. (#23508) Information on soft fork status is
  now only available via the `getdeploymentinfo` RPC.

- The `deprecatedrpc=exclude_coinbase` configuration option has been removed.
  The `receivedby` RPCs (`listreceivedbyaddress`, `listreceivedbylabel`,
  `getreceivedbyaddress` and `getreceivedbylabel`) now always return results
  accounting for received coins from coinbase outputs, without an option to
  change that behaviour. Excluding coinbases was previously deprecated in 23.0.
  (#25171)

- The `deprecatedrpc=fees` configuration option has been removed. The top-level
  fee fields `fee`, `modifiedfee`, `ancestorfees` and `descendantfees` are no
  longer returned by RPCs `getmempoolentry`, `getrawmempool(verbose=true)`,
  `getmempoolancestors(verbose=true)` and `getmempooldescendants(verbose=true)`.
  The same fee fields can be accessed through the `fees` object in the result.
  The top-level fee fields were previously deprecated in 23.0. (#25204)

- The `getpeerinfo` RPC has been updated with a new `presynced_headers` field,
  indicating the progress on the presync phase mentioned in the
  "P2P and network changes" section above.

- RPC `disconnectnode` now accepts a subnet into `address` argument. (#26576)

- The output from `getmempoolinfo` is extended to include a `rbf_policy` key
  briefly describing the current replace-by-fee policy ("never", "optin", or
  "always").

Changes to wallet related RPCs can be found in the Wallet section below.

Build System
------------

- The `--enable-upnp-default` and `--enable-natpmp-default` options
  have been removed. If you want to use port mapping, you can
  configure it using a .conf file, or by passing the relevant
  options at runtime. (#26896)

Updated settings
----------------

- If the `-checkblocks` or `-checklevel` options are explicitly provided by the
user, but the verification checks cannot be completed due to an insufficient
dbcache, Bitcoin Knots will now return an error at startup. (#25574)

- Setting `-blocksonly` will now reduce the maximum mempool memory
  to 5MB (users may still use `-maxmempool` to override). Previously,
  the default 300MB would be used, leading to unexpected memory usage
  for users running with `-blocksonly` expecting it to eliminate
  mempool memory usage.

  As unused mempool memory is shared with dbcache, this also reduces
  the dbcache size for users running with `-blocksonly`, potentially
  impacting performance.
- Setting `-maxconnections=0` will now disable `-dnsseed`
  and `-listen` (users may still set them to override).

Changes to GUI or wallet related settings can be found in the GUI or Wallet section below.

New settings
------------

- The `shutdownnotify` option is used to specify a command to execute synchronously
  before Bitcoin Knots has begun its shutdown sequence. (#23395)

- To assist in controlling access to the RPC "cookie" file on multiuser systems, a new
  `rpccookieperms` setting has been added to set the file permissions mode before
  writing the access token.

Wallet
------

- Added a new `next_index` field in the response in `listdescriptors` to
  have the same format as `importdescriptors` (#26194)

- RPC `listunspent` now has a new argument `include_immature_coinbase`
  to include coinbase UTXOs that don't meet the minimum spendability
  depth requirement (which before were silently skipped). (#25730)

- Rescans for descriptor wallets are now significantly faster if compact
  block filters (BIP158) are available. Since those are not constructed
  by default, the configuration option "-blockfilterindex=1" has to be
  provided to take advantage of the optimization. This improves the
  performance of the RPC calls `rescanblockchain`, `importdescriptors`
  and `restorewallet`. (#25957)

- RPC `unloadwallet` now fails if a rescan is in progress. (#26618)

- Wallet passphrases may now contain null characters.
  Prior to this change, only characters up to the first
  null character were recognized and accepted. (#27068)

- Address Purposes strings are now restricted to the currently known values of "send",
  "receive", and "refund". Wallets that have unrecognized purpose strings will have
  loading warnings, and the `listlabels` RPC will raise an error if an unrecognized purpose
  is requested. (#27217)

- In the `createwallet`, `loadwallet`, `unloadwallet`, and `restorewallet` RPCs, the
  "warning" string field is deprecated in favor of a "warnings" field that
  returns a JSON array of strings to better handle multiple warning messages and
  for consistency with other wallet RPCs. The "warning" field will be fully
  removed from these RPCs in v26. It can be temporarily re-enabled during the
  deprecation period by launching bitcoind with the configuration option
  `-deprecatedrpc=walletwarningfield`. (#27279)

- Descriptor wallets can now spend coins sent to P2WSH Miniscript descriptors. (#24149)

- The `-walletrbf` startup option will now default to `true`. The
  wallet will now default to opt-in RBF on transactions that it creates. (#25610)

- The `replaceable` option for the `createrawtransaction` and
  `createpsbt` RPCs will now default to `true`. Transactions created
  with these RPCs will default to having opt-in RBF enabled. (#25610)

- The `wsh()` output descriptor was extended with Miniscript support. You can import Miniscript
  descriptors for P2WSH in a watchonly wallet to track coins, but you can't spend from them using
  the Bitcoin Core wallet yet.
  You can find more about Miniscript on the [reference website](https://bitcoin.sipa.be/miniscript/). (#24148)

- The `tr()` output descriptor now supports multisig scripts through the `multi_a()` and
  `sortedmulti_a()` functions. (#24043)

- The `importdescriptors` RPC can now be used to import BIP 93 (codex32) seeds. (#27351)

- To help prevent fingerprinting transactions created by the Bitcoin Core wallet, change output
  amounts are now randomized. (#24494)

- The `listsinceblock`, `listtransactions` and `gettransaction` output now contain a new
  `parent_descs` field for every "receive" entry. (#25504)

- A new optional `include_change` parameter was added to the `listsinceblock` command. (#25504)

- RPC `importaddress` now supports watch-only descriptor wallets.

- RPC `walletprocesspsbt` return object now includes field `hex` (if the transaction
  is complete) containing the serialized transaction suitable for RPC `sendrawtransaction`. (#28414)

- The result of RPC `getaddressinfo` adds an `isactive` key which can be used to determine if
  the address is derived from the currently active wallet seed. (#27216)

- RPC `getreceivedbylabel` now returns an error, "Label not found
  in wallet" (-4), if the label is not in the address book. (#25122)

Migrating Legacy Wallets to Descriptor Wallets
---------------------------------------------

An experimental RPC `migratewallet` has been added to migrate Legacy (non-descriptor) wallets to
Descriptor wallets. More information about the migration process is available in the
[documentation](https://github.com/bitcoin/bitcoin/blob/master/doc/managing-wallets.md#migrating-legacy-wallets-to-descriptor-wallets).

GUI changes
-----------

- The "Mask values" is a persistent option now. (gui#701)

- The "Mask values" option affects the "Transaction" view now, in addition to the
  "Overview" one. (gui#708)

- A new menu item to restore a wallet from a backup file has been added (gui#471).

- Configuration changes made in the bitcoin GUI (such as the pruning setting,
proxy settings, UPNP preferences) are now saved to `<datadir>/settings.json`
file rather than to the Qt settings backend (windows registry or unix desktop
config files), so these settings will now apply to bitcoind, instead of being
ignored. (#15936, gui#602)

- Also, the interaction between GUI settings and `bitcoin.conf` settings is
simplified. Settings from `bitcoin.conf` are now displayed normally in the GUI
settings dialog, instead of in a separate warning message ("Options set in this
dialog are overridden by the configuration file: -setting=value"). And these
settings can now be edited because `settings.json` values take precedence over
`bitcoin.conf` values. (#15936)

REST
----

- A new `/rest/deploymentinfo` endpoint has been added for fetching various
  state info regarding deployments of consensus changes. (#25412)

- The `/headers/` and `/blockfilterheaders/` endpoints have been updated to use
  a query parameter instead of path parameter to specify the result count. The
  count parameter is now optional, and defaults to 5 for both endpoints. The old
  endpoints are still functional, and have no documented behaviour change.

  For `/headers`, use
  `GET /rest/headers/<BLOCK-HASH>.<bin|hex|json>?count=<COUNT=5>`
  instead of
  `GET /rest/headers/<COUNT>/<BLOCK-HASH>.<bin|hex|json>` (deprecated)

  For `/blockfilterheaders/`, use
  `GET /rest/blockfilterheaders/<FILTERTYPE>/<BLOCK-HASH>.<bin|hex|json>?count=<COUNT=5>`
  instead of
  `GET /rest/blockfilterheaders/<FILTERTYPE>/<COUNT>/<BLOCK-HASH>.<bin|hex|json>` (deprecated)

  (#24098)

Binary verification
----

- The binary verification script has been updated. In previous releases it
  would verify that the binaries had been signed with a single "release key".
  In this release and moving forward it will verify that the binaries are
  signed by a _threshold of trusted keys_. For more details and
  examples, see:
  https://github.com/bitcoin/bitcoin/blob/master/contrib/verify-binaries/README.md
  (#27358)

Low-level changes
=================

RPC
---

- The JSON-RPC server now rejects requests where a parameter is specified multiple
  times with the same name, instead of silently overwriting earlier parameter values
  with later ones. (#26628)

- RPC `listsinceblock` now accepts an optional `label` argument
  to fetch incoming transactions having the specified label. (#25934)

- Previously `setban`, `addpeeraddress`, `walletcreatefundedpsbt`, methods
  allowed non-boolean and non-null values to be passed as boolean parameters.
  Any string, number, array, or object value that was passed would be treated
  as false. After this change, passing any value except `true`, `false`, or
  `null` now triggers a JSON value is not of expected type error. (#26213)

- The `deriveaddresses`, `getdescriptorinfo`, `importdescriptors` and `scantxoutset` commands now
  accept Miniscript expression within a `wsh()` descriptor. (#24148)

- The `getaddressinfo`, `decodescript`, `listdescriptors` and `listunspent` commands may now output
  a Miniscript descriptor inside a `wsh()` where a `wsh(raw())` descriptor was previously returned. (#24148)

Credits
=======

Thanks to everyone who directly contributed to this release:

- /dev/fd0
- 0xb10c
- 721217.xyz
- Abubakar Sadiq Ismail
- Adam Jonas
- akankshakashyap
- Ali Sherief
- amadeuszpawlik
- Amiti Uttarwar
- Andreas Kouloumos
- Andrew Chow
- Andrew Toth
- Anthony Towns
- Antoine Poinsot
- Antoine Riard
- Aurèle Oulès
- avirgovi
- Ayush Sharma
- Baas
- Ben Woosley
- Bitcoin Hodler
- BrokenProgrammer
- brunoerg
- Bruno Garcia
- brydinh
- Bushstar
- Calvin Kim
- CAnon
- Carl Dong
- chinggg
- Chris Geihsler
- Cory Fields
- Daniel Kraft
- Daniela Brozzoni
- darosior
- Dave Scotese
- David Bakin
- David Gumberg
- dergoegge
- Dhruv Mehta
- Dimitri
- Dimitris Tsapakidis
- dontbyte
- dougEfish
- Douglas Chimento
- Duncan Dean
- ekzyis
- Elichai Turkel
- Ethan Heilman
- eugene
- Eunoia
- Fabian Jahr
- FractalEncrypt
- furszy
- Gleb Naumenko
- glozow
- Greg Weber
- Gregory Sanders
- gruve-p
- Hennadii Stepanov
- hernanmarino
- hiago
- Igor Bubelov
- ishaanam
- ismaelsadeeq
- Jacob P.
- Jadi
- James O'Beirne
- Janna
- Jarol Rodriguez
- Jeff Ruane
- Jeffrey Czyz
- Jeremy Rand
- Jeremy Rubin
- Jesse Barton
- jessebarton
- João Barbosa
- JoaoAJMatos
- John Moffett
- John Newbery
- Jon Atack
- Jonas Schnelli
- jonatack
- Joshua Kelly
- Josiah Baker
- josibake
- Juan Pablo Civile
- Karl-Johan Alm
- kdmukai
- KevinMusgrave
- kevkevin
- Kiminuo
- klementtan
- Kolby Moroz Liebl
- kouloumos
- Kristaps Kaupe
- Larry Ruane
- Leonardo Araujo
- Leonardo Lazzaro
- Luke Dashjr
- MacroFake
- Marnix
- Martin Leitner-Ankerl
- Martin Zumsande
- Matias Furszyfer
- Matt Whitlock
- Matthew Zipkin
- Michael Dietz
- Michael Folkson
- Michael Ford
- Miles Liu
- mruddy
- Murch
- Murray Nesbitt
- mutatrum
- muxator
- omahs
- Oskar Mendel
- Pablo Greco
- pablomartin4btc
- pasta
- Patrick Strateman
- Pavol Rusnak
- Peter Bushnell
- phyBrackets
- Pieter Wuille
- practicalswift
- Pttn
- Randall Naar
- Randy McMillan
- Riahiamirreza
- Robert Spigler
- Roman Zeyde
- Russell O'Connor
- Ryan Ofsky
- S3RK
- Samer Afach
- Sebastian Falbesoner
- Seibart Nedor
- Shashwat
- sinetek
- Sjors Provoost
- Skuli Dulfari
- Smlep
- sogoagain
- SomberNight
- Stacie Waleyko
- stickies-v
- stratospher
- Stéphan Vuylsteke
- Suhail Saqan
- Suhas Daftuar
- Suriyaa Sundararuban
- t-bast
- TakeshiMusgrave
- TheCharlatan
- Vasil Dimov
- Vasil Stoyanov
- virtu
- W. J. van der Laan
- w0xlt
- whiteh0rse
- Will Clark
- William Casarin
- Yancy Ribbens
- Yusuf Sahin HAMZA

As well as to everyone that helped with translations on
[Transifex](https://www.transifex.com/bitcoin/bitcoin/).
