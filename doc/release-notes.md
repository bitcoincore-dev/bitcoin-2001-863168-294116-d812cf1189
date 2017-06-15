Bitcoin Knots version 0.14.2.knots20170618 is now available from:

  <https://bitcoinknots.org/files/0.14.x/0.14.2.knots20170618/>

This is a new minor version release, including new features, various bugfixes
and performance improvements, as well as updated translations.

Please report bugs using the issue tracker at github:

  <https://github.com/bitcoinknots/bitcoin/issues>

Compatibility
==============

Bitcoin Knots officially supports multiple operating systems using the Linux
kernel, macOS 10.8+, and Windows Vista and later.

Microsoft ended support for Windows XP on [April 8th, 2014](https://www.microsoft.com/en-us/WindowsForBusiness/end-of-xp-support),
No attempt is made to prevent installing or running the software on Windows XP, you
can still do so at your own risk but be aware that there are known instabilities and issues.
Please do not report issues about Windows XP to the issue tracker.

Bitcoin Knots should also work on most other Unix-like systems but is not
officially supported on them. If you encounter problems, however, please do
feel free to report them.

Notable changes
===============

BIP148 support
--------------

A large number of users have decided to enforce Segwit activation beginning in August 2017,
using a user-activated softfork defined by BIP148.

As a softfork, BIP148 is backward compatible with old nodes that don't support it. However,
as usual, unsupporting nodes are left vulnerable to a chain split attack by miners, which
can form a second variant of Bitcoin rejected by BIP148 nodes.

Bitcoin Knots can either support BIP148 or leave it unsupported, giving the user the end
decision. This is configured using the new `bip148` config file option. Users should set
this option before August, to indicate whether they wish to support BIP148 (`bip148=1`), or
remain vulnerable to a chain split attack from miners (`bip148=0`). Please note that in
this current version of Bitcoin Knots, the default is to *not* support BIP148, but this
default may be changed in a future version, so either way, you should set the option.

Enabling BIP148 support in August or later is supported, only provided there is either no
chain split, or your node has not pruned blocks from the beginning of the split.

Further details on the risks of supporting or not supporting BIP148 can be read at:

  <http://tiny.cc/bip148-risks>

Software expiration
-------------------

This release of Bitcoin Knots will expire on January 1st, 2019. This is a security
precaution to help ensure nodes remain kept up to date. Future versions of Bitcoin Knots
will also expire 1-2 years after their release.

This is an optional feature. You may disable it by setting `softwareexpiry=0` in your
config file. You may also set `softwareexpiry` to any other POSIX timestamp, to trigger
an expiration at that time instead.

Wallet maintenance limit
------------------------

Previous versions of Bitcoin Knots allowed wallet maintenance (`-salvagewallet`,
`-zapwallettxes`, and `-upgradewallet`) on multiple wallet files. This is no longer
allowed for safety. If you wish to do such maintenance to multiple wallets, please
script it by launching bitcoind once per wallet.

Low-level RPC changes
---------------------

- Error codes have been updated to be more accurate for the following error cases:
  - `getblock` now returns RPC_MISC_ERROR if the block can't be found on disk (for
  example if the block has been pruned). Previously returned RPC_INTERNAL_ERROR.
  - `pruneblockchain` now returns RPC_MISC_ERROR if the blocks cannot be pruned
  because the node is not in pruned mode. Previously returned RPC_METHOD_NOT_FOUND.
  - `pruneblockchain` now returns RPC_INVALID_PARAMETER if the blocks cannot be pruned
  because the supplied timestamp is too late. Previously returned RPC_INTERNAL_ERROR.
  - `pruneblockchain` now returns RPC_MISC_ERROR if the blocks cannot be pruned
  because the blockchain is too short. Previously returned RPC_INTERNAL_ERROR.
  - `setban` now returns RPC_CLIENT_INVALID_IP_OR_SUBNET if the supplied IP address
  or subnet is invalid. Previously returned RPC_CLIENT_NODE_ALREADY_ADDED.
  - `setban` now returns RPC_CLIENT_INVALID_IP_OR_SUBNET if the user tries to unban
  a node that has not previously been banned. Previously returned RPC_MISC_ERROR.
  - `removeprunedfunds` now returns RPC_WALLET_ERROR if bitcoind is unable to remove
  the transaction. Previously returned RPC_INTERNAL_ERROR.
  - `removeprunedfunds` now returns RPC_INVALID_PARAMETER if the transaction does not
  exist in the wallet. Previously returned RPC_INTERNAL_ERROR.
  - `fundrawtransaction` now returns RPC_INVALID_ADDRESS_OR_KEY if an invalid change
  address is provided. Previously returned RPC_INVALID_PARAMETER.
  - `fundrawtransaction` now returns RPC_WALLET_ERROR if bitcoind is unable to create
  the transaction. The error message provides further details. Previously returned
  RPC_INTERNAL_ERROR.
  - `bumpfee` now returns RPC_INVALID_PARAMETER if the provided transaction has
  descendants in the wallet. Previously returned RPC_MISC_ERROR.
  - `bumpfee` now returns RPC_INVALID_PARAMETER if the provided transaction has
  descendants in the mempool. Previously returned RPC_MISC_ERROR.
  - `bumpfee` now returns RPC_WALLET_ERROR if the provided transaction has
  has been mined or conflicts with a mined transaction. Previously returned
  RPC_INVALID_ADDRESS_OR_KEY.
  - `bumpfee` now returns RPC_WALLET_ERROR if the provided transaction is not
  BIP 125 replaceable. Previously returned RPC_INVALID_ADDRESS_OR_KEY.
  - `bumpfee` now returns RPC_WALLET_ERROR if the provided transaction has already
  been bumped by a different transaction. Previously returned RPC_INVALID_REQUEST.
  - `bumpfee` now returns RPC_WALLET_ERROR if the provided transaction contains
  inputs which don't belong to this wallet. Previously returned RPC_INVALID_ADDRESS_OR_KEY.
  - `bumpfee` now returns RPC_WALLET_ERROR if the provided transaction has multiple change
  outputs. Previously returned RPC_MISC_ERROR.
  - `bumpfee` now returns RPC_WALLET_ERROR if the provided transaction has no change
  output. Previously returned RPC_MISC_ERROR.
  - `bumpfee` now returns RPC_WALLET_ERROR if the fee is too high. Previously returned
  RPC_MISC_ERROR.
  - `bumpfee` now returns RPC_WALLET_ERROR if the fee is too low. Previously returned
  RPC_MISC_ERROR.
  - `bumpfee` now returns RPC_WALLET_ERROR if the change output is too small to bump the
  fee. Previously returned RPC_MISC_ERROR.
- `signrawtransaction` errors will now include the txin witness in the object under a new
  "witness" key as well as the former "txinwitness" key (which is nowdeprecated and will be
  removed in 0.15).
- `getrawtransaction` can now be given an optional block hash to find the requested
  transaction in.

miniupnp CVE-2017-8798
----------------------------

Bundled miniupnpc was updated to 2.0.20170509. This fixes an integer signedness error
(present in MiniUPnPc v1.4.20101221 through v2.0) that allows remote attackers
(within the LAN) to cause a denial of service or possibly have unspecified
other impact.

This only affects users that have explicitly enabled UPnP through the GUI
setting or through the `-upnp` option, as since the last UPnP vulnerability
(in Bitcoin Core 0.10.3) it has been disabled by default.

If you use this option, it is recommended to upgrade to this version as soon as
possible.

Known Bugs
==========

Since 0.14.0 the approximate transaction fee shown in Bitcoin-Qt when using coin
control and smart fee estimation does not reflect any change in target from the
smart fee slider. It will only present an approximate fee calculated using the
default target. The fee calculated using the correct target is still applied to
the transaction and shown in the final send confirmation dialog.

0.14.2 Change log
=================

Detailed release notes follow. This overview includes changes that affect
behavior, not code moves, refactors and string updates. For convenience in locating
the code changes and accompanying discussion, both the pull request and
git merge commit are mentioned. Changes specific to Bitcoin Knots (beyond
Core) are flagged with an asterisk ('*') before the description.

### RPC and other APIs
- #10410 `321419b` Fix importwallet edge case rescan bug (ryanofsky)
- #9853  `3ad00b4` *Return correct error codes in bumpfee (John Newbery)
- #9853  `fe51c89` *Return correct error codes in blockchain.cpp (John Newbery)
- #9853  `18c109d` *Return correct error codes in removeprunedfunds (John Newbery)
- #9853  `4943d7a` *Return correct error codes in setban (John Newbery)
- #9853  `f5efe82` *Return correct error codes in fundrawtransaction (John Newbery)
- #8384  `0daa720` *Add witness data output to TxInError messages in a new key (Gregory Sanders)
- #10275 `633d334` *Allow getrawtransaction to take optional blockhash to fetch transaction from a block directly (Karl-Johan Alm)
- #10595 `48f16d6` *Bugfix: RPC/Mining: Use pre-segwit sigops and limits, when working with non-segwit GBT clients (Luke Dashjr)

### P2P protocol and network code
- #10424 `37a8fc5` Populate services in GetLocalAddress (morcos)
- #10441 `9e3ad50` Only enforce expected services for half of outgoing connections (theuni)
- #10593 `73ad56d` *Instead of DoS banning for invalid blocks, merely disconnect nodes if we're relying on them as a primary node (Luke Dashjr)
- #10594 `a511b89` *Bugfix: net: Apply whitelisting criteria to outgoing connections (Luke Dashjr)
- #10532 `f4935f8` *Define a service bit for BIP148 (Luke Dashjr)
- #10532 `9213b04` *Add BIP148 indicator to user agent comments (Luke Dashjr)
- #10532 `6194aab` *Preferentially peer with nodes enforcing BIP148 to avoid partitioning risk (Luke Dashjr)

### Validation
- #10532 `c1224e9` *Add BIP148 UASF logic with a -bip148 option (mkwia)
- #10532 `0b27414` *Check BIP148 on historical blocks and rewind if necessary (Luke Dashjr)

### Build system
- #10414 `ffb0c4b` miniupnpc 2.0.20170509 (fanquake)
- #10228 `ae479bc` Regenerate bitcoin-config.h as necessary (theuni)
- #10328 `c94e262` *Update contrib/debian to latest Ubuntu PPA upload (Matt Corallo)

### Miscellaneous
- #10245 `44a17f2` Minor fix in build documentation for FreeBSD 11 (shigeya)
- #10215 `0aee4a1` Check interruptNet during dnsseed lookups (TheBlueMatt)
- #10445 `87a21d5` *Fix: make CCoinsViewDbCursor::Seek work for missing keys (Pieter Wuille)
- #10282 `21f123d` *Expire bitcoind & bitcoin-qt 1-2 years after its last change (Luke Dashjr)
- #10290 `33fbec1` *Add -stopatheight for benchmarking (Andrew Chow)

### GUI
- #10231 `1e936d7` Reduce a significant cs_main lock freeze (jonasschnelli)

### Wallet
- #10294 `1847642` Unset change position when there is no change (instagibbs)
- #10308 `28b8b8b` *Securely erase potentially sensitive keys/values (Thomas Snider)
- #8694  `d9d0870` *Include actual backup filename in recovery warning message (Luke Dashjr)
- #8694  `e2e2882` *Base backup filenames on original wallet filename (Luke Dashjr)
- #8694  `9ac761f` *Forbid -salvagewallet, -zapwallettxes, and -upgradewallet with multiple wallets (Luke Dashjr)

Credits
=======

Thanks to everyone who directly contributed to this release:

- Alex Morcos
- Andrew Chow
- Anthony Towns
- Cory Fields
- earonesty
- fanquake
- Gregory Sanders
- John Newbery
- Jonas Schnelli
- Karl-Johan Alm
- Luke Dashjr
- Matt Corallo
- mkwia
- Pieter Wuille
- practicalswift
- Russell Yanofsky
- Shigeya Suzuki
- Thomas Kerin
- Thomas Snider
- Wladimir J. van der Laan

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/bitcoin/).

