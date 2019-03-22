
#include <stdio.h>

#include "global.h"
#include "srfio.h"
#include "srfiofile.h"
#include "srfiobits.h"

#include <string>

using namespace std;

enum {
    SRF1_PACKET=0xAA,                   /* content */
    SRF1_DISTORTION_PACKET=0xDA,        /* content, distorted in shareware versions on playback */
    SRF2_PACKET=0x7E                    /* SRF-II packet (often informational) */
};

/* It's impossible to handle all SRF packets as a C++ class because how much data you handle
 * is packet-dependent. */
class SRF_PacketHeader {
public:
    unsigned char           type = 0;
    std::string             srf1_header;        /* SRF-I header */
    DWORD                   srf2_chunk_id = 0;  /* SRF-II chunk id */
    DWORD                   srf2_chunk_length = 0;/* SRF-II chunk length */
};

int main(int argc,char **argv) {
    if (argc < 2) return 1;

    SRFIOSourceFile *r_fileio = new SRFIOSourceFile();
    SRFIOSourceBits *b_fileio = new SRFIOSourceBits();

    if (!r_fileio->open(argv[1])) {
        fprintf(stderr,"Unable to open file\n");
        return 1;
    }
    b_fileio->SetIOSource(r_fileio);

    /* ready to read */

    b_fileio->SetIOSource(NULL);
    delete b_fileio;
    delete r_fileio;
    return 0;
}

