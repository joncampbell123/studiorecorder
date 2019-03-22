
/*****************************************************************
 *
 * 256-bit integer C++ class
 *
 *****************************************************************
 *
 * NOTE: To avoid problems, this class always stores the 256-bit
 *       integer in big endian format (MSB...LSB)
 *
 *****************************************************************
 * (C) 2001 Jonathan Campbell
 *****************************************************************/

#include "int256.h"
#include <math.h>
#include <memory.h>

int256::int256()
{
	int x;

	for (x=0;x < 32;x++) bytes[x]=0;
	__err_intconvert_saturated=0;
}

int256::~int256()
{
}

#define INTBYTES		sizeof(int)			/* total bytes in an integer */
#define INTALLBITS		(INTBYTES*8u)		/* total bits in an integer. MUST BE A MULTIPLE OF 8! */
#define INTNSBITS		(INTALLBITS-1u)		/* total bits in an integer minus sign bit */
#define INTSIGNMASK		(1u << INTNSBITS)	/* mask for extracting integer sign bit */

#define MAXINTNEG		(-1u << INTNSBITS)	    	/* largest possible negative integer */
#define MAXINTPOS		((1u << INTNSBITS) - 1u)	/* largest possible positive integer */

/* convert to regular double-precision floating point */
double int256::get_double()
{
	int x;
	double retv;

/* convert to double-precision floating point */
	retv  = (double)(bytes[0]&0x7F);
	for (x=1;x < 32;x++) {
		retv *= 256.0;
		retv += (double)(bytes[x]&0xFF);
	}

/* make negative */
	if (bytes[0] & 0x80) retv = -(retv + 1.0);

	return retv;
}

/* convert double-precision floating point to 256-bit int */
void int256::put_double(double sr)
{
	int x;

/* make note of sign and convert to absolute value */
	if (sr < 0.0) {
		bytes[0] = 0x80;

		sr = -sr - 1.0;
		if (sr < 0.0) sr = 0.0;
	}
	else {
		bytes[0] = 0x00;
	}

/* convert floating point to integer. 
   for sake of sanity when converting segments to integer
   we always round down. storing integer version in BYTE
   array and type conversion takes care of storing only
   the lower 8 bits. */
	for (x=31;x >= 1;x--) {
		bytes[x] = (int)floor(sr);
		sr /= 256.0;
	}

/* convert last portion and clip if too large */
	if (sr > 127.0)		sr = 127.0;
	bytes[x] |= (int)floor(sr);
}

/* convert 256-bit int to int */
int	int256::get_int()
{
	int retv,x,xc,sh,i;

/* signed? */
	if (bytes[0] & 0x80) {
		x=32-INTBYTES;
		i=0;
		xc=0;
		while (x-- > 0 && !xc) xc += (bytes[i++] != 0xFF);
		xc += !(bytes[32-INTBYTES] & 0x80);

/* saturate if too big to fit in integer */
		if (!(__err_intconvert_saturated = xc)) {
			x = 32-INTBYTES;
			retv = ((int)(bytes[x++] | 0x80)) << (INTALLBITS-8);
			sh = (INTALLBITS-8);

			while (sh > 0) {
				sh -= 8;
				retv += ((int)bytes[x++]) << sh;
			}
		}
		else {
			retv = MAXINTNEG;
		}
	}
/* unsigned? */
	else {
		x=32-INTBYTES;
		i=0;
		xc=0;
		while (x-- > 0 && !xc) xc += (bytes[i++] != 0x00);
		xc += (bytes[32-INTBYTES] & 0x80);

/* saturate if too big to fit in integer */
		if (!(__err_intconvert_saturated = xc)) {
			x = 32-INTBYTES;
			retv = ((int)bytes[x++]) << (INTALLBITS-8);
			sh = (INTALLBITS-8);

			while (sh > 0) {
				sh -= 8;
				retv += ((int)bytes[x++]) << sh;
			}
		}
		else {
			retv = MAXINTPOS;
		}
	}

	return retv;
}

/* convert int to 256-bit int */
void int256::put_int(int sr)
{
	int x,sh;

	x=0;
/* signed integer? */
	if (sr & (1 << ((unsigned)INTNSBITS))) {
		memset(bytes,0xFF,32-INTBYTES);
		sh = INTALLBITS;
		x = 32-INTBYTES;

		while (sh > 0) {
			sh -= 8;
			bytes[x] = (sr >> sh)&0xFF;
			x++;
		}
	}
/* unsigned integer? */
	else {
		memset(bytes,0,32-INTBYTES);
		sh = INTALLBITS;
		x = 32-INTBYTES;

		while (sh > 0) {
			sh -= 8;
			bytes[x] = (sr >> sh)&0xFF;
			x++;
		}
	}
}

/* shift left */
void int256::shl(int sh)
{
	int x,s1,s2,sb,so;//,fill;

	if (sh <= 0) return;

	if (sh >= 255) {
		if (bytes[0] & 0x80)	for (x=0;x < 32;x++) bytes[x] = 0xFF;
		else					for (x=0;x < 32;x++) bytes[x] = 0x00;
	}
	else if (sh & 7) {
//		fill = (bytes[0] & 0x80) ? 0xFF : 0x00;
		s1 = sh&7;
		s2 = 8-s1;
		so = sh>>3;
		sb = 31 - so;

		bytes[0]  = (bytes[0] & 0x80) | ((bytes[so] << s1) & 0x7F); so++;
		bytes[0] |= ((bytes[so] >> s2) & 0x7F);

		for (x=1;x < sb;x++) {
			bytes[x] = bytes[so] << s1; so++;
			bytes[x] |= bytes[so] >> s2;
		}
		bytes[x] = (bytes[so] << s1); x++;
		while (x < 32) {
			bytes[x] = 0;
			x++;
		}
	}
	else {
//		fill = (bytes[0] & 0x80) ? 0xFF : 0x00;
		so = sh>>3;
		sb = 32 - so;

		bytes[0] = (bytes[so] & 0x7F) | (bytes[0] & 128);
		so++;

		for (x=1;x < sb;) {
			bytes[x] = bytes[so];
			so++;
			x++;
		}
		for (x=sb;x < 32;x++) bytes[x] = 0;
	}
}

/* shift right */
void int256::shr(int sh)
{
	int x,s1,s2,sb,so,fill;

	if (sh <= 0) return;

	if (sh >= 255) {
		if (bytes[0] & 0x80)	for (x=0;x < 32;x++) bytes[x] = 0xFF;
		else					for (x=0;x < 32;x++) bytes[x] = 0x00;
	}
	else if (sh & 7) {
		fill = (bytes[0] & 0x80) ? 0xFF : 0x00;
		s1 = sh&7;
		s2 = 8-s1;
		so = sh>>3;
		sb = 31 - so;

		for (x=31;x > so;x--) {
			bytes[x] = bytes[sb] >> s1; sb--;
			bytes[x] |= bytes[sb] << s2;
		}
		if (x > 0) {
			bytes[x] = (bytes[sb] >> s1) | (fill << s2);
			x--;
		}
		while (x > 0) {
			bytes[x] = fill;
			x--;
		}

		bytes[0] = (bytes[0] & 0x80) | (fill >> s1);
	}
	else {
		fill = (bytes[0] & 0x80) ? 0xFF : 0x00;
		so = sh>>3;
		sb = 31 - so;

		for (x=31;x >= so;) {
			bytes[x] = bytes[sb];
			sb--;
			x--;
		}
		for (x=so-1;x >= 0;x--) bytes[x] = fill;
	}
}

/* bit scan forward */
int int256::bsf()
{
	int x,i;

	for (x=31,i=0;x >= 0 && !((bytes[x] >> i)&1);) {
		i++;
		if (i >= 8) {
			i=0;
			x--;
		}
	}

	if (x < 0) return 0;

	return (((31-x)*8)+i);
}

/* bit scan reverse */
int int256::bsr()
{
	int x,i;

	for (x=0,i=7;x < 32 && !((bytes[x] >> i)&1);) {
		i--;
		if (i < 0) {
			i=7;
			x++;
		}
	}

	if (x >= 32) return 0;

	return (((31-x)*8)+i);
}

/* negate */
void int256::neg()
{
	int x;

	if (bytes[0] & 0x80) {
		for (x=0;x < 32;x++) bytes[0] ^= 0xFF;

// add +1 to complete negate and carry
		x=31; bytes[31]++;
		while (!bytes[x] && x >= 1) {
			x--;
			bytes[x]++;
		}
	}
}

void int256::mul_int(int sr)
{
	unsigned char tmpres[32];		/* temp. buffer for multipulcation result */
	int x,y,z,a,c,srsgn,v;

/* we need input integer to be absolute */
	if ((srsgn=((sr < 0) ? 0x80 : 0x00)) != 0) sr = -sr;
	memset(tmpres,0,32);

/* for each bit, if set, add "sr" shifted over that bit number */
	for (x=0;x < (int)INTNSBITS && sr;x++) {
		if (sr&1) {
			if (!x) {			/* case 1: we're at the first bit, so we can add without displacement */
				c=0;
				for (y=31;y >= 1;y--) {
					v=bytes[y]+tmpres[y]+c;
					c=v>>8;
					tmpres[y]=(v&0xFF);
				}

/* add, avoid changing sign bit except for maintaining sign for result */
				v=(bytes[0]&0x7F)+(tmpres[0]&0x7F)+c;
				tmpres[0] = ((tmpres[0]&0x80) ^ srsgn) | (v&0x7F);
			}
			else if ((x&7)==0) {/* case 2; we're aligned to a byte boundary, so add without shifting */
				c=0;
				a=x>>3;
				for (y=31-a;y >= 1;y--) {
					v=bytes[y+a]+tmpres[y]+c;
					c=v>>8;
					tmpres[y]=(v&0xFF);
				}

/* add, avoid changing sign bit except for maintaining sign for result */
				v=bytes[a]+(tmpres[0]&0x7F)+c;
				tmpres[0] = ((tmpres[0]&0x80) ^ srsgn) | (v&0x7F);
			}
			else {				/* case 3: we need to shift and add */
				c=0;
				a=x>>3;
				z=x&7;

				for (y=31-a;y >= 1;y--) {
					v=(bytes[y+a] << z)+tmpres[y]+c;
					c=v>>8;
					tmpres[y]=(v&0xFF);
				}

/* add, avoid changing sign bit except for maintaining sign for result */
				v=(bytes[a] << z)+(tmpres[0]&0x7F)+c;
				tmpres[0] = ((tmpres[0]&0x80) ^ srsgn) | (v&0x7F);
			}
		}

		sr>>=1;
	}

	memcpy(bytes,tmpres,32);
}

void int256::add_int(int sr)
{
	int x,sg,carry;

/* adding is easy.... just add corresponding bytes of "sr" to the
   bytes of our 256-bit integer. keep going until carry condition
   expires. this code assumes that when you shift a signed int
   to the right the sign bit is replicated to fill the spaces. */
	carry=0;
	for (x=31;x >= 1 && (sr || carry);x--) {
		sg       = ((sr&0xFF)+bytes[x]+carry);
		carry    = (sg>>8);
		bytes[x] = sg&0xFF;
		sr     >>= 8;
	}

/* then, add the last part. if overflow occurs we don't care. */
	if (sr) {
		sg       = bytes[0]+(sr&0xFF)+carry;
		bytes[0] = sg&0xFF;
	}
}

void int256::sub_int(int sr)
{
	int x,sg,carry;

/* subtracting is easy.... just subtract corresponding bytes of "sr
   to the bytes of our 256-bit integer. keep going until carry
   condition expires. this code assumes that when you shift a
   signed int to the right the sign bit is replicated to fill the
   spaces. */
	carry=0;
	for (x=31;x >= 1 && (sr || carry);x--) {
		sg       = (bytes[x] + -(sr&0xFF) + -carry);
		carry    = (sg<0) ? (((-sg)>>8)+1) : 0;
		bytes[x] = sg&0xFF;
		sr     >>= 8;
	}

/* then, add the last part. if overflow occurs we don't care. */
	if (sr) {
		sg       = (bytes[0] + -(sr&0xFF) + -carry);
		bytes[0] = sg&0xFF;
	}
}

void int256::sub(int256 sr)
{
	int x,sg,carry;

	carry=0;
	for (x=31;x >= 0;x--) {
		sg = ((int)bytes[x]) - (((int)sr.bytes[x]) + carry);
		bytes[x] = (unsigned char)(sg&0xFF);
		carry = (sg < 0)?1:0;
	}
}

int int256::sign()
{
	int x,s;

	if (bytes[0]&0x80)		return -1;

	for (x=0,s=0;x < 32;x++) s += (bytes[x]!=0)?1:0;

	return (s != 0);
}

/* compare this vs v treating each as signed.

   1 this > v
   0 this = v
  -1 this < v*/
int int256::compare(int256 v)
{
	int x,sgn,dif;

/* save a lot of expense by comparing signs.
   if v is nonnegative and we are negative, v > this.
   if v is negative and we are nonnegative, v < this. */
	sgn = (bytes[0] & 0x80);
	if (sgn && !(v.bytes[0] & 0x80)) return -1;
	if (!sgn && (v.bytes[0] & 0x80)) return  1;

/* compare the highest portions first */
/* nonnegative specific */
	if (!sgn) {
		dif = (int)(v.bytes[0] & 0x7F) - (int)(bytes[0] & 0x7F);
		if (dif < 0) return 1;
		else if (dif > 0) return -1;

/* then scan towards least significant until mismatch occurs */
		x=1;
		dif=0;
		while (x < 32 && dif == 0) {
			dif = (int)v.bytes[x] - (int)bytes[x];
			x++;
		}
		if (dif < 0) return 1;
		else if (dif > 0) return -1;
	}
	else {
/* negative specific */
		dif = (int)(v.bytes[0] & 0x7F) - (int)(bytes[0] & 0x7F);
		if (dif > 0) return 1;
		else if (dif < 0) return -1;

/* then scan towards least significant until mismatch occurs */
		x=1;
		dif=0;
		while (x < 32 && dif == 0) {
			dif = (int)v.bytes[x] - (int)bytes[x];
			x++;
		}
		if (dif > 0) return 1;
		else if (dif < 0) return -1;
	}

	return 0;
}

/* compare this vs v treating each as signed.
   v is an integer.

   1 this > v
   0 this = v
  -1 this < v*/
int int256::compare_int(signed int v)
{
	int x,sgn,dif,msk;

/* save a lot of expense by comparing signs.
   if v is nonnegative and we are negative, v > this.
   if v is negative and we are nonnegative, v < this. */
	sgn = (bytes[0] & 0x80);
	if (sgn && !(v & INTSIGNMASK)) return -1;
	if (!sgn && (v & INTSIGNMASK)) return  1;

/* compare the highest portions first */
/* nonnegative specific */
	if (!sgn) {
		msk = (0x7F << (INTALLBITS-8));
		dif = (v & msk) - ((int)(bytes[0]) & 0x7F);
		if (dif < 0) return 1;
		else if (dif > 0) return -1;

/* then scan towards least significant until mismatch occurs */
		x=1;
/* assuming no retard compiler has less than 16-bit wide integers! */
		msk = (0xFF << (INTALLBITS-16));
		dif=0;
		while (x < (int)INTBYTES && dif == 0) {
			dif = (v & msk) - (int)bytes[x];
			x++;
			msk >>= 8;
		}
		if (dif < 0) return 1;
		else if (dif > 0) return -1;
	}
	else {
/* negative specific */
		msk = (0x7F << (INTALLBITS-8));
		dif = (v & msk) - ((int)(bytes[0]) & 0x7F);
		if (dif > 0) return 1;
		else if (dif < 0) return -1;

/* then scan towards least significant until mismatch occurs */
		x=1;
/* assuming no retard compiler has less than 16-bit wide integers! */
		msk = (0xFF << (INTALLBITS-16));
		dif=0;
		while (x < (int)INTBYTES && dif == 0) {
			dif = (v & msk) - (int)bytes[x];
			x++;
			msk >>= 8;
		}
		if (dif > 0) return 1;
		else if (dif < 0) return -1;
	}

	return 0;
}

/* copies contents of v to this */
void int256::copy(int256 v)
{
	memcpy(bytes,v.bytes,32);
}
