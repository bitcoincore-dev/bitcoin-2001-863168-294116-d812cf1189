#include "streams.h"
#include "util.h"
#include "core/transaction.h"
#include "version.h"

int main(int argc, char **argv)
{
    CAutoFile filein(fopen(argv[1], "rb"), SER_DISK, CLIENT_VERSION);

    try {
    	while (1){
    		int64_t timestamp;
    		CTransaction tx;

    		filein >> timestamp;
    		filein >> tx;

            int64_t ts = timestamp / 1000000;
            int micros = timestamp % 1000000;

    		printf("%s.%d %s", DateTimeStrFormat("%Y%m%d %H:%M:%S", ts).c_str(), 
                micros, tx.ToString().c_str());
    	}	
    } catch (std::ios_base::failure) {

    }

}
