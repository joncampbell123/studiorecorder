
#ifndef CM_INCLUDED_WAS_INT256
#define CM_INCLUDED_WAS_INT256

/*****************************************************************
 *
 * Include file for 256-bit integer C++ object class
 *
 *****************************************************************
 * (C) 2001 Jonathan Campbell
 *****************************************************************/

class int256 {
public:				/* cure MS Visual C++'s "private class" paranoia */
					int256();
					~int256();
/* operations */
	double			get_double();
	int				get_int();
	void			put_double(double sr);
	void			put_int(int sr);
	void			mul_int(int sr);
	void			add_int(int sr);
	void			sub_int(int sr);
	void			sub(int256 sr);
	void			shl(int sh);
	void			shr(int sh);
	int				bsf();
	int				bsr();
	void			neg();
	int				sign();
	int				compare(int256 v);
	int				compare_int(signed int v);
	void			copy(int256 v);
/* the integer data itself */
	unsigned char	bytes[32];
/* status vars */
	unsigned char	__err_intconvert_saturated;		/* conversion of integer resulted in saturation */
};

#endif //CM_INCLUDED_WAS_INT256
