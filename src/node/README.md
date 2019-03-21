# src/node/

The `src/node/` directory is meant to contain code that accesses node state
(state in `CChain`, `CBlockIndex`, `CCoinsView`, `CTxMemPool`, and similar
classes), and should be segregated from wallet and GUI code, to ensure wallet
and GUI code changes don't interfere with node operation, and to allow wallet
and GUI code to run in separate processes, and perhaps eventually be maintained
in separate source repositories.

As a rule of thumb, code in [`src/wallet/`](../wallet/) and [`src/qt/`](../qt/)
shouldn't call code in `src/node/` directly, and vice versa. But interactions
can happen though more limited [`src/interfaces/`](../interfaces/) classes. Code
outside these directories should also be fine to call from anywhere.

The `src/node/` directory is a new directory introduced in
[#14978](https://github.com/bitcoin/bitcoin/pull/14978) and at the moment is
sparsely populated. Eventually it could contain more substantial files like
[`src/validation.cpp`](../validation.cpp) and
[`src/txmempool.cpp`](../txmempool.cpp).
