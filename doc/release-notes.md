Bitcoin Knots version *0.16.2.knots20180730* is now available from:

  <https://bitcoinknots.org/files/0.16.x/0.16.2.knots20180730/>

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

- The new RPC `getzmqnotifications` returns information about active ZMQ
  notifications.

0.16.2 Change log
=================

Detailed release notes follow. This overview includes changes that affect
behavior, not code moves, refactors and string updates. For convenience in
locating the code changes and accompanying discussion, both the pull request
and git merge commit are mentioned. Changes specific to Bitcoin Knots (beyond
Core) are flagged with an asterisk ('*') before the description.

### Wallet
- #13622 `c04a4a5` Remove mapRequest tracking that just effects Qt display. (TheBlueMatt)
- #12905 `cfc6f74` [rpcwallet] Clamp walletpassphrase value at 100M seconds (sdaftuar)

### RPC and other APIs
- #13451 `cbd2f70` rpc: expose CBlockIndex::nTx in getblock(header) (instagibbs)
- #13507 `f7401c8` RPC: Fix parameter count check for importpubkey (kristapsk)
- #12837 `bf1f150` rpc: fix type mistmatch in `listreceivedbyaddress` (joemphilips)
- #13547 `1825e37` *Error on missing amount in signrawtransaction (ajtowns)
- #13608 `0fe5e01` *Require that input amount is provided for bitcoin-tx witness transactions (Empact)
- #13655 `1cdbea7` *bitcoinconsensus: invalid flags should be set to bitcoinconsensus_error type, add test cases covering bitcoinconsensus error codes (afk11)
- #13570 `b87d341` *RPC: Add new getzmqnotifications method. (domob1812)

### GUI
- #13537 `a6f1df8` *Show symbol for inbound/outbound in peer table (wodry)

### Validation
- n/a    `72a0cff` *Add a new checkpoint at block 532,480 and update statistics (luke-jr)

### Build system
- #13544 `9fd3e00` depends: Update Qt download url (fanquake)
- #13788 `e9064cf` *configure: Skip assembly support checks, when assembly is disabled (luke-jr)
- #13789 `bb9957c` *crypto/sha256: Use pragmas to enforce necessary intrinsics for GCC and Clang (luke-jr)
- n/a    `bd5c15a` *configure: Check assembler crc32 with user CXXFLAGS appended (luke-jr)

### Tests and QA
- #13061 `170b309` Make tests pass after 2020 (bmwiedemann)
- #13545 `e15e3a9` tests: Fix test case `streams_serializedata_xor` Remove Boost dependency. (practicalswift)

### Miscellaneous
- #13131 `ce8aa54` Add Windows shutdown handler (ken2812221)
- #13652 `20461fc` rpc: Fix that CWallet::AbandonTransaction would leave the grandchildren, etc. active (Empact)

Credits
=======

Thanks to everyone who directly contributed to this release:

- Anthony Towns
- Ben Woosley
- Bernhard M. Wiedemann
- Chun Kuan Lee
- Cory Fields
- Daniel Kraft
- fanquake
- Gregory Sanders
- joemphilips
- Kristaps Kaupe
- Luke Dashjr
- Matt Corallo
- practicalswift
- Suhas Daftuar
- Thomas Kerin
- wodry

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/bitcoin/).
