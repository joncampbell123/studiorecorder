
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>

#include "global.h"
#include "srfio.h"
#include "maths.h"
#include "int256.h"
#include "srfiofile.h"
#include "srfiobits.h"
#include "srf2idz.h"

#include <string>

#ifndef O_BINARY
#define O_BINARY (0)
#endif

using namespace std;

enum {
    SRF1_PACKET=0xAA,                   /* content */
    SRF1_DISTORTION_PACKET=0xDA,        /* content, distorted in shareware versions on playback */
    SRF2_PACKET=0x7E                    /* SRF-II packet (often informational) */
};

#define SRF1_TIMESTRING                 "TimeString"
#define SRF1_CHANNELCONTENT             "ChannelContent"

typedef struct {
	char			name[512] = {0};
	int				sample_rate = -1;
	int				bps = -1;
	int				channels = -1;
	bool			monoflag = false;
	bool			stereoflag = false;
	bool			lr_split = false;
	bool			mute = false;
} COMMONSRF1AUDPARMS;

/* I can't believe I allowed a critical part of the format to be so loose in syntax AND used a callback in this manner. */
void ParseSRFAudParms(const char *fmtparms,COMMONSRF1AUDPARMS *parmset,void (*parmcallback)(char *parm,int strlen,void *data),void *data)
{
	int i,j,o,pr,ou;
	char parms[512];

	j = strlen(fmtparms);
	parmset->name[0]=0;
	parmset->sample_rate = -1;
	parmset->bps = -1;
	parmset->channels = -1;
	parmset->monoflag = 0;
	parmset->stereoflag = 0;
	parmset->lr_split = 0;
	parmset->mute = 0;

	for (i=0;i < j;) {
		while (fmtparms[i] == ' ') i++;

		o=0;
		while (fmtparms[i] != ',' && fmtparms[i]) {
			parms[o] = fmtparms[i];
			i++;
			o++;
		}
		if (fmtparms[i] == ',') i++;
		parms[o]=0;
		o--;

// filter out whitespaces
		while (o >= 0 && parms[o] == ' ') {
			parms[o] = 0;
			o--;
		}
		if (parms[o]) o++;
		o = strlen(parms);

// look at the tag
		pr = 0;

// name:?
		if (!strnicmp(parms,"name:",5) && parmset->name[0] == 0 && !pr) {
			strcpy(parmset->name,parms+5);
			pr=1;
		}

// sample rate?
		if (o >= 2) {
			if (!strnicmp(parms + o + -2,"Hz",2) && parmset->sample_rate == -1 && !pr) {
/* avoid confusion with "MHz" and "KHz" being used in the name field
   (likely if the user was stupid enough to enter KHz and MHz somewhere
	in the name field). Basically, if the digit before Hz is not a number,
	ignore it. */
				if (o >= 3) {
					if (!isdigit(parms[o-3]))			ou = 0;
					else								ou = atoi(parms);
				}
				else {
					ou = atoi(parms);
				}

				pr=1;
				if (ou > 0 && ou <= 192000) parmset->sample_rate=ou;
			}
		}

// bits/sample?
		if (o >= 3) {
			if (!strnicmp(parms + o + -3,"bps",3) && parmset->bps == -1 && !pr) {
				parmset->bps = atoi(parms);
				pr=1;
			}
		}

// mono tag?
		if (!strcmpi(parms,"mono") && parmset->channels == -1 && !pr) {
			parmset->channels=1;
			parmset->monoflag=1;
			pr=1;
		}

// stereo tag?
		if (!strcmpi(parms,"stereo") && parmset->channels == -1 && !pr) {
			parmset->channels=2;
			parmset->stereoflag=1;
			pr=1;
		}

// L-R split tag?
		if (!strcmpi(parms,"LR-split") && !pr) {
			parmset->lr_split = 1;
			pr=1;
		}

// mute tag?
		if (!strcmpi(parms,"mute") && !pr) {
			parmset->mute=1;
			pr=1;
		}

// okay, I don't know what this is, pass it down
		if (!pr && parmcallback) {
			parmcallback(parms,o,data);
		}
	}

// LR-split alone means 2-channel stereo
	if (parmset->channels == -1 && parmset->lr_split) {
		parmset->channels = 2;
	}
}

// audio, <name:xxx>, <sample rate>Hz, <mono | stereo>, <bits>bps, <unsigned|signed>, <Bendian|Lendian>, <mute>
//
// rules:
// - the order of the parameters does not necessarily have to follow
//   this order, excepting the "audio" tag
//
// - parameters are separated by commas
//
// - <sample rate> must be numeric, and is determined by the
//   'Hz' suffix. It cannot be negative. If "Hz" is not preceeded
//   by a number (i.e. MHz) it should not be processed as a sample
//   rate value. To further avoid confusion all SRF recording
//   software should take steps to prevent the user from entering
//   'Hz' as any part of the name tag.
//
//  [added 5-30-2001 when problem cropped up recording SRF of Pat
//   Cashman show, "KOMO AM 1000KHz" was entered in the name field
//   and it was discovered afterwards that the SRF playback
//   software had problems parsing the stream because of this]
//
// - <name:xxx> is optional. It give the audio channel a title to
//   go by on playback. It is signified by beginning as "name:"
//   and all characters up until the next comma are considered part
//   of the name
//
// - mono/stereo parameters are optional, if non-existient, decoder
//   should assume monual sound
//
//  [3-18-2002: Since the recording software has always added
//   stereo/mono tags, and additional channel arrangements may
//   be implemented in the future that are incompatible, it is
//   now mandatory that either 'mono', 'stereo', or 'LR-split'
//   exist as one of the descriptor parameters.
//
//   mono = 1-channel
//   stereo = 2-channel stereophonic
//   LR-split = if used alone, implies stereo 2-channel format.
//              has no meaning when used with 'mono'.
//
//   The allowance of 'LR-split' to imply 'stereo' is meant to
//   be a space-saving convencience since, in an SRF-I stream,
//   the descriptor strings are moderately redundant throughout.
//   However, this will cause problems with any version of our
//   SRF player prior to v4.26 because they assume that the
//   lack of the 'stereo' tag implies a monural format. Therefore
//   SRF recording software should provide an option to write
//   the backwards-compatible arrangement of LR-split]
//
// - <bits> must be numeric, and is determined by the 'bps' suffix
//
// - unsigned/signed tags are optional. if non-existient, decoder
//   should assume unsigned
//
// - Bendian/Lendian tags are optional. if non-existient, decoder
//   assume Big Endian byte order
//
// - The "mute" tag is not required. It's presence should indicate
//   that the audio chunk was meant to be muted but was recorded
//   anyway for posterity
//
// - format descriptor may not be longer than 512 characters

/* -----structs and callback function for handling params specific
        to this format's descriptor strings */
typedef struct {
	int			sign = -1;
	int			bendian = -1;
} SRF1PCMAUDPARMS;

void srf_v1_interpret_audio__pc(char *parms,int strlen,void *data)
{
	SRF1PCMAUDPARMS *pa = (SRF1PCMAUDPARMS*)data;

    // By the way, these informative tags were written in all SRF files but never really paid attention to!
    // I wanted to support different PCM formats, but all I implemented support for was 16-bit signed or
    // 8-bit signed. SRFPLAY as compiled in 2002 did not support unsigned formats. Even the 12-bit format
    // (hardly used!) was signed 12-bit!

// unsigned tag?
	if (!strcmpi(parms,"unsigned") && pa->sign == -1) {
		pa->sign=0;
	}
// signed tag?
	if (!strcmpi(parms,"signed") && pa->sign == -1) {
		pa->sign=1;
	}
// Lendian tag?
	if (!strcmpi(parms,"Lendian") && pa->bendian == -1) {
		pa->bendian = 0;
	}
// Bendian tag?
	if (!strcmpi(parms,"Bendian") && pa->bendian == -1) {
		pa->bendian = 1;
	}
}

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

bool SRFReadTimeString(SRF1_TimeString &ts,SRFIOSource *rfio) {
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

struct SRF1_ChannelContentHeader {
    WORD            channel_num = 0;
    DWORD           length = 0;
    std::string     raw_format_string;
    std::string     typestr;
    std::string     paramstr;

    void clear(void) {
        channel_num = 0;
        length = 0;
        raw_format_string.clear();
        typestr.clear();
        paramstr.clear();
    }
};

bool SRFChannelContentHeader(SRF1_ChannelContentHeader &ccn,SRFIOSource *rfio) {
    ccn.clear();

    ccn.channel_num  =  (((unsigned char)rfio->getbyte()) << 8u);
    ccn.channel_num +=    (unsigned char)rfio->getbyte();

    ccn.length  =       (((unsigned char)rfio->getbyte()) << 24ul);
    ccn.length +=       (((unsigned char)rfio->getbyte()) << 16ul);
    ccn.length +=       (((unsigned char)rfio->getbyte()) <<  8ul);
    ccn.length +=         (unsigned char)rfio->getbyte();

    {
        char c;

        c = rfio->getbyte();
        for (unsigned int i=0;c != 0 && i < 511;i++) {
            ccn.raw_format_string += c;
            c = rfio->getbyte();
        }
    }

    // unused (once a checksum)
    rfio->getbyte();
    rfio->getbyte();

    /* take the raw string, separate by commas. first one is type */
    {
        auto i = ccn.raw_format_string.begin();

        for (;i != ccn.raw_format_string.end();i++) {
            if ((*i) == ',') {
                i++;
                while (i != ccn.raw_format_string.end() && *i == ' ') i++;
                break;
            }

            ccn.typestr += (*i);
        }

        for (;i != ccn.raw_format_string.end();i++)
            ccn.paramstr += (*i);
    }

    return true;
}

static constexpr const char * const srf2_months[12] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
static constexpr const char * const srf2_wdays[7] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
static constexpr const char * const srf2_ampm[2] = {"AM","PM"};

/* SRF-II time does not necessarily transmit ALL parameters every packet.
 * Sometimes only changes are transmitted.
 *
 * Foolishly, I didn't think to add a flag that says whether the whole or partial time is transmitted in that packet.
 * You might even say there are no "keyframe" packets, except what you can guess. Doh. */
struct SRF2_TimeCode {
	int256					params[64];
	bool					params_present[64] = {false};
    bool                    time_available = false;

    const std::string       format_string_default_rectime = "\x80:\x81:\x82.\x83";
    const std::string       format_string_default_time = "\x80:\x81:\x82 \x8B \x8C, \x8A";
    std::string             format_string_default;
    std::string             format_string;

    enum {
        TC_TIME=0,
        TC_RECTIME
    };

    int                     tc_type = TC_TIME;

    SRF2_TimeCode(const int type) : tc_type(type) {
        switch (type) {
            case TC_TIME:
                format_string_default = format_string_default_time;
                break;
            case TC_RECTIME:
                format_string_default = format_string_default_rectime;
                break;
            default:
                break;
        }

        format_string = format_string_default;
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

    bool take_packet(const SRF_PacketHeader &hdr,SRFIOSource *rfio=NULL/*for more data*/) {
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

        if (rfio != NULL) {
            /* following the SRF-II packet header is a string up to len bytes
             * with formatting codes so the recording software controls how it appears
             * while params carry the raw data.
             *
             * Foolishly, the reading code will stop when it hits a zero byte which may
             * occur before len bytes. Doh. */
            bool repeat_last = false;
            std::string nfmt;

            for (unsigned int x=0;x < hdr.srf2_chunk_length;x++) {
                char c = rfio->getbyte();

                if (c == 0) {
                    break;
                }
                else if (c == ((char)0xFF)) { // char literal
                    x++; // it counts!
                    c = rfio->getbyte();
                    if (c != 0) nfmt += c;
                }
                else if (c == ((char)0xFE)) { // repeat last string
                    repeat_last = true;
                    break;
                }
                else {
                    nfmt += c;
                }
            }

            if (nfmt.empty()) {
                /* The original software does not do this, but if the string came out empty,
                 * go back to the original string. */
                format_string = format_string_default;
            }
            else if (!repeat_last) {
                format_string = nfmt;
            }
        }

        return time_available;
    }

    std::string formatted_time_string(void) const {
        std::string ret; /* I hope your C++ compiler converts the return into the && move operator */

        if (time_available) {
            for (auto i=format_string.begin();i!=format_string.end();i++) {
                if (((unsigned char)(*i)) & 0x80u) {
                    if (!(((unsigned char)(*i)) & 0x40u)) {
                        unsigned char idx = (((unsigned char)(*i)) & 0x3Fu);
                        int dat = int256(params[idx]).get_int(); /* Copy construct to avoid const access error */
                        char tmp[256];

                        tmp[0] = 0;
                        switch (idx) {
                            case HOUR:          sprintf(tmp,"%d",dat); break;
                            case MINUTE:        sprintf(tmp,"%02d",dat); break;
                            case SECOND:        sprintf(tmp,"%02d",dat); break;
                            case MILLISECOND:   sprintf(tmp,"%03d",dat); break;
                            case HOUR12:        sprintf(tmp,"%d",dat); break;
                            case CHANNEL_ID:    sprintf(tmp,"%d",dat); break;
                            case YEAR:          sprintf(tmp,"%d",dat); break;
                            case MONTH: {
                                if (dat >= 1 && dat <= 12)  sprintf(tmp,"%s",srf2_months[dat-1]);
                                else                        sprintf(tmp,"MONTH[%d]",dat);
                            } break;
                            case DAY_OF_MONTH:  sprintf(tmp,"%d",dat); break;
                            case DAY_OF_WEEK: {
                                if (dat >= 1 && dat <= 7)   sprintf(tmp,"%s",srf2_wdays[dat-1]);
                                else                        sprintf(tmp,"WEEKDAY[%d]",dat);
                            } break;
                            case AM_PM: {
                                if (dat >= 0 && dat <= 1)   sprintf(tmp,"%s",srf2_ampm[dat]);
                                else                        sprintf(tmp,"AMPM[%d]",dat);
                            } break;
                            default:
                                if (idx >= 53)              sprintf(tmp,"%04d",dat);
                                else if (idx >= 41)         sprintf(tmp,"%02d",dat);
                                else if (idx >= 30)         sprintf(tmp,"%d",dat);
                                else                        sprintf(tmp,"PARAM%d[%d]",idx,dat);
                                break;
                        };

                        ret += tmp;
                    }

                    /* code ignores bytes if both 0x80 and 0x40 (c >= 0xC0).
                     * I probably meant to encode more information later, though I never did. */
                }
                else {
                    ret += *i;
                }
            }
        }

        return ret;
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

bool SRFReadPacketHeader(SRF_PacketHeader &hdr,SRFIOSource *rfio,SRFIOSourceBits *bfio) {
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

bool SRFAudioDecode(int16_t* &audio,uint32_t &audio_length,uint32_t &audio_channels,uint32_t &audio_rate,const SRF1_ChannelContentHeader &ccn,SRFIOSource *rfio) {
    SRF1PCMAUDPARMS			pcmparms;
    COMMONSRF1AUDPARMS		parms;

    if (audio) delete[] audio;
    audio = NULL;
    audio_length = 0;
    audio_channels = 0;
    audio_rate = 0;

    ParseSRFAudParms(ccn.paramstr.c_str(),&parms,srf_v1_interpret_audio__pc,&pcmparms);

    // do we have enough info?
    if (pcmparms.bendian == -1)		pcmparms.bendian = 1;		// Big Endian default
    if (pcmparms.sign == -1)		pcmparms.sign = 0;			// Unsigned default
    if (parms.sample_rate == -1)	return false;				// cannot work without sample rate
    if (parms.bps == -1)			return false;				// cannot work without bits/sample
    if (parms.sample_rate <= 1)     return false;

    if (parms.channels < 1) parms.channels = 1;                 // in case mono/stereo is missing
    if (parms.channels > 2) return false;                       // Studio Recorder was never used for anything beyond stereo

    audio_rate = parms.sample_rate;
    audio_channels = parms.channels;

    if (parms.bps == 16)
        audio_length = ccn.length / (2ul * (unsigned long)audio_channels);
    else if (parms.bps == 8)
        audio_length = ccn.length / (1ul * (unsigned long)audio_channels);
    // TODO: Is there any SRF file that used the 12-bit format?

    if (audio_length > 0) {
        audio = new(std::nothrow) int16_t[audio_length * audio_channels];
        if (audio == NULL) return false;

        unsigned char buf[2/*bytes/sample*/ * 2/*channels*/];
        int16_t *d = audio;
        int samp;

        if (parms.bps == 16) {
            for (size_t i=0;i < audio_length;i++) {
                unsigned char *s = buf;

                rfio->getblock((char*)buf,2 * audio_channels);

                for (size_t c=0;c < audio_channels;c++) {
                    samp  =  (int)(*s++);
                    samp += ((int)(*s++)) << 8;
                    if (samp & 0x8000) samp -= 0x10000;
                    *d++ = (int16_t)samp;
                }

                assert(s <= (buf+sizeof(buf)));
            }
        }
        else if (parms.bps == 8) {
            for (size_t i=0;i < audio_length;i++) {
                unsigned char *s = buf;

                rfio->getblock((char*)buf,1 * audio_channels);

                for (size_t c=0;c < audio_channels;c++) {
                    samp  = ((int)(*s++)) << 8;
                    if (samp & 0x8000) samp -= 0x10000;
                    *d++ = (int16_t)samp;
                }

                assert(s <= (buf+sizeof(buf)));
            }
        }
        else {
            return false;
        }

        assert(d <= (audio + (audio_channels * audio_length)));
    }

#if 0
    printf("        PCM: bendian=%u sign=%u rate=%lu bps=%u ch=%u\n",
        (unsigned int)pcmparms.bendian,
        (unsigned int)pcmparms.sign,
        (unsigned long)parms.sample_rate,
        (unsigned int)parms.bps,
        (unsigned int)parms.channels);
#endif

    return true;
}

class SRFChannel {
public:
    SRFChannel() { }
    ~SRFChannel() { close_wav(); }
    SRFChannel(const SRFChannel &x) = delete;
    SRFChannel(const SRFChannel &&x) = delete;
private:
    unsigned int                output_rate = 48000;
    unsigned int                output_channels = 2;
    int16_t                     cur_sample[2] = {0,0};
    int16_t                     next_sample[2] = {0,0};
    unsigned int                frac = 0;                   // frac / output_rate for interpolation
    unsigned long               out_count = 0;
private:
    int                         fd = -1;
public:
    bool open_wav(const unsigned int n) {
        char tmp[64];

        sprintf(tmp,"channel%u.wav",n);
        return open_wav(tmp);
    }
    bool open_wav(const char *path) {
        if (fd < 0) {
            fd = open(path,O_WRONLY|O_BINARY|O_CREAT|O_TRUNC,0644);
            if (fd < 0) return false;

            out_count = 0;

            unsigned char hdr[44];

            memset(hdr,0,sizeof(hdr));

            memcpy(hdr+0 ,"RIFF",4);
            *((uint32_t*)(hdr+4)) = htole32(0xFFFFFFFFul); // dummy length
            memcpy(hdr+8 ,"WAVE",4);

            memcpy(hdr+12,"fmt ",4);
            *((uint32_t*)(hdr+16)) = htole32(16);           // length of "fmt "

            *((uint16_t*)(hdr+20)) = 1;                     // wFormatTag = WAVE_FORMAT_PCM
            *((uint16_t*)(hdr+22)) = output_channels;       // nChannels
            *((uint32_t*)(hdr+24)) = output_rate;           // nSamplesPerSec
            *((uint32_t*)(hdr+28)) = output_rate * output_channels * 2;//nAvgBytesPerSec
            *((uint16_t*)(hdr+32)) = output_channels * 2;   // nBlockAlign
            *((uint16_t*)(hdr+34)) = 16;                    // wBitsPerSample

            memcpy(hdr+36,"data",4);
            *((uint32_t*)(hdr+40)) = htole32(0xFFFFFFFFul); // dummy length

            ::write(fd,hdr,44);
        }

        return (fd >= 0);
    }
    void close_wav(void) {
        if (fd >= 0) {
            unsigned char buf[4];
            unsigned long x1,x2;

            x1 = out_count + 36;
            x2 = out_count;
            out_count = 0;

            *((uint32_t*)buf) = htole32(x1);
            ::lseek(fd,4,SEEK_SET); // RIFF:WAVE chunk
            ::write(fd,buf,4);

            *((uint32_t*)buf) = htole32(x2);
            ::lseek(fd,40,SEEK_SET); // data chunk
            ::write(fd,buf,4);

            close(fd);
        }
    }
public:
    void interpolate(int16_t *output) {
        for (unsigned int c=0;c < output_channels;c++) {
            int delta = (int)next_sample[c] - (int)cur_sample[c];
            int delta2 = (int)(((long)delta * (long)frac) / (long)output_rate);
            output[c] = (int16_t)((int)cur_sample[c] + delta2);
        }
    }
    void finish_interpolate(size_t src_rate) {
        int16_t result[2];

        while (frac < output_rate) {
            interpolate(result);
            write_dst_sample(result);
            frac += src_rate;
        }
    }
    void write_dst_sample(int16_t *s) {
        if (fd >= 0) {
            unsigned char buf[2 * 2]; /* sizeof(int16_t) * 2 */
            unsigned char *d = buf;

            assert(output_channels <= 2);
            for (unsigned int c=0;c < output_channels;c++) {
                *d++ = (unsigned char)(s[c] & 0xFF);
                *d++ = (unsigned char)(s[c] >> 8);
            }

            assert(d <= (buf+sizeof(buf)));
            ::write(fd,buf,2 * output_channels);
            out_count += 2 * output_channels;
        }
    }
    void load_interpolate_sample(int16_t *src,size_t src_channels) {
        for (unsigned int c=0;c < output_channels;c++) cur_sample[c] = next_sample[c];

        if (src_channels == 1 && output_channels == 2) {
            next_sample[0] = next_sample[1] = src[0];
        }
        else if (src_channels == 2 && output_channels == 1) {
            next_sample[0] = (int16_t)(((int)src[0] + (int)src[1]) / 2);
        }
        else {
            size_t c=0;

            while (c < std::min((size_t)output_channels,src_channels)) {
                next_sample[c] = src[c];
                c++;
            }
            while (c < std::max((size_t)output_channels,src_channels)) {
                next_sample[c] = 0;
                c++;
            }
        }
    }
    int write(int16_t *src,size_t src_samples,size_t src_channels,size_t src_rate) {
        int r = 0;

        if (src_rate <= 0)
            return 0;

        assert(src_channels <= 2);
        assert(output_channels <= 2);
        while (src_samples > 0) {
            finish_interpolate(src_rate);
            if (frac >= output_rate) {
                frac -= output_rate;
                load_interpolate_sample(src,src_channels);
                src += src_channels;
                src_samples--;
            }
        }

        return r;
    }
public:
};

/* this is more generous than SRFPLAY */
#define MAX_CHANNELS 64

SRFChannel      srf_channel[MAX_CHANNELS];

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
                else if (!strcasecmp(hdr.srf1_header.c_str(),SRF1_CHANNELCONTENT)) {
                    SRF1_ChannelContentHeader ccn;

                    if (SRFChannelContentHeader(/*&*/ccn,r_fileio)) {
                        printf("  Content: Channel #%u Length %lu bytes\n",(unsigned int)ccn.channel_num,(unsigned long)ccn.length);
                        printf("  ....raw format: %s\n",ccn.raw_format_string.c_str());
                        printf("  ..........type: %s\n",ccn.typestr.c_str());
                        printf("  .........param: %s\n",ccn.paramstr.c_str());

                        if (ccn.typestr == "audio") {
                            int16_t *audio = NULL;
                            uint32_t audio_length = 0;
                            uint32_t audio_channels = 0;
                            uint32_t audio_rate = 0;

                            if (SRFAudioDecode(/*&*/audio,/*&*/audio_length,/*&*/audio_channels,/*&*/audio_rate,ccn,r_fileio)) {
                                if (ccn.channel_num < MAX_CHANNELS) {
                                    srf_channel[ccn.channel_num].open_wav(ccn.channel_num);
                                    srf_channel[ccn.channel_num].write(audio,audio_length,audio_channels,audio_rate);
                                }
                            }

                            if (audio) delete[] audio;
                        }
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
                        printf("..Formatted: %s\n",srf2_time.formatted_time_string().c_str());
                        break;
                    case SRF_V2_RECTIME:
                        srf2_rectime.take_packet(/*&*/hdr);
                        printf("   Rec Time: %s\n",srf2_rectime.raw_time_string().c_str());
                        printf("..Formatted: %s\n",srf2_rectime.formatted_time_string().c_str());
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

