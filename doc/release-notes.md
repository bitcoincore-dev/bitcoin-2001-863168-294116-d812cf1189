Bitcoin Knots version 0.12.1.knots20160629 is now available from:

  <https://bitcoinknots.org/files/0.12.x/0.12.1.knots20160629/>

This is a new minor version release, including the BIP9, BIP68 and BIP112
softfork, various bugfixes and updated translations.

Please report bugs using the issue tracker at github:

  <https://github.com/bitcoinknots/bitcoin/issues>

Upgrading and downgrading
=========================

How to Upgrade
--------------

If you are running an older version, shut it down. Wait until it has completely
shut down (which might take a few minutes for older versions), then run the
installer (on Windows) or just copy over /Applications/Bitcoin-Qt (on Mac) or
bitcoind/bitcoin-qt (on Linux).

Downgrade warning
-----------------

### Downgrade to a version < 0.12.0

Because release 0.12.0 and later will obfuscate the chainstate on every
fresh sync or reindex, the chainstate is not backwards-compatible with
pre-0.12 versions of Bitcoin Core or other software.

If you want to downgrade after you have done a reindex with 0.12.0 or later,
you will need to reindex when you first start Bitcoin Core or Knots version
0.11.

Notable changes
===============

First version bits BIP9 softfork deployment
-------------------------------------------

This release includes a soft fork deployment to enforce [BIP68][],
[BIP112][] and [BIP113][] using the [BIP9][] deployment mechanism.

The deployment sets the block version number to 0x20000001 between
midnight 1st May 2016 and midnight 1st May 2017 to signal readiness for 
deployment. The version number consists of 0x20000000 to indicate version
bits together with setting bit 0 to indicate support for this combined
deployment, shown as "csv" in the `getblockchaininfo` RPC call.

For more information about the soft forking change, please see
<https://github.com/bitcoin/bitcoin/pull/7648>

This specific backport pull-request can be viewed at
<https://github.com/bitcoin/bitcoin/pull/7543>

For mining-specific changes, see
<https://github.com/bitcoin/bitcoin/pull/7935>

[BIP9]: https://github.com/bitcoin/bips/blob/master/bip-0009.mediawiki
[BIP68]: https://github.com/bitcoin/bips/blob/master/bip-0068.mediawiki
[BIP112]: https://github.com/bitcoin/bips/blob/master/bip-0112.mediawiki
[BIP113]: https://github.com/bitcoin/bips/blob/master/bip-0113.mediawiki

BIP68 soft fork to enforce sequence locks for relative locktime
---------------------------------------------------------------

[BIP68][] introduces relative lock-time consensus-enforced semantics of
the sequence number field to enable a signed transaction input to remain
invalid for a defined period of time after confirmation of its corresponding
outpoint.

For more information about the implementation, see
<https://github.com/bitcoin/bitcoin/pull/7184>

BIP112 soft fork to enforce OP_CHECKSEQUENCEVERIFY
--------------------------------------------------

[BIP112][] redefines the existing OP_NOP3 as OP_CHECKSEQUENCEVERIFY (CSV)
for a new opcode in the Bitcoin scripting system that in combination with
[BIP68][] allows execution pathways of a script to be restricted based
on the age of the output being spent.

For more information about the implementation, see
<https://github.com/bitcoin/bitcoin/pull/7524>

BIP113 locktime enforcement soft fork
-------------------------------------

Bitcoin Core 0.11.2 previously introduced mempool-only locktime
enforcement using GetMedianTimePast(). This release seeks to
consensus enforce the rule.

Bitcoin transactions currently may specify a locktime indicating when
they may be added to a valid block.  Current consensus rules require
that blocks have a block header time greater than the locktime specified
in any transaction in that block.

Miners get to choose what time they use for their header time, with the
consensus rule being that no node will accept a block whose time is more
than two hours in the future.  This creates a incentive for miners to
set their header times to future values in order to include locktimed
transactions which weren't supposed to be included for up to two more
hours.

The consensus rules also specify that valid blocks may have a header
time greater than that of the median of the 11 previous blocks.  This
GetMedianTimePast() time has a key feature we generally associate with
time: it can't go backwards.

[BIP113][] specifies a soft fork enforced in this release that
weakens this perverse incentive for individual miners to use a future
time by requiring that valid blocks have a computed GetMedianTimePast()
greater than the locktime specified in any transaction in that block.

Mempool inclusion rules currently require transactions to be valid for
immediate inclusion in a block in order to be accepted into the mempool.
This release begins applying the BIP113 rule to received transactions,
so transaction whose time is greater than the GetMedianTimePast() will
no longer be accepted into the mempool.

**Implication for miners:** you will begin rejecting transactions that
would not be valid under BIP113, which will prevent you from producing
invalid blocks when BIP113 is enforced on the network. Any
transactions which are valid under the current rules but not yet valid
under the BIP113 rules will either be mined by other miners or delayed
until they are valid under BIP113. Note, however, that time-based
locktime transactions are more or less unseen on the network currently.

**Implication for users:** GetMedianTimePast() always trails behind the
current time, so a transaction locktime set to the present time will be
rejected by nodes running this release until the median time moves
forward. To compensate, subtract one hour (3,600 seconds) from your
locktimes to allow those transactions to be included in mempools at
approximately the expected time.

For more information about the implementation, see
<https://github.com/bitcoin/bitcoin/pull/6566>

Miscellaneous
-------------

The p2p alert system is off by default. To turn on, use `-alert` with
startup configuration.

0.12.1 Change log
=================

Detailed release notes follow. This overview includes changes that affect
behavior, not code moves, refactors and string updates. For convenience in locating
the code changes and accompanying discussion, both the pull request and
git merge commit are mentioned. Changes specific to Bitcoin Knots (beyond Core)
are flagged with an asterisk ('*') before the description.

### RPC and other APIs
- #7739 `7ffc2bd` Add abandoned status to listtransactions (jonasschnelli)
- #7292 `1474ef2` *Update getmempool* RPC calls from final PR (Suhas Daftuar)
- #7518 `07dc972` *Add change options and lockUnspents option to fundrawtransaction (João Barbosa)
- #7527 `a9e73f7` *Fix and cleanup listreceivedbyX documentation (instagibbs)
- #7533 `e57dd84` *AcceptToMemoryPool: Implement rejection overrides for CSV-related policies (luke-jr)
- #7935 `db4bacf` *Versionbits: GBT support (luke-jr)

### Block and transaction handling
- #7543 `834aaef` Backport BIP9, BIP68 and BIP112 with softfork (btcdrak)

### P2P protocol and network code
- #7804 `90f1d24` Track block download times per individual block (sipa)
- #7832 `4c3a00d` Reduce block timeout to 10 minutes (laanwj)
- #7164 `9095594` *Do not download transactions during inital sync (ptschip)
- #7553 `a5bc6a1` *Remove vfReachable and modify IsReachable to only use vfLimited. (Patrick Strateman)
- #7637 `66d5408` *Fix memleak in TorController (rework) (Wladimir J. van der Laan)
- #7642 `d3ead9b` *Avoid "Unknown command" messages when receiving getaddr on outbound connections. (R E Broadley)
- #7862 `7e71785` *Use txid as key in mapAlreadyAskedFor (Suhas Daftuar)
- #7919 `43c14ac` *Fix headers announcements edge case (Suhas Daftuar)

### Validation
- #7821 `4226aac` init: allow shutdown during 'Activating best chain...' (laanwj)
- #7835 `46898e7` Version 2 transactions remain non-standard until CSV activates (sdaftuar)

### Build system
- #7487 `00d57b4` Workaround Travis-side CI issues (luke-jr)
- #7606 `a10da9a` No need to set -L and --location for curl (MarcoFalke)
- #7614 `ca8f160` Add curl to packages (now needed for depends) (luke-jr)
- #7776 `a784675` Remove unnecessary executables from gitian release (laanwj)
- #7189 `1c3d38b` *Remove spurious dollar sign. Fixes #7189. (Chris Moore)
- #7339 `9505815` *Bugfix: Allow explicit --with-libevent when it is required (luke-jr)
- #7339 `8f0c1da` *Bugfix: Actually disable utils if --without-utils passed to configure (luke-jr)
- #7522 `d5fe965` *BITCOIN_SUBDIR_TO_INCLUDE: Improve compatibility further, and reformat to be more readable (luke-jr)
- #7658 `5583a3d` *Add curl to Gitian setup instrustions (BtcDrak)
- #8048 `c3aedca` *doc: Remove outdated qt4 install information from README.md (Wladimir J. van der Laan)
- #8169 `9d016a4` *OSX diskimages need 0775 folder permissions (Jonas Schnelli)
- #8293 `c0251ea` *Bugfix: Allow building libbitcoinconsensus without any univalue (luke-jr)

### Wallet
- #7715 `19866c1` Fix calculation of balances and available coins. (morcos)
- #7521 `21b2f82` *Don't resend wallet txs that aren't in our own mempool (Alex Morcos)

### Testing and QA

- #7320 `3316552` *[qa] Test walletpassphrase timeout (MarcoFalke)
- #7372 `6aae129` *[qa] wallet: Print maintenance (MarcoFalke)
- #7684 `ad8c743` *[qa] Extend tests (MarcoFalke)
- #7697 `c0d9e31` *Tests: make prioritise_transaction.py more robust (Suhas Daftuar)
- #7702 `f23cb7c` *[qa] Add tests verifychain, lockunspent, getbalance, listsinceblock (MarcoFalke)
- #7720 `d89fbfe` *[qa] rpc-test: Normalize assert() (MarcoFalke)
- #7757 `b1dd64b` *[qa] wallet: Wait for reindex to catch up (MarcoFalke)
- #7778 `ff9b436` *[qa] Bug fixes and refactor (MarcoFalke)
- #7801 `28ba22c` *[qa] Remove misleading "errorString syntax" (MarcoFalke)
- #7822 `6862627` *Add listunspent() test for spendable/unspendable UTXO (Joao Fonseca)
- #7853 `f1f1b82` *[qa] py2: Unfiddle strings into bytes explicitly (MarcoFalke)
- #7950 `18b3c3c` *travis: switch to Trusty (Cory Fields and Wladimir J. van der Laan)

### Miscellaneous
- #7617 `f04f4fd` Fix markdown syntax and line terminate LogPrint (MarcoFalke)
- #7747 `4d035bc` added depends cross compile info (accraze)
- #7741 `a0cea89` Mark p2p alert system as deprecated (btcdrak)
- #7780 `c5f94f6` Disable bad-chain alert (btcdrak)
- #7526 `64fd0ce` *fix spelling of advertise in src and doc (jloughry)
- #7541 `52c1011` *Clarify description of blockindex (Matthew Zipkin)
- #7850 `06c73a1` *Removed call to `TryCreateDirectory` from `GetDefaultDataDir` in `src/util.cpp`. (Alexander Regueiro)
- #7922 `c3d1bc3` *CBase58Data::SetString: cleanse the full vector (Kaz Wesley)

Credits
=======

Thanks to everyone who directly contributed to this release:

- accraze
- Alex Morcos
- Alexander Regueiro
- BtcDrak
- Chris Moore
- Cory Fields
- Joao Fonseca
- Jonas Schnelli
- João Barbosa
- Kaz Wesley
- Luke Dashjr
- MarcoFalke
- Mark Friedenbach
- Matthew Zipkin
- NicolasDorier
- Patrick Strateman
- Pieter Wuille
- R E Broadley
- Suhas Daftuar
- Wladimir J. van der Laan
- accraze
- instagibbs
- jloughry
- ptschip

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/bitcoin/).

