
#ifndef CM_INCLUDED_WAS_MATHS
#define CM_INCLUDED_WAS_MATHS

/*****************************************************************
 *
 * Include file for various math function library
 *
 *****************************************************************
 * (C) 2000-2001 Jonathan Campbell
 *****************************************************************/

void int_to_floatrep(int val,int &mantissa,int &exponent,int &sign);
int int_needed_bits(unsigned int val);
int paritysum(int p,int val);

#endif //CM_INCLUDED_WAS_MATHS
