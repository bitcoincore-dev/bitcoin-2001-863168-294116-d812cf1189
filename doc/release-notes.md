27.1 Release Notes
=====================

Bitcoin Knots version 27.1.knots20240621 is now available from:

  <https://bitcoinknots.org/files/27.x/27.1.knots20240621/>

This release includes new features, various bug fixes, and performance
improvements.

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
`-bytespersigop` policy) or datacarrier penalties (ie, `-datacarriercost`).
This could result in reporting a lower virtual size than is actually used for
mempool or mining purposes.

Due to disruption of the shared Bitcoin Transifex repository, this release
still does not include updated translations, and Bitcoin Knots may be unable
to do so until/unless that is resolved.

Notable changes
===============

P2P and network changes
-----------------------

- As it has been several years since the last vulnerability in the port
  mapping frameworks used by Bitcoin Knots, both UPnP and NAT-PMP have been
  re-enabled by default when your node is configured to listen for incoming
  connections (which is also on by default). This helps automatically
  configure routers and VPNs to not block those incoming connections, making
  the network more resilient. If you wish to disable these, you can set
  `-natpmp=0` and `-upnp=0` - or you may prefer to disable incoming
  connections altogether with `-listen=0`.

- Network-adjusted time has been removed from consensus code. It is replaced
  with (unadjusted) system time. The warning for a large median time offset
  (70 minutes or more) is kept. This reduces the implicit security assumption of
  requiring an honest majority of outbound peers, and increases the importance
  of the node operator ensuring their system time is (and stays) correct to not
  fall out of consensus with the network. (#28956)

Node policy changes
-------------------

- Experimental support for Opt-in Topologically Restricted Until Confirmation
  (TRUC) Transactions policy (aka v3 transaction policy) is available. By
  setting the transaction version number to 3, TRUC transactions request some
  stricter relay filtering of themselves and related transactions. By default,
  Bitcoin Knots v27.1 will accept, relay, and mine v3 transactions without
  these additional restrictions. If you have configured `-mempoolreplacement`
  to require transactions to signal replacability, v3 transactions are
  considered replacable (as this is an explicit part of the TRUC request).
  The `-mempooltruc` option can be set to either `enforce` to impose the other
  requested restrictions, or to `reject` to restore the previous policy of
  rejecting v3 transactions entirely. (#28948)

Wallet
------

- The CoinGrinder coin selection algorithm has been introduced to mitigate unnecessary
  large input sets and lower transaction costs at high feerates. CoinGrinder
  searches for the input set with minimal weight. Solutions found by
  CoinGrinder will produce a change output. CoinGrinder is only active at
  elevated feerates (default: 30+ sat/vB, based on `-consolidatefeerate`×3). (#27877)

Mining
------

- The `getblocktemplate` RPC method's template request mode now accepts new
  parameters "blockmaxsize", "blockmaxweight", and "minfeerate" to override
  the configured parameters on a call-by-call basis. Setting these differently
  from their defaults will disable the internal template caching for now, so
  may be less efficient if you have multiple applications using
  getblocktemplate directly.

- `getblocktemplate` (template request mode) also will now accept
  "skip_validity_test" in the list of client "capabilities". If this is
  specified, the internal template validity safety check is skipped, and the
  new template (if one isn't already cached) will not be cached for future
  calls. It is recommended that this feature is not used unless you plan to
  follow up with a template proposal getblocktemplate call (defined in BIP 23).

Updated RPCs
------------

- `disconnectnode` now supports disconnecting by IP (without specifying a port
  number or subnet).

- `getrawaddrman` will no longer include a dummy "mapped_as" nor
  "source_mapped_as" when an AS map has not been enabled.

Updated settings
----------------

- The `rpccookieperms` setting has been simplified to values "owner", "group",
  or "all". The old octal permissions may still be used, but are deprecated
  and may be removed in a future version.

mempool.dat compatibility
-------------------------

- The `mempool.dat` file created by -persistmempool or the savemempool RPC will
  be written in a new format. This new format includes the obfuscation of
  transaction contents to mitigate issues where external programs (such as
  anti-virus) attempt to interpret and potentially quarantine the file.

  This new format can not be read by previous software releases. To allow for a
  downgrade, a temporary setting `-persistmempoolv1` has been added to fall back
  to the legacy format. (#28207)

Build System
------------

- A C++20 capable compiler is now required to build Bitcoin Knots. (#28349)

Credits
=======

Thanks to everyone who directly contributed to this release:

- 22388o⚡️
- Aaron Clauson
- Amiti Uttarwar
- Andrew Toth
- Anthony Towns
- Antoine Poinsot
- Ava Chow
- Brandon Odiwuor
- brunoerg
- Chris Stewart
- Cory Fields
- dergoegge
- djschnei21
- Fabian Jahr
- fanquake
- furszy
- Gloria Zhao
- Greg Sanders
- Hennadii Stepanov
- Hernan Marino
- iamcarlos94
- ismaelsadeeq
- Jadi
- Jameson Lopp
- Jesse Barton
- John Moffett
- Jon Atack
- josibake
- jrakibi
- Justin Dhillon
- Kashif Smith
- kevkevin
- Kristaps Kaupe
- L0la L33tz
- Luke Dashjr
- Lőrinc
- marco
- MarcoFalke
- Mark Friedenbach
- Marnix
- Martin Leitner-Ankerl
- Martin Zumsande
- Matt Corallo
- Max Edwards
- Murch
- muxator
- naiyoma
- nanlour
- Nikodemas Tuckus
- ns-xvrn
- pablomartin4btc
- Peter Todd
- Pieter Wuille
- Richard Myers
- Roman Zeyde
- Ryan Ofsky
- Sebastian Falbesoner
- Sergi Delgado Segura
- Sjors Provoost
- stickies-v
- stratospher
- Suhas Daftuar
- Supachai Kheawjuy
- TheCharlatan
- UdjinM6
- Vasil Dimov
- w0xlt
- willcl-ark
- Wladimir J. van der Laan
