
/*---------------------------------------------------------------*
 | SRFIOSourceFile for use in reading streams from a file        |
 +---------------------------------------------------------------+
 | (C) 2000-2002 Jonathan Campbell                               |
 +---------------------------------------------------------------+
 | Derived from base class SRFIOSource                           |
 *---------------------------------------------------------------*/

#include "global.h"
#include "srfio.h"
#include "srfiofile.h"

SRFIOSourceFile::SRFIOSourceFile()
{
	file=NULL;
}

SRFIOSourceFile::~SRFIOSourceFile()
{
	close();
}

/* reset read buffers */
void SRFIOSourceFile::resetio()
{
/* ----------- MANAGEMENT OF MAMBO #5 ENCRYPTION ----------- */
	in_buffer_gone=0;
	in_buffer_in=0;
/* --------------------------------------------------------- */
}

/* notification by caller that it is prepared to scan for any
   up and coming chunks. This is relevant as Mambo #5 encryption
   is not allowed by SRF standard to begin anywhere except directly
   before a new chunk. */
void SRFIOSourceFile::chunk_boundary_notify()
{
}

/* refill buffer */
void SRFIOSourceFile::refillbuffer()
{
	int x;
	DWORD fpos;

	if (!file) {
		in_buffer_gone=1;
		in_buffer_gone_left=0;
		return;
	}

	in_buffer_in=0;
	x = fread(in_buffer,1,MAX_SRFIOSRCFILE_BUFFER,file);
	if (x < MAX_SRFIOSRCFILE_BUFFER)	in_buffer_gone_left = x;
	else								in_buffer_gone_left = MAX_SRFIOSRCFILE_BUFFER;

	if (x < MAX_SRFIOSRCFILE_BUFFER) {
		memset(in_buffer+x,0,MAX_SRFIOSRCFILE_BUFFER-x);
	}

	fpos = ftell(file);
	if (file_len < fpos) file_len = fpos;
}

int SRFIOSourceFile::getbyte()
{
	BYTE r;

	if (in_buffer_in >= in_buffer_gone_left) {
		in_buffer_gone=1;
		in_buffer_gone_left=0;
		return -1;
	}

	r = in_buffer[in_buffer_in++];
	file_pos++;
	if (in_buffer_in >= MAX_SRFIOSRCFILE_BUFFER) refillbuffer();

	return r;
}

int SRFIOSourceFile::getblock(char *dat,int len)
{
	int ld,cc;

	cc=0;
	file_pos += len;
	while (len > 0) {
		ld = in_buffer_gone_left - in_buffer_in;
		if (ld > len) ld = len;
		if (ld > 0)	memcpy(dat,in_buffer+in_buffer_in,ld);
		else		len=0;

		cc += ld;
		in_buffer_in += ld;
		dat += ld;
		len -= ld;
		if (in_buffer_in >= MAX_SRFIOSRCFILE_BUFFER) {
			in_buffer_in = 0;
			refillbuffer();
		}
	}

	return cc;
}

/* seek to... */
void SRFIOSourceFile::seek(DWORD pos)
{
	in_buffer_gone=0;

	fseek(file,0,SEEK_END);
	file_len=ftell(file);
	file_pos=pos;
	fseek(file,pos,SEEK_SET);
	refillbuffer();

	resetio();
}

void SRFIOSourceFile::flush()
{
	if (in_buffer_in) refillbuffer();
}

int SRFIOSourceFile::open(char *path)
{
	close();

	file = fopen(path,"rb");
	if (!file) return 0;

	resetio();

	strcpy(name,path);
	fseek(file,0,SEEK_END);
	file_pos=0;
	file_len=ftell(file);
	fseek(file,0,SEEK_SET);

	refillbuffer();

	return 1;
}

void SRFIOSourceFile::reopen()
{
	resetio();

	fseek(file,0,SEEK_END);
	file_pos=0;
	file_len=ftell(file);
	fseek(file,0,SEEK_SET);

	refillbuffer();
}

void SRFIOSourceFile::close()
{
	if (file) {
		fclose(file);
		file=NULL;
	}
}
