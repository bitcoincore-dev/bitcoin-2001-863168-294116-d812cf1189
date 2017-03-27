#ifndef BITCOIN_IPC_CLIENT_H
#define BITCOIN_IPC_CLIENT_H

#if ENABLE_IPC
#define FIXME_IMPLEMENT_IPC(x)
#define FIXME_IMPLEMENT_IPC_VALUE(x) (*(decltype(x)*)(0))
#else
#define FIXME_IMPLEMENT_IPC(x) x
#define FIXME_IMPLEMENT_IPC_VALUE(x) x
#endif

#endif // BITCOIN_IPC_CLIENT_H
