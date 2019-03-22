
#include <stdio.h>

#include "global.h"
#include "srfio.h"
#include "srfiofile.h"
#include "srfiobits.h"

using namespace std;

int main(int argc,char **argv) {
    if (argc < 2) return 1;

    SRFIOSourceFile *r_fileio = new SRFIOSourceFile();
    SRFIOSourceBits *b_fileio = new SRFIOSourceBits();

    if (!r_fileio->open(argv[1])) {
        fprintf(stderr,"Unable to open file\n");
        return 1;
    }
    b_fileio->SetIOSource(r_fileio);

    /* ready to read */

    b_fileio->SetIOSource(NULL);
    delete b_fileio;
    delete r_fileio;
    return 0;
}

