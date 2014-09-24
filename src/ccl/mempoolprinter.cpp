#include "util.h"
#include "core.h"
#include "txmempool.h"

#include <iostream>
#include <stdio.h>

void print(CTxMemPoolEntry &);

using namespace std;

int main(int argc, char **argv)
{
    CAutoFile filein = CAutoFile(fopen(argv[1], "rb"), SER_DISK, CLIENT_VERSION);

    try {
        while (1){
            CTransaction tx;
            int64_t nFee;
            int64_t nTime;
            double dPriority;
            unsigned int nHeight;

            filein >> tx;
            filein >> nFee;
            filein >> nTime;
            filein >> dPriority;
            filein >> nHeight;
            //printf("%s ", tx.ToString().c_str());
            //printf("%lld %lld %lu %lld %e %u\n", timeMicros, nFee, nTxSize, nTime, dPriority, nHeight);
            CTxMemPoolEntry e(tx, nFee, nTime, dPriority, nHeight);

//            printf("%s.%d ", DateTimeStrFormat("%Y%m%d %H:%M:%S", ts).c_str(), micros);
            print(e);
        }   
    } catch (std::ios_base::failure) {

    }

}

void print(CTxMemPoolEntry &entry)
{
    cout << entry.GetTx().ToString() << endl;
    cout << "nFee= " << entry.GetFee() << " ";
    cout << "nTxSize= " << entry.GetTxSize() << " ";
    cout << "nTime= " << entry.GetTime() << " ";
    cout << "dPriority= " << entry.GetPriority(entry.GetHeight()) << " ";
    cout << "nHeight= " << entry.GetHeight() << endl;
}
