
#ifndef _INC_SRF_SRFIOBITS
#define _INC_SRF_SRFIOBITS

#include "srfio.h"

/* ------- SRFIOSourceBits class ---------------------------
   provides a way to extract bits from an SRFIOSource,
   regardless of SRFIOSource derivative.
   --------------------------------------------------------- */
class SRFIOSourceBits {
public:
						SRFIOSourceBits();
	virtual             ~SRFIOSourceBits();
	void				SetIOSource(SRFIOSource *source);
	void				getbits_reset();
	int					getbits(int bits);
	int					peekbits(int bits);
	BYTE				getbyte();
	void				SetAutoLimit(int n);
	void				ClearAutoLimit();
	int					bitreg;
	int					bits;
	int					autolimit;
	SRFIOSource*		iosource;
};

#endif //_INC_SRF_SRFIOBITS
