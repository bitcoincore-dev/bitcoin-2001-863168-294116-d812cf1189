#include "streams.h"
#include "util.h"
#include "core/block.h"
#include "clientversion.h"

void print(CBlock &);

int main(int argc, char **argv)
{
    CAutoFile filein(fopen(argv[1], "rb"), SER_DISK, CLIENT_VERSION);

    try {
    	while (1){
    		int64_t timestamp;
    		CBlock block;

    		filein >> timestamp;
    		filein >> block;

            int64_t ts = timestamp / 1000000;
            int micros = timestamp % 1000000;

    		printf("%s.%d ", DateTimeStrFormat("%Y%m%d %H:%M:%S", ts).c_str(), micros);
		    print(block);
    	}	
    } catch (std::ios_base::failure) {

    }

}

void print(CBlock &block)
{
    printf("CBlock(hash=%s, ver=%d, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%lu)\n",
        block.GetHash().ToString().c_str(),
        block.nVersion,
        block.hashPrevBlock.ToString().c_str(),
        block.hashMerkleRoot.ToString().c_str(),
        block.nTime, block.nBits, block.nNonce,
        block.vtx.size());
    for (unsigned int i = 0; i < block.vtx.size(); i++)
    {
        printf("[%d] %s\n", i, block.vtx[i].ToString().c_str());
    }
    printf("\n");
}
