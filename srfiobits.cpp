
/*---------------------------------------------------------------*
 | SRF stream reading class bit level reading class              |
 +---------------------------------------------------------------+
 | (C) 2000-2002 Jonathan Campbell                               |
 +---------------------------------------------------------------+
 | Referring to an SRFIOSource object, reads data from it and    |
 | returns from it any specified number of bits.                 |
 *---------------------------------------------------------------*/

#include "global.h"
#include "MediaSourceSRF.h"
#include "srfio.h"
#include "srfiobits.h"
#include "srfc_mambo5.h"

SRFIOSourceBits::SRFIOSourceBits()
{
	iosource=NULL;
	autolimit = -1;
}

SRFIOSourceBits::~SRFIOSourceBits()
{
}

void SRFIOSourceBits::SetIOSource(SRFIOSource *source)
{
	iosource=source;
	getbits_reset();
}

void SRFIOSourceBits::getbits_reset()
{
	bitreg=0;
	bits=8;
}

int SRFIOSourceBits::getbits(int bitcount)
{
	int re,res;

	re=0;
	res=bitcount-1;
	while (bitcount > 0) {
		if (bits >= 8) {
			if (!autolimit)			bitreg = 0;
			else if (iosource)		bitreg = iosource->getbyte();
			else					bitreg = 0;
			bits = 0;
		}

		re |= ((bitreg >> (7-bits))&1)<<res;
		res--;
		bits++;
		bitcount--;
	}

	return re;
}

int SRFIOSourceBits::peekbits(int bitcount)
{
	int re,res,bts,btreg;
	DWORD ofs;

	ofs=iosource->getcurrentpos();

	re=0;
	bts=bits;
	res=bitcount-1;
	btreg=bitreg;
	while (bitcount > 0) {
		if (bts >= 8) {
			if (!autolimit)			btreg = 0;
			else if (iosource)		btreg = iosource->getbyte();
			else					btreg = 0;
			bts = 0;
		}

		re |= ((btreg >> (7-bts))&1)<<res;
		res--;
		bts++;
		bitcount--;
	}

	iosource->seek(ofs);

	return re;
}

BYTE SRFIOSourceBits::getbyte()
{
	BYTE re;

	getbits_reset();
	if (!autolimit)			re = 0;
	else if (iosource)		re = iosource->getbyte();
	else					re = 0;

	return re;
}

void SRFIOSourceBits::SetAutoLimit(int n)
{
	autolimit=n;
}

void SRFIOSourceBits::ClearAutoLimit()
{
	autolimit = -1;
}
