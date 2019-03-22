
#include <stdio.h>

#include "global.h"
#include "srfio.h"
#include "maths.h"
#include "int256.h"
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

	int256					srf_v2_params[64];
	bool					srf_v2_params_present[64] = {false};
public:
    void clear(void) {
        type = 0;
        srf2_chunk_id = 0;
        srf2_chunk_length = 0;
    }
    void reset_v2(void) {
        for (unsigned int i=0;i < 64;i++)
            srf_v2_params_present[i] = false;
    }
};

bool SRFReadPacketHeader(SRF_PacketHeader &hdr,SRFIOSourceFile *rfio,SRFIOSourceBits *bfio) {
    BYTE i;

    hdr.clear();

/* let the SRF I/O class know we're looking for SRF chunks */
	rfio->chunk_boundary_notify();

/* get the ball rolling by fetching the first byte */
	i=bfio->getbyte();

/* NOTE: code is meant to decode whatever it encounters,
         however with the more bit-sensitive formats such as
		 MPEG it is easy for  program to mis-interpret one
		 as another and crash from trying to misinterpret, say,
		 an MPEG program stream as an MPEG audio stream. So,
		 the first MPEG format we encounter is the format we
		 stick with during playback. */

/* SRF-I packet? */
	if (i == 0xAA) {
		i=bfio->getbyte(); if (i != 0x5F)	return 0;
		i=bfio->getbyte(); if (i != 0x8E)	return 0;
		i=bfio->getbyte(); if (i != 0x2F)	return 0;
		i=bfio->getbyte(); if (i != 0x65)	return 0;
		i=bfio->getbyte(); if (i != 0x43)	return 0;
		i=bfio->getbyte(); if (i != 0x21)	return 0;
		i=bfio->getbyte(); if (i != 0x00)	return 0;

		i=bfio->getbyte();
		while (i != 0 && hdr.srf1_header.size() < 63) {
			hdr.srf1_header += (char)i;
			i = bfio->getbyte();
		}

/* skip past unused padding bytes */
		i=bfio->getbyte();
		i=bfio->getbyte();

        hdr.type = SRF1_PACKET;
		return true;
	}
/* SRF-I distortion 1 packet? */
	else if (i == 0xDA) {
		i=bfio->getbyte(); if (i != 0xD0)	return 0;
		i=bfio->getbyte(); if (i != 0x00)	return 0;
		i=bfio->getbyte(); if (i != 0x22)	return 0;

		i=bfio->getbyte();
		while (i != 0 && hdr.srf1_header.size() < 63) {
			hdr.srf1_header += (char)i;
			i = bfio->getbyte();
		}

        hdr.type = SRF1_DISTORTION_PACKET;
		return true;
	}
/* SRF-II packet? */
	else if (i == 0x7E) {
        DWORD chunk_id,length;
        int parity,lol,pt;

		i=bfio->getbyte(); if (i != 0x53)	return 0;
		i=bfio->getbyte(); if (i != 0x52)	return 0;
		i=bfio->getbyte(); if (i != 0x46)	return 0;
		i=bfio->getbyte(); if (i != 0x2D)	return 0;
		i=bfio->getbyte(); if (i != 0x32)	return 0;

		parity = paritysum(1,0x7E);
		parity = paritysum(parity,0x53);
		parity = paritysum(parity,0x52);
		parity = paritysum(parity,0x46);
		parity = paritysum(parity,0x2D);
		parity = paritysum(parity,0x32);
// chunk id?
		chunk_id  = ((DWORD)bfio->getbyte())<<24ul;
		chunk_id |= ((DWORD)bfio->getbyte())<<16ul;
		chunk_id |= ((DWORD)bfio->getbyte())<<8ul;
		chunk_id |= ((DWORD)bfio->getbyte());
		parity = paritysum(parity,chunk_id&0xFFul);
		parity = paritysum(parity,(chunk_id>>8ul)&0xFFul);
		parity = paritysum(parity,(chunk_id>>16ul)&0xFFul);
		parity = paritysum(parity,(chunk_id>>24ul)&0xFFul);
// length of length?
		bfio->getbits_reset();
		lol = bfio->getbits(5);
		parity = paritysum(parity,lol);
// length?
		if (lol > 0)	length = bfio->getbits(lol);
		else			length = 0;
		parity = paritysum(parity,length);
// parity?
		pt = bfio->getbits(1);
		if (pt == parity) {
            hdr.srf2_chunk_length = length;
            hdr.srf2_chunk_id = chunk_id;
            hdr.type = SRF2_PACKET;
            hdr.reset_v2();
			return true;
		}
	}

    return false;
}

int main(int argc,char **argv) {
    if (argc < 2) return 1;

    SRFIOSourceFile *r_fileio = new SRFIOSourceFile();      // file read access
    SRFIOSourceBits *b_fileio = new SRFIOSourceBits();      // utility atop file for bitfield parsing

    if (!r_fileio->open(argv[1])) {
        fprintf(stderr,"Unable to open file\n");
        return 1;
    }
    b_fileio->SetIOSource(r_fileio);

    /* r_fileio have refilled the buffer already */
    while (!r_fileio->is_source_depleted()) {
        SRF_PacketHeader hdr;

        if (SRFReadPacketHeader(/*&*/hdr,r_fileio,b_fileio)) {
            if (hdr.type == SRF1_PACKET || hdr.type == SRF1_DISTORTION_PACKET) {
                printf("SRF-I packet header='%s'\n",hdr.srf1_header.c_str());
            }
            else if (hdr.type == SRF2_PACKET) {
                printf("SRF-II packet chunkid=0x%08lx chunklen=0x%08lx\n",
                    (unsigned long)hdr.srf2_chunk_id,(unsigned long)hdr.srf2_chunk_length);
            }
            else {
                printf("SRF: Unknown packet type (BUG?)\n");
            }
        }
    }

    b_fileio->SetIOSource(NULL);
    delete b_fileio;
    delete r_fileio;
    return 0;
}

