
#include <stdio.h>

#include "global.h"
#include "srfio.h"
#include "maths.h"
#include "int256.h"
#include "srfiofile.h"
#include "srfiobits.h"
#include "srf2idz.h"

#include <string>

using namespace std;

enum {
    SRF1_PACKET=0xAA,                   /* content */
    SRF1_DISTORTION_PACKET=0xDA,        /* content, distorted in shareware versions on playback */
    SRF2_PACKET=0x7E                    /* SRF-II packet (often informational) */
};

#define SRF1_TIMESTRING                 "TimeString"

struct SRF1_TimeString {
    std::string             timestamp;

    void clear(void) {
        timestamp.clear();
    }
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

bool SRFReadTimeString(SRF1_TimeString &ts,SRFIOSourceFile *rfio) {
    ts.clear();

    {
        char c;

        c = (char)rfio->getbyte();
        for (unsigned int x=0;c != 0 && x < 256;x++) {
            ts.timestamp += c;
            c = (char)rfio->getbyte();
        }
    }

    /* Then the parser reads two additional bytes.
     * Why? Why did I write it to do that??? */
    rfio->getbyte();
    rfio->getbyte();

    return true;
}

/* SRF-II time does not necessarily transmit ALL parameters every packet.
 * Sometimes only changes are transmitted.
 *
 * Foolishly, I didn't think to add a flag that says whether the whole or partial time is transmitted in that packet.
 * You might even say there are no "keyframe" packets, except what you can guess. Doh. */
struct SRF2_TimeCode {
	int256					params[64];
	bool					params_present[64] = {false};
    bool                    time_available = false;

    enum {
        TC_TIME=0,
        TC_RECTIME
    };

    int                     tc_type = TC_TIME;

    SRF2_TimeCode(const int type) : tc_type(type) {
    }

    void clear(void) {
        time_available = false;
        for (unsigned int i=0;i < 64;i++) params_present[i] = false;
    }

    /* parameter indexes */
    enum {
        HOUR=0,                 // 24-hour format
        MINUTE=1,
        SECOND=2,
        MILLISECOND=3,
        HOUR12=4,               // 12-hour format
        CHANNEL_ID=5,

        YEAR=10,
        MONTH=11,
        DAY_OF_MONTH=12,
        DAY_OF_WEEK=13,
        AM_PM=14
        /* 30-40 inclusive print as int
         * 41-52 inclusive print as %02d
         * 53-63 inclusive print as %03d */
    };

    bool take_packet(const SRF_PacketHeader &hdr) {
        for (unsigned int x=0;x < 64;x++) {
            if (hdr.srf_v2_params_present[x]) {
                params[x] = hdr.srf_v2_params[x];
                params_present[x] = true;
            }
        }

        if (tc_type == TC_TIME) {
            time_available =
                params_present[HOUR] &&     params_present[MINUTE] &&
                params_present[SECOND] &&   params_present[YEAR] &&
                params_present[MONTH] &&    params_present[DAY_OF_MONTH];
        }
        else if (tc_type == TC_RECTIME) {
            time_available =
                params_present[HOUR] &&     params_present[MINUTE] &&
                params_present[SECOND];
        }

        return time_available;
    }

    std::string raw_time_string(void) const {
        std::string ret; /* I hope your C++ compiler converts the return into the && move operator */

        if (time_available) {
            char tmp[256];

            /* NTS: Copy-construct to use get_int() without causing any change to the original */
            memset(tmp,0,sizeof(tmp));
            if (tc_type == TC_TIME) {
                snprintf(tmp,sizeof(tmp)-1,"%04u-%02u-%02u %02u:%02u:%02u",
                        int256(params[YEAR]).get_int(),
                        int256(params[MONTH]).get_int(),
                        int256(params[DAY_OF_MONTH]).get_int(),
                        int256(params[HOUR]).get_int(),
                        int256(params[MINUTE]).get_int(),
                        int256(params[SECOND]).get_int());
            }
            else if (tc_type == TC_RECTIME) {
                snprintf(tmp,sizeof(tmp)-1,"%02u:%02u:%02u",
                        int256(params[HOUR]).get_int(),
                        int256(params[MINUTE]).get_int(),
                        int256(params[SECOND]).get_int());
            }
            ret = tmp;

            if (params_present[MILLISECOND]) {
                snprintf(tmp,sizeof(tmp)-1,".%03u",int256(params[MILLISECOND]).get_int());
                ret += tmp;
            }
        }
        else {
            ret = "N/A";
        }

        return ret;
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

            {
                int mbc,exp,sgn,mantissal,expl,par,chnk,shh;
                char dat[32];

                mbc=0;
                // while encountering marker bits...
                while (bfio->getbits(1) && mbc < 64) {
                    mbc++;

                    // read it from the stream
                    sgn = bfio->getbits(1);				// sign (FIXME: Never USED???)
                    mantissal = bfio->getbits(8)+1;		// mantissa length
                    par = bfio->getbits(1);				// mantissa parity check
                    if (par != paritysum(1,mantissal-1)) return false;

                    // data mantissa
                    memset(dat,0,32);

                    shh = (mantissal+7)>>3;
                    if (mantissal & 7) {
                        dat[32 - shh] = bfio->getbits(mantissal & 7);
                        shh--;
                    }

                    while (shh > 0) {
                        dat[32 - shh] = bfio->getbits(8);
                        shh--;
                    }

                    expl = bfio->getbits(3)+1;			// data exponent length
                    par = bfio->getbits(1);				// data exponent length parity
                    if (par != paritysum(1,expl-1)) return false;			// bad header
                    exp = bfio->getbits(expl);			// data exponent
                    chnk = bfio->getbits(6);				// "type code" (param idx)
                    par = bfio->getbits(1);				// "type code" (param idx) parity check
                    if (par != paritysum(1,chnk)) return false;			// bad header

                    if (!hdr.srf_v2_params_present[chnk]) {				// double references not allowed
                        hdr.srf_v2_params_present[chnk]=1;
                        memcpy(&hdr.srf_v2_params[chnk].bytes[0],dat,32);
                        hdr.srf_v2_params[chnk].shl(exp);
                    }
                }
                bfio->getbits_reset();
            }

			return true;
		}
	}

    return false;
}

int main(int argc,char **argv) {
    SRF2_TimeCode srf2_time(SRF2_TimeCode::TC_TIME);
    SRF2_TimeCode srf2_rectime(SRF2_TimeCode::TC_RECTIME);

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

                if (!strcasecmp(hdr.srf1_header.c_str(),SRF1_TIMESTRING)) {
                    SRF1_TimeString ts;

                    if (SRFReadTimeString(/*&*/ts,r_fileio)) {
                        /* SRF-I timestamp is just an ASCIIZ string.
                         * The Studio Recorder software at the time always used 'HH:MM:SS  MM-DD-YYYY'
                         *
                         * HH is 24-hour format
                         *
                         * Example: '11:46:22  11-18-2000'
                         *
                         * Later on, Studio Recorder would record both SRF-I and SRF-II timestamps,
                         * before eventually dropping (in 2001) SRF-I timestamps entirely. */
                        printf("  Time: '%s'\n",ts.timestamp.c_str());
                    }
                }
            }
            else if (hdr.type == SRF2_PACKET) {
                const char *what = "?";

                switch (hdr.srf2_chunk_id) {
                    case SRF_V2_TIME:
                        what = "Time";
                        break;
                    case SRF_V2_RECTIME:
                        what = "Recording time";
                        break;
                    case SRF_V2_FX:
                        what = "FX parameters";
                        break;
                    default:
                        break;
                };

                printf("SRF-II packet chunkid=0x%08lx chunklen=0x%08lx %s\n",
                    (unsigned long)hdr.srf2_chunk_id,(unsigned long)hdr.srf2_chunk_length,what);
                printf("   Contents: ");
                for (unsigned int i=0;i < 64;i++) {
                    if (hdr.srf_v2_params_present[i]) {
                        printf("[%u] = %.3f ",i,hdr.srf_v2_params[i].get_double());
                    }
                }
                printf("\n");

                switch (hdr.srf2_chunk_id) {
                    case SRF_V2_TIME:
                        srf2_time.take_packet(/*&*/hdr);
                        printf("       Time: %s\n",srf2_time.raw_time_string().c_str());
                        break;
                    case SRF_V2_RECTIME:
                        srf2_rectime.take_packet(/*&*/hdr);
                        printf("   Rec Time: %s\n",srf2_rectime.raw_time_string().c_str());
                        break;
                    default:
                        break;
                };
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

