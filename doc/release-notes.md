21.2 Release Notes
====================

Bitcoin Knots version *21.2.knots20210629* is now available from:

  <https://bitcoinknots.org/files/21.x/21.2.knots20210629/>

This Long Term Support (LTS) "Steel Rope" release is based on the unchanged
Bitcoin Knots feature set from 2021 June 29th, with only bug fixes and updated
translations.

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

Upgrading directly from very old versions of Bitcoin Core or Knots is
possible, but it might take some time if the data directory needs to be migrated. Old
wallet versions of Bitcoin Knots are generally supported.

Compatibility
==============

Bitcoin Knots is supported on operating systems using the Linux kernel,
macOS 10.12+, and Windows 7 and newer. It is not recommended to use
Bitcoin Knots on unsupported systems.

From Bitcoin Knots 0.20.0 onwards, macOS versions earlier than 10.12 are no
longer supported.

Users running macOS Catalina/10.15 or newer should "right-click" and then
choose "Open" to start Bitcoin Knots .dmg. This is due to new signing
requirements and privacy violations imposed by Apple, which the Bitcoin
Knots project does not implement.

Notable changes
===============

Removed RPC
-----------

- Due to unresolved bugs, the `removeaddress` RPC (introduced experimentally
  in v0.21.1) had to be removed.

Updated settings
----------------

- In previous releases, the meaning of the command line option
  `-persistmempool` (without a value provided) incorrectly disabled mempool
  persistence.  `-persistmempool` is now treated like other boolean options to
  mean `-persistmempool=1`. Passing `-persistmempool=0`, `-persistmempool=1`
  and `-nopersistmempool` is unaffected. (#23061)

- Multiple commands can be specified for the `-*notify` options, and all will
  be executed. Formerly, only the prevailing command in order of option
  precedence would run. (#22372)

Low-level changes
=================

RPC
---

- `estimatesmartfee` will no longer return values lower than the current
  minimum accepted fee for relaying. (#22722)

0.21.2 change log
=================

Detailed release notes follow. This overview includes changes that affect
behavior, not code moves, refactors and string updates. Changes specific to
Bitcoin Knots (beyond Core) are flagged with an asterisk ('*') before the
description.

### Consensus

- n/a *Update checkpoints and chain params, adding a new checkpoint at block 714,000 (luke-jr)

### Mining

- #23050 *Fix BlockAssembler::AddToBlock, CTxMemPool::PrioritiseTransaction logging (jonatack)

### Block and transaction handling

- #19851 *refactor: Extract ParseOpCode from ParseScript (promag)
- #22875 *Fix Racy ParseOpCode function initializatio (jeremyrubin)
- #22895 *don't call GetBlockPos in ReadBlockFromDisk without lock (jonatack)
- #23061 *Fix (inverse) meaning of -persistmempool (MarcoFalke)

### P2P protocol and network code

- #22569 Rate limit the processing of rumoured addresses (sipa)
- #22834 *respect -onlynet= when making outbound connections (vasild)
- #22879 *addrman: Fix format string in deserialize error (MarcoFalke)
- #15421 *TorController: Close non-std fds when execing tor slave (luke-jr)

### Wallet

- #22359 *Properly set fInMempool in mempool notifications (MarcoFalke)
- #22781 *fix the behavior of IsHDEnabled, return false in case of a blank hd wallet (Saibato)
- #23106 *Ensure wallet is unlocked before signing PSBT with walletprocesspsbt and GUI (meshcollider)
- #23644 *Replace confusing getAdjustedTime() with GetTime() (MarcoFalke)

### RPC and other APIs

- #19361 Reset scantxoutset progress before inferring descriptors (prusnak)
- #22417 *util/system: Close non-std fds when execing slave processes (luke-jr)
- #22722 *update estimatesmartfee rpc to return max of estimateSmartFee, mempoolMinFee and minRelayTxFee (pranabp-bit)
- #23348 *wallet: Do not return "keypoololdest" for blank descriptor wallets (hebasto)
- #12677 *Elaborate docs for ancestor{count,size,fees} in listunspent output (luke-jr)
- #15836 *Bugfix: RPC/client: getmempoolinfo's deprecated with_fee_histogram param should be converted as JSON (luke-jr)
- #20556 *Properly document return values (submitblock, gettxout, getblocktemplate, scantxoutset) (MarcoFalke)
- #21422 *Update docs for getmempoolinfo's fee_histogram results (kiminuo)
- #22798 *doc: Fix RPC result documentation (MarcoFalke)
- #22918 *Update docs for getblock verbosity level 3 (kiminuo)
- #23139 *fix "trusted" and "generated" descriptions in TransactionDescriptionString (jonatack)
- #17034 *Bugfix: PSBT: Check the actual version number against PSBT_HIGHEST_VERSION (achow101)
- #17034 *Bugfix: PSBT: Correct operator< logic for CExtPubKey (achow101)
- #17631 *REST: Use ParseUInt32 for parsing blockfilterheaders count param to catch invalid strings (luke-jr)
- #20295 *getblockfrompeer may succeed (with a warning) if we already have the block (Sjors)
- #20295 *Ensure getblockfrompeer errors if the peer doesn't exist, even if we already have the block (luke-jr)
- #23706 *Forward compatibility with getblockfrompeer param name changing to peer_id (Sjors)
- #21260 *Actually document meaning of "in_mempool" value (luke-jr)
- #23549 *scanblocks: Document possible return values and status key when no ongoing scan (jamesob)
- #23634 *add missing scantxoutset examples (theStack)
- #15129 *Wallet: Disable removeaddress since it's broken (luke-jr)
- #22372 *Support multiple -*notify commands (luke-jr)
- #9152 *Document sweepprivkeys result (luke-jr)
- #20551 *net: Correct documented type for connection_type parameter to addnode (luke-jr)
- #7533 *Correct documented type of reject_reason parameters (String) (luke-jr)

### Build System

- #21932 depends: update Qt 5.9 source url (kittywhiskers)
- #22017 Update Windows code signing certificate (achow101)
- #22191 Use custom MacOS code signing tool (achow101)
- #22713 Fix build with Boost 1.77.0 (sizeofvoid)
- #21882 *Fix undefined reference to __mulodi4 (update) (hebasto)
- #23045 *Restrict check for CRC32C intrinsic to aarch64 (laanwj)
- #23182 *configure: Accept python3.9-3.11 as valid Python programs (ajtowns, fanquake)
- #23314 *explicitly disable libsecp256k1 openssl based tests (fanquake)
- #23345 *Drop unneeded dependencies for bitcoin-wallet tool (hebasto)
- #22348 *Fix Boost Process compatibility with mingw-w64 compiler (hebasto)
- #23607 *Adapt to compatibility break in bleeding edge libevent (Perlover)
- n/a *Bugfix: build: Adapt out-of-tree qrc to movies->animation rename (luke-jr)

### Tests and QA

- #20182 ci: Build with --enable-werror by default, and document exceptions (hebasto)
- #20535 Fix intermittent feature_taproot issue (MarcoFalke)
- #21663 Fix macOS brew install command (hebasto)
- #22730 Run fuzzer task for the master branch only (hebasto)
- #23027 *Bugfix: Skip tests for tools not being built (luke-jr)
- #22693 *wallet_basic: Test use_txids in getaddressinfo result (luke-jr)
- #20391 *add more functional tests for setfeerate (jonatack)
- #23139 *add listtransactions/listsinceblock "trusted" coverage (jonatack)
- #23716 *replace hashlib.ripemd160 with an own implementation (sipa)

### GUI

- gui#277 Do not use QClipboard::Selection on Windows and macOS. (hebasto)
- gui#365 Draw "eye" sign at the beginning of watch-only addresses (hebasto)
- gui#121 *Early subscribe core signals in transaction table model (promag)
- gui#379 *Prompt to reset settings when settings.json cannot be read (ryanofsky)
- gui#399 *Add Load PSBT functionaliy with nowallet (psancheti110)
- gui#409 *Fix window title of wallet loading window (shaavan)
- gui#418 *fix bitcoin-qt app categorization on apple silicon (jarolrod)
- gui#430 *Improvements to the open up transaction in third-party link action (jarolrod)
- gui#439 *Do not show unused widgets at startup (hebasto)
- gui#154 *Support macOS Dark mode (goums)
- gui#319 *Handle palette changes in Paste button in Open URI dialog (kristapsk)
- gui#149 *Intro: Use QFontMetrics::horizontalAdvance in Qt 5.11+ rather than deprecated QFontMetrics::width (luke-jr)
- gui#363 *Bugfix: Peers: Use translated strings to calculate column widths (luke-jr)
- gui#309 *Use Qt5.5-compatible syntax for network icon menu actions (luke-jr)
- gui#318 *Peers: Use Qt5.5-compatible syntax for "Copy address" menu action (luke-jr)
- gui#444 *Bugfix: NetWatch: Check if m_log is full before doing overwriting of deleted rows (luke-jr)
- gui#444 *NetWatch: Update for Bech32 (luke-jr)
- gui#444 *NetWatch: Use a softer/brighter green for active search field background colour (luke-jr)
- gui#469 *Load Base64 PSBT string from file (achow101)
- gui#506 *QRImageWidget: Sizing and font fixups (luke-jr)
- #15428 *Bugfix: Pairing: Don't try to add layout to the wrong parent even temporarily (luke-jr)
- gui#444 *Group "Watch network activity" and "Mempool Statistics" menu entries together nicely (luke-jr)
- #929 *Fix comparison of character size for Tonal font detection (luke-jr)
- n/a *Bugfix: Peers: When sorting by address, sort by network first \[correctly\] (luke-jr)

### Miscellaneous

- #22390 *system: skip trying to set the locale on NetBSD (fanquake)
- #2241 *dbwrapper: Be stricter about build/runtime LevelDB version check in SanityCheck (luke-jr)
- #2241 *Use internal LevelDB test interface to bump system LevelDB mmap limit up to 4096 like Bitcoin Core's fork (luke-jr)
- #22577 *Close minor startup race between main and scheduler threads (LarryRuane)
- #22591 *error if settings.json exists, but is unreadable (tylerchambers)
- n/a *bitcoin.svg: Re-colour rope from brown to silver ("Steel Rope") for LTS (luke-jr)
- #20223 *Drop the leading 0 from the version number (achow101)


Credits
=======

Thanks to everyone who directly contributed to this release:

- Andrew Chow
- Anthony Towns
- fanquake
- Hennadii Stepanov
- James O'Beirne
- Jarol Rodriguez
- Jeremy Rubin
- João Barbosa
- Jon Atack
- Kiminuo
- Kittywhiskers Van Gogh
- Kristaps Kaupe
- Larry Ruane
- Luke Dashjr
- MarcoFalke
- Pavol Rusnak
- Perlover
- Pieter Wuille
- pranabp-bit
- Prateek Sancheti
- Rafael Sadowski
- Russell Yanofsky
- Saibato
- Samuel Dobson
- Sebastian Falbesoner
- Shashwat
- Sjors Provoost
- Sylvain Goumy
- Tyler Chambers
- Vasil Dimov
- W. J. van der Laan

As well as to everyone that helped with translations on
[Transifex](https://www.transifex.com/bitcoin/bitcoin/).
