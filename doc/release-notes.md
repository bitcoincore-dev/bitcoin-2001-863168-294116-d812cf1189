26.2rc1 Release Notes
==================

Bitcoin Knots version 26.2rc1.knotsCHANGEME is now available from:

  <https://bitcoinknots.org/files/26.x/26.1.knotsCHANGEME/>

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

Due to disruption of the shared Bitcoin Transifex repository, this release
still does not include updated translations, and Bitcoin Knots may be unable
to do so until/unless that is resolved.

Notable changes
===============

Node policy changes
-------------------

- A new non-standard token/asset protocol launched a few weeks ago ("Runes").
  Due to its lack of competent review, design flaws (as well as the relative
  worthlessness of the tokens at least when first minted) resulted in it being
  particularly spammy in practice. Some users have chosen to block all
  datacarrier transactions in an effort to mitigate this.

  To better address users' concerns, this release adds a new `-rejecttokens`
  policy filter (also available in the GUI) that will only block Runes
  transactions, thereby enabling users who choose to tolerate datacarrier
  otherwise to re-enable that policy. Note that it is _not_ enabled by
  default at this time.

- Similarly, a new policy filter has been added to block parasitic
  transactions. Many parasite transactions cannot be detected, but this new
  filter aims to do what it can when possible, currently just so-called
  "CAT-21" transactions built using the Ordinal attack. It _is_ enabled by
  default, and can be disabled using `-rejectparasites=0` (or in the GUI) if
  you wish to tolerate these. (knots#78)

- The dust limit has historically required outputs to be at least three times
  the value they provide when later spent. The experimental dynamic adjustment
  function, however, was adjusting it based on exactly (1x) the value the
  output provides. To address this, you can now specify a multiplier by
  prefixing your policy by a number (with up to three decimal places) followed
  by an asterisk. So `-dustdynamic=3.142*target:N` will require outputs to be
  3.142 times the value they provide; or `-dustdynamic=1*target:N` will behave
  the same as previous versions for `target:N`. The default multiplier, if
  none is specified, is now three times as historically has been used. If you
  use this feature, please leave a comment about your experience on GitHub:
  <https://github.com/bitcoinknots/bitcoin/issues/74>

Updated RPCs
------------
### Script

- #29853: sign: don't assume we are parsing a sane TapMiniscript

### P2P and network changes

- `submitpackage` now returns results with per-transaction errors rather than
  throwing an exception potentially obscuring other transactions being
  accepted. (#28848)

- `getprioritisedtransactions` now also includes "modified_fee" in results,
  with the sum of the real transaction fee and the fee delta. (#28885)

- `getrawaddrman` has been extended to include in the results for each entry
  "mapped_as" and "source_mapped_as". (#30062)

- #29691: Change Luke Dashjr seed to dashjr-list-of-p2p-nodes.us
- #30085: p2p: detect addnode cjdns peers in GetAddedNodeInfo()

### RPC

- #29869: rpc, bugfix: Enforce maximum value for setmocktime
- #28554: bugfix: throw an error if an invalid parameter is passed to getnetworkhashps RPC
- #30094: rpc: move UniValue in blockToJSON
- #29870: rpc: Reword SighashFromStr error message

### Build

- #29747: depends: fix mingw-w64 Qt DEBUG=1 build
- #29985: depends: Fix build of Qt for 32-bit platforms with recent glibc
- #30151: depends: Fetch miniupnpc sources from an alternative website

### Misc

- #29776: ThreadSanitizer: Fix #29767
- #29856: ci: Bump s390x to ubuntu:24.04
- #29764: doc: Suggest installing dev packages for debian/ubuntu qt5 build
- #30149: contrib: Renew Windows code signing certificate

Credits
=======

Thanks to everyone who directly contributed to this release:

- Antoine Poinsot
- Ava Chow
- brunoerg
- Charlie
- dergoegge
- Fabian Jahr
- fanquake
- furszy
- Greg Sanders
- glozow
- Hennadii Stepanov
- Jameson Lopp
- Jon Atack
- kevkevin
- Léo Haf
- Luke Dashjr
- MarcoFalke
- Martin Zumsande
- Matthew Zipkin
- nanlour
- pablomartin4btc
- Sebastian Falbesoner
- Sergi Delgado Segura
- Vasil Dimov
- willcl-ark
- Wladimir J. van der Laan

As well as to everyone that helped with translations on
[Transifex](https://www.transifex.com/bitcoin/bitcoin/).
