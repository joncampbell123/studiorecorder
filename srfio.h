
#ifndef _INC_SRF_SRFIO
#define _INC_SRF_SRFIO

/* ------- BASE SRFIOSource class --------------------------
   This class is a bare-bones implementation of SRFIOSource.
   It does not read from anything, does not carry variables of
   it's own, and effectively does nothing. It contains all
   parts that are expected of this class and all derivatives.
   --------------------------------------------------------- */
class SRFIOSource {
public:
						SRFIOSource();
						~SRFIOSource();
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
	virtual char*		getsourcename();
	virtual int			is_source_depleted();
	virtual DWORD		getlength();
	virtual DWORD		getcurrentpos();

	int					bitreg;
	int					bits;
	DWORD				file_pos;
	DWORD				file_len;
	int					in_buffer_gone;
	int					in_buffer_gone_left;
	char				name[270];
};

#endif //_INC_SRF_SRFIO
