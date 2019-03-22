// global stuff

#ifndef PI
#define PI			3.1415926535
#endif

//#include <windows.h>
//#include <malloc.h>
//#include <memory.h>
#include <string.h>
#include <time.h>
//#include <commdlg.h>
//#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
//#include <direct.h>
//#include <io.h>
#include <math.h>
//#include <malloc.h>
//#include <mmsystem.h>
//#include "resource.h"

#ifndef WIN32
# define strcmpi strcasecmp
# define strnicmp strncasecmp
# include <stdint.h>
typedef uint32_t DWORD;
typedef uint16_t WORD;
typedef uint8_t BYTE;
#endif //WIN32
