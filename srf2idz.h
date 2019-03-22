
/*---------------------------------------------------------------*
 | SRF-II chunk IDs                                              |
 +---------------------------------------------------------------+
 | (C) 2001-2002 Jonathan Campbell                               |
 *---------------------------------------------------------------*/

/* used to make SRF-II DWORD IDs */
#define MAKE_SRF2_FOURCC(a,b,c,d)		((DWORD)((a << 24) | (b << 16) | (c << 8) | (d)))

/* SRF-II chunk IDs */
#define SRF_V2_TIME						MAKE_SRF2_FOURCC('T','I','M','E')
#define SRF_V2_RECTIME					MAKE_SRF2_FOURCC('R','T','I','M')
#define SRF_V2_FX						MAKE_SRF2_FOURCC('F','X','P','R')
