
/*---------------------------------------------------------------*
 | SRF stream reading class SRFIOSource                          |
 +---------------------------------------------------------------+
 | (C) 2000-2002 Jonathan Campbell                               |
 +---------------------------------------------------------------+
 | Manages reading an SRF stream, exposes an API for reading     |
 | bits and bytes from the stream.                               |
 |                                                               |
 | It is important to note that this code was originally written |
 | for the purposes of reading SRF streams yet it can be used    |
 | also to read any kind of stream, even if not related to SRF.  |
 |                                                               |
 | Class is designed to be used as a base class from which more  |
 | advanced streaming control may be contained in a derived class|
 *---------------------------------------------------------------*/

#include "global.h"
#include "srfio.h"
//#include "srfc_mambo5.h"

SRFIOSource::SRFIOSource()
{
}

SRFIOSource::~SRFIOSource()
{
	close();
}

void SRFIOSource::resetio()
{
}

void SRFIOSource::chunk_boundary_notify()
{
}

void SRFIOSource::refillbuffer()
{
}

/* get byte */
int SRFIOSource::getbyte()
{
	in_buffer_gone=1;
	in_buffer_gone_left=0;
	file_pos++;
	return -1;
}

/* get block */
int SRFIOSource::getblock(char *dat,int len)
{
	in_buffer_gone=1;
	in_buffer_gone_left=0;
	file_pos += len;
	memset(dat,0,len);
	return 0;
}

/* seek to... */
void SRFIOSource::seek(DWORD pos)
{
	in_buffer_gone=0;
	file_len=0;
	file_pos=pos;
	refillbuffer();
	resetio();
}

void SRFIOSource::flush()
{
}

int SRFIOSource::open(char *path)
{
    (void)path;
	file_pos=0;
	file_len=0;
	return 1;
}

void SRFIOSource::reopen()
{
	file_pos=0;
}

void SRFIOSource::close()
{
	file_pos=0;
	file_len=0;
}

char *SRFIOSource::getsourcename()
{
	return name;
}

int	SRFIOSource::is_source_depleted()
{
	if (in_buffer_gone && !in_buffer_gone_left) return 1;

	return 0;
}

DWORD SRFIOSource::getlength()
{
	return file_len;
}

DWORD SRFIOSource::getcurrentpos()
{
	return file_pos;
}
