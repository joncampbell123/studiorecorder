
/*****************************************************************
 *
 * Various helpful math functions library
 *
 *****************************************************************
 * (C) 1999-2001 Jonathan Campbell
 *****************************************************************/

#include "maths.h"

void int_to_floatrep(int val,int &mantissa,int &exponent,int &sign)
{
	unsigned int m,e,s;

	if (!val) {
		mantissa = 0;
		exponent = 0;
		sign = 0;
	}

	s = (val >> ((sizeof(int)*8)-1));
	if (s)		m = (unsigned)(-val);
	else		m = (unsigned)val;
	e = 0;
	while (!(m & 1)) {
		m >>= 1;
		e++;
	}

	mantissa = (int)m;
	exponent = (int)e;
	sign     = (int)s;
}

int int_needed_bits(unsigned int val)
{
	int x;

	x=0;
	while (val) {
		x++;
		val >>= 1;
	}

	return x;
}

int paritysum(int p,int val)
{
	while (val) {
		p ^= val&1;
		val >>= 1;
	}

	return p;
}
