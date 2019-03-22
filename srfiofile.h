
#ifndef _INC_SRF_SRFIOFILE
#define _INC_SRF_SRFIOFILE

#include "srfiofile.h"

/* ------- SRFIOSourceFile class ----------------------------
   A derivative of SRFIOSource that allows stream access from
   a file.
   ---------------------------------------------------------- */
#define MAX_SRFIOSRCFILE_BUFFER 32768
class SRFIOSourceFile : public SRFIOSource {
public:
						SRFIOSourceFile();
	virtual             ~SRFIOSourceFile();
	virtual int			getbyte();
	virtual int			getblock(char *dat,int len);
	virtual void		seek(DWORD pos);
	virtual int			open(char *path);
	virtual void		reopen();
	virtual void		close();
	virtual void		resetio();
	virtual void		chunk_boundary_notify();
	virtual void		flush();
	virtual void		refillbuffer();

	int					in_buffer_in;
	BYTE				in_buffer[MAX_SRFIOSRCFILE_BUFFER];

	FILE*				file;
};

#endif //_INC_SRF_SRFIOFILE
