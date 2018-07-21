Bitcoin Knots version *0.16.1.knots20180713* is now available from:

  <https://bitcoinknots.org/files/0.16.x/0.16.1.knots20180713/>

This is a new minor version release, including new features, various bugfixes
and performance improvements, as well as updated translations.

Please report bugs using the issue tracker at GitHub:

  <https://github.com/bitcoinknots/bitcoin/issues>

How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has completely
shut down (which might take a few minutes for older versions), then run the
installer (on Windows) or just copy over `/Applications/Bitcoin-Qt` (on Mac)
or `bitcoind`/`bitcoin-qt` (on Linux).

The first time you run version 0.15.0 or newer, your chainstate database will be converted to a
new format, which will take anywhere from a few minutes to half an hour,
depending on the speed of your machine.

Note that the block database format also changed in version 0.8.0 and there is no
automatic upgrade code from before version 0.8 to version 0.15.0 or higher. Upgrading
directly from 0.7.x and earlier without re-downloading the blockchain is not supported.
However, as usual, old wallet versions are still supported.

Downgrading warning
-------------------

Wallets created in 0.16 and later are not compatible with versions prior to 0.16
and will not work if you try to use newly created wallets in older versions. Existing
wallets that were created with older versions are not affected by this.

Compatibility
==============

Bitcoin Knots is supported on multiple operating systems using the Linux kernel,
macOS 10.8+, and Windows Vista and later. Windows XP is not supported.

Bitcoin Knots should also work on most other Unix-like systems but is not
frequently tested on them.

Notable changes
===============

RPC changes
-----------

### RPC method whitelists

RPC method whitelists have been added to help enforce application policies for
services being built on top of Bitcoin Knots (e.g., your Lightning Node maybe
shouldn't be adding new peers). The aim of this feature is not to make it
advisable to connect your Bitcoin node to arbitrary services, but to reduce
risk and prevent unintended access.

### Removal of premature PSBT support

BIP 174 has been modified, making it incompatible with the premature PSBT
support in v0.16.0. The new PSBT code is too invasive to backport, so the PSBT
RPC methods have simply been removed for now.

### Low-level changes

- JSON transaction decomposition now includes a `weight` field which provides
  the transaction's exact weight. This is included in REST /rest/tx/ and
  /rest/block/ endpoints when in json mode. This is also included in `getblock`
  (with verbosity=2), `listsinceblock`, `listtransactions`, and
  `getrawtransaction` RPC commands.
- JSON mempool entry descriptions now include the `bip125-replaceable` field,
  showing if enabling the replace-by-fee feature is requested for them.
- When known, `signrawtransaction` may now include fee information. (Note that
  sometimes it is not known.)
- New setscriptthreadsenabled/scriptthreadsinfo methods have been added to
  give more control over block verification threads.

0.16.1 Change log
=================

Detailed release notes follow. This overview includes changes that affect
behavior, not code moves, refactors and string updates. For convenience in
locating the code changes and accompanying discussion, both the pull request
and git merge commit are mentioned. Changes specific to Bitcoin Knots (beyond
Core) are flagged with an asterisk ('*') before the description.

### Policy
- #11423 `d353dd1` [Policy] Several transaction standardness rules (jl2012)

### Mining
- #12743 `d8caa5e` *Fix csBestBlock/cvBlockChange waiting in rpc/mining (sipa)

### RPC and other APIs
- #13084 `a01e1c3` *Do not turn OP_1NEGATE in scriptSig into 0x0181 in signing code (sipa)
- #12196 `e03f77f` *scantxoutset: add support for scripts (Jonas Schnelli)
- n/a    `c24f202` *Remove premature PSBT support (luke-jr)
- #12676 `12facda` *RPC/Blockchain: Add "bip125-replaceable" to mempool entry data (dexX7)
- #12763 `338deae` *Add RPC method Whitelist Feature (JeremyRubin)
- #12778 `39992b5` *Add username and ip logging for RPC method requests (Gabriel Davidian)
- #12791 `401874c` *Expose a transaction's weight via RPC (TheBlueMatt)
- #12911 `375f14e` *wallet: Show fee in results for signrawtransaction when known (Karl-Johan Alm)
- #12965 `52fdced` *Add RPC call setscriptthreadsenabled/scriptthreadsinfo: allow to disable verification threads (Jonas Schnelli)
- #13452 `c3f8bdc` *have verifytxoutproof check the number of txns in proof structure (Gregory Sanders)

### Block and transaction handling
- #13199 `c71e535` Bugfix: ensure consistency of m_failed_blocks after reconsiderblock (sdaftuar)
- #13023 `bb79aaf` Fix some concurrency issues in ActivateBestChain() (skeees)
- #12696 `b244951` *Fix possible data race when committing block files (Evan Klitzke)
- #12653 `5e88c40` *Allow to specify the directory for the blocks storage with -blocksdir (Jonas Schnelli)
- #13191 `0e55d8c` *4-way SSE4.1 implementation for double SHA256 on 64-byte inputs (sipa)
- #13191 `b47a7dd` *8-way AVX2 implementation for double SHA256 on 64-byte inputs (sipa)
- #13386 `7ca060a` *Add SHA256 implementation using using Intel SHA intrinsics (sipa)
- #13203 `0f15e67` *Add POWER8 vector impl for 4-way SHA256 (TheBlueMatt)

### P2P protocol and network code
- #12626 `f60e84d` Limit the number of IPs addrman learns from each DNS seeder (EthanHeilman)
- #13134 `92b4187` *net: Add option `-enablebip61` to configure sending of BIP61 notifications (laanwj)
- #13151 `09b8192` *net: Serve blocks directly from disk when possible (laanwj)

### Wallet
- #13265 `5d8de76` Exit SyncMetaData if there are no transactions to sync (laanwj)
- #13030 `5ff571e` Fix zapwallettxes/multiwallet interaction. (jnewbery)
- n/a    `fcacaad` *net_processing: Optimise wallet beancounting (avoid locks) (luke-jr)
- #13339 `84d3c0e` *wallet: Replace %w by wallet name in -walletnotify script (João Barbosa)
- #13437 `53f5cdb` *wallet: Erase wtxOrderd wtx pointer on removeprunedfunds (MarcoFalke)

### GUI
- #12999 `1720eb3` Show the Window when double clicking the taskbar icon (ken2812221)
- #12650 `f118a7a` Fix issue: "default port not shown correctly in settings dialog" (251Labs)
- #12854 `b63f23c` *Add P2P, Network, and Qt categories to the desktop icon (luke-jr)
- #12617 `fb79554` *gui: Show messages as text not html (laanwj)
- #12793 `0bcc074` *qt: Avoid resetting on resetguisettings=0 (MarcoFalke)
- #8550  `450ac60` *Bugfix: GUI/MempoolStats: Avoid dereferencing NULL (luke-jr)
- #12616 `a0a60fd` *qt: Set modal overlay hide button as default (João Barbosa)
- #12621 `ce6fe4e` *qt: Avoid querying unnecessary model data when filtering transactions (João Barbosa)
- #12818 `e3d0364` *TransactionView: highlight replacement tx after fee bump (Sjors Provoost)
- #13158 `48ea7c7` *[qt]: changes sendcoinsdialog's box layout for improved readability. (marcoagner)
- n/a    `12d2844` *contrib/debian: Use Knots icon for .desktop (luke-jr)

### Build system
- #12474 `b0f692f` Allow depends system to support armv7l (hkjn)
- #12585 `72a3290` depends: Switch to downloading expat from GitHub (fanquake)
- #12648 `46ca8f3` test: Update trusted git root (MarcoFalke)
- #11995 `686cb86` depends: Fix Qt build with Xcode 9 (fanquake)
- #12636 `845838c` backport: #11995 Fix Qt build with Xcode 9 (fanquake)
- #12946 `e055bc0` depends: Fix Qt build with XCode 9.3 (fanquake)
- #12998 `7847b92` Default to defining endian-conversion DECLs in compat w/o config (TheBlueMatt)
- #2241  `bb237b8` *configure: Accept memenv included in main LevelDB library, for system-leveldb (luke-jr)
- #2241  `afd7edb` *init: Sanity check LevelDB build/runtime versions (luke-jr)
- n/a    `94f46ba` *Backport changes from Matt Corallo's PPA (TheBlueMatt)

### Tests and QA
- #12447 `01f931b` Add missing signal.h header (laanwj)
- #12545 `1286f3e` Use wait_until to ensure ping goes out (Empact)
- #12804 `4bdb0ce` Fix intermittent rpc_net.py failure. (jnewbery)
- #12553 `0e98f96` Prefer wait_until over polling with time.sleep (Empact)
- #12486 `cfebd40` Round target fee to 8 decimals in assert_fee_amount (kallewoof)
- #12843 `df38b13` Test starting bitcoind with -h and -version (jnewbery)
- #12475 `41c29f6` Fix python TypeError in script.py (MarcoFalke)
- #12638 `0a76ed2` Cache only chain and wallet for regtest datadir (MarcoFalke)
- #12902 `7460945` Handle potential cookie race when starting node (sdaftuar)
- #12904 `6c26df0` Ensure bitcoind processes are cleaned up when tests end (sdaftuar)
- #13049 `9ea62a3` Backports (MarcoFalke)
- #13201 `b8aacd6` Handle disconnect_node race (sdaftuar)
- #13105 `ea15fd9` *Use --failfast when running functional tests on Travis (James O'Beirne)
- #13545 `07837ee` *Fix incorrect tests (practicalswift)
- #13300 `f3c7737` *qa: Initialize lockstack to prevent null pointer deref (MarcoFalke)
- #13304 `aede1a9` *qa: Fix wallet_listreceivedby race (MarcoFalke)
- #13192 `0a72b91` *[tests] Fixed intermittent failure in p2p_sendheaders.py. (lmanners)

### Miscellaneous
- #12518 `a17fecf` Bump leveldb subtree (MarcoFalke)
- #12442 `f3b8d85` devtools: Exclude patches from lint-whitespace (MarcoFalke)
- #12988 `acdf433` Hold cs_main while calling UpdatedBlockTip() signal (skeees)
- #12985 `0684cf9` Windows: Avoid launching as admin when NSIS installer ends. (JeremyRand)
- #13149 `ac0f63e` *Print a log message if we fail to shrink the debug log file (practicalswift)
- #13159 `448935e` *logging: Fix potential use-after-free in LogPrintStr(...) (practicalswift)
- #13159 `51d2202` *Don't close old debug log file handle prematurely when trying to re-open (on SIGHUP) (practicalswift)
- #12783 `2e2eb61` *macOS: Disable AppNap for macOS 10.9+ (Alexey Ivanov)
- #12887 `ca2b1b5` *[trivial] Add newlines to end of log messages. (jnewbery)

### Documentation
- #12637 `60086dd` backport: #12556 fix version typo in getpeerinfo RPC call help (fanquake)
- #13184 `4087dd0` RPC Docs: `gettxout*`: clarify bestblock and unspent counts (harding)
- #13246 `6de7543` Bump to Ubuntu Bionic 18.04 in build-windows.md (ken2812221)
- #12556 `e730b82` Fix version typo in getpeerinfo RPC call help (tamasblummer)

Credits
=======

Thanks to everyone who directly contributed to this release:

- 251
- Alexey Ivanov
- Ben Woosley
- Chun Kuan Lee
- David A. Harding
- dexX7
- e0
- Evan Klitzke
- fanquake
- Gabriel Davidian
- Gregory Sanders
- Henrik Jonsson
- James O'Beirne
- JeremyRand
- Jeremy Rubin
- Jesse Cohen
- João Barbosa
- John Newbery
- Johnson Lau
- Jonas Schnelli
- Karl-Johan Alm
- lmanners
- Luke Dashjr
- marcoagner
- MarcoFalke
- Matt Corallo
- Pieter Wuille
- practicalswift
- Sjors Provoost
- Suhas Daftuar
- Tamas Blummer
- Wladimir J. van der Laan

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/bitcoin/).
