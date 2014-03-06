
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

// 23 Mar 2012 : GWA : BUFFER_COUNT must be even.

#define BUFFER_COUNT 128
#define BUFFER_SIZE 128


int
main(int argc, char **argv)
{

	// 23 Mar 2012 : GWA : Assume argument passing is *not* supported.
	
	(void) argc;
	(void) argv;
	int fh, len;
//	off_t pos, target;

	const char * filename = "fileonlytest.dat";

	// 23 Mar 2012 : GWA : Test that open works.
(void)len;
	printf("Opening %s\n", filename);

	fh = open(filename, O_RDWR|O_CREAT|O_TRUNC);
	if (fh < 0) {
		err(1, "create failed");
	}
	
	char buf[100];
	sprintf(buf, "ech/o %d > debugfile", fh);
	system(buf);

  // 23 Mar 2012 : GWA : Do the even-numbered writes. Test read() and
  // lseek(SEEK_END).
 return 0;
} 
