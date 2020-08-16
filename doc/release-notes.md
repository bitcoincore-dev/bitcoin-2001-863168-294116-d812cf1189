0.20.1 Release Notes
====================

Bitcoin Knots version *0.20.1.knots20200815* is now available from:

  <https://bitcoinknots.org/files/0.20.x/0.20.1.knots20200815/>

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
installer (on Windows) or just copy over `/Applications/Bitcoin-Qt` (on Mac)
or `bitcoind`/`bitcoin-qt` (on Linux).

Upgrading directly from a version of Bitcoin Knots that has reached its EOL is
possible, but it might take some time if the data directory needs to be migrated. Old
wallet versions of Bitcoin Knots are generally supported.

Compatibility
==============

Bitcoin Knots is supported on operating systems using the Linux kernel,
macOS 10.12+, and Windows 7 and newer. It is not recommended to use
Bitcoin Knots on unsupported systems.

From Bitcoin Knots 0.20.0 onwards, macOS versions earlier than 10.12 are no
longer supported. Additionally, Bitcoin Knots does not yet change appearance
when macOS "dark mode" is activated.

Users running macOS Catalina should "right-click" and then choose "Open" to
open the Bitcoin Knots .dmg. This is due to new signing requirements and
privacy violations imposed by Apple, which the Bitcoin Knots project does not
implement.

Notable changes
===============

Changes regarding misbehaving peers
-----------------------------------

Peers that misbehave (e.g. send us invalid blocks) are now referred to as
discouraged nodes in log output, as they're not (and weren't) strictly banned:
incoming connections are still allowed from them, but they're preferred for
eviction.

Furthermore, a few additional changes are introduced to how discouraged
addresses are treated:

- Discouraging an address does not time out automatically after 24 hours
  (or the `-bantime` setting). Depending on traffic from other peers,
  discouragement may time out at an indeterminate time.

- Discouragement is not persisted over restarts.

- There is no method to list discouraged addresses. They are not returned by
  the `listbanned` RPC. That RPC also no longer reports the `ban_reason`
  field, as `"manually added"` is the only remaining option.

- Discouragement cannot be removed with the `setban remove` RPC command.
  If you need to remove a discouragement, you can remove all discouragements by
  stop-starting your node.

Other P2P/network changes
-------------------------

- If the node's network memory has been initialised sufficiently due to prior
  usage, DNS seeds will not be looked up unless the node cannot find a
  connection for 5 minutes (this used to  be 11 seconds) to improve privacy.

- A new `-networkactive` command-line option has been added to start the node
  with network activity supported, but disabled. The `setnetworkactive` RPC
  can later be used to turn network activity on as desired.

- IDs of transactions with policy-rejected inputs (such as unrecognised
  witness versions) will be added to the "reject filter" to avoid
  redownloading them unnecessarily.

GUI changes
-----------

- The debug window's peer list now shows granular permissions granted rather
  than just the old "whitelisted" flag.

New RPCs
--------

- Experimental support for "prune locks" has been added, enabling external
  applications to pause pruning of specific block ranges. These can be
  controlled using the new `setprunelock` RPC method, and listed with
  `listprunelocks`. Note that prune locks saved to disk could possibly
  conflict with future versions of Bitcoin Core or Knots, should they be
  rejected or substantially changed before becoming final. Should this occur,
  simply delete all persistant prune locks before upgrading.

- The `getindexinfo` RPC returns the actively running indices of the node,
  including their current sync status and height. It also accepts an `index_name`
  to specify returning only the status of that index. (#19550)

Updated RPCs
------------

- `getnetworkinfo` provides new `connections_in` and `connections_out` fields
  corresponding to the number of inbound and outbound peer-to-peer network
  connections. These new fields are in addition to the `connections` field that
  returns the total number. (#19405)

CLI
---

- The `connections` field of `bitcoin-cli -getinfo` is expanded to return a JSON
  object with `in`, `out` and `total` peer-to-peer network connections. It
  previously returned an integer value for the number of connections. (#19405)

- A new `bitcoin-cli -netinfo` option has been added to display a summary of
  and/or detailed information about your node's p2p connections.

0.20.1 change log
=================

Detailed release notes follow. This overview includes changes that affect
behavior, not code moves, refactors and string updates. Changes specific to
Bitcoin Knots (beyond Core) are flagged with an asterisk ('*') before the
description.

### Block and transaction handling
- n/a    *Update checkpoints and chain params, adding a new checkpoint at block 643,700 (luke-jr)

### P2P protocol and network code
- #19219 Replace automatic bans with discouragement filter (sipa)
- #16939 *DNS seeds: wait for 5m instead of 11s if 1000+ peers are known (ajtowns)
- #19473 *Add -networkactive option (hebasto)
- #19620 *Add txids with non-standard inputs to reject filter (sdaftuar, instagibbs)

### Wallet
- #19300 Handle concurrent wallet loading (promag)
- #18095 *Fix crashes and infinite loop in ListWalletDir() (uhliksk)
- #18850 *Fix ZapSelectTx to sync wallet spends (Anthony Fieroni)
- #19419 *let Listwalletdir not iterate through our blocksdata (saibato)
- #19502 *Bugfix: Wallet: Soft-fail exceptions within ListWalletDir file checks (luke-jr)

### RPC and other APIs
- #19517 psbt: Increment input value sum only once per UTXO in decodepsbt (achow101)
- #19362 *reset scantxoutset progress on finish (Pavol Rusnak)
- #19328 *Add gettxoutsetinfo hash_type option (fjahr)
- #19405 *add network in/out connections to `getnetworkinfo` and `-getinfo` (jonatack)
- #19463 *Prune locks (luke-jr)
- #19550 *Add getindexinfo RPC (fjahr)

### GUI
- #19059 update Qt base translations for macOS release (fanquake)
- gui#20 *Wrap tooltips in the intro window (hebasto)
- gui#39 *Add visual accenting for the 'Create new receiving address' button (hebasto)
- gui#43 *Call setWalletActionsEnabled(true) only for the first wallet (hebasto)
- #15202 *Add Close All Wallets action (promag)
- #18027 *PSBT: Make sure all existing signatures are fully combined before checking for completeness (gwillen)
- gui#6  *Do not truncate node flag strings in debugwindow.ui peers details tab (saibato)
- gui#34 *Show permissions instead of whitelisted (laanwj)

### Build system
- #19152 improve build OS configure output (skmcontrib)
- #19536 qt, build: Fix QFileDialog for static builds (hebasto)
- #19403 *build: improve __builtin_clz* detection (fanquake)
- #19375 *pass _WIN32_WINNT=0x0601 when building libevent for Windows (fanquake)
- #19493 *Fix clang build in Mac (Anthony Fieroni)
- #19525 *add -Wl,-z,separate-code to hardening flags (fanquake)

### Tests and QA
- #19444 Remove cached directories and associated script blocks from appveyor config (sipsorcery)
- #18640 appveyor: Remove clcache (MarcoFalke)

### Miscellaneous
- #19194 util: Don't reference errno when pthread fails (miztake)
- #18700 Fix locking on WSL using flock instead of fcntl (meshcollider)
- #19643 *Add `-netinfo` peer connections dashboard (jonatack)

Credits
=======

Thanks to everyone who directly contributed to this release:

- Aaron Clauson
- Andrew Chow
- Anthony Fieroni
- Anthony Towns
- Fabian Jahr
- fanquake
- Glenn Willen
- Gregory Sanders
- Hennadii Stepanov
- João Barbosa
- Jon Atack
- Luke Dashjr
- MarcoFalke
- MIZUTA Takeshi
- Pavol Rusnak
- Pieter Wuille
- sachinkm77
- saibato
- Samuel Dobson
- Suhas Daftuar
- uhliksk
- Wladimir J. van der Laan

As well as to everyone that helped with translations on
[Transifex](https://www.transifex.com/bitcoin/bitcoin/).
