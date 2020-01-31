#
/*
 *    Copyright (C) 2020
 *    Hayati Ayguen (h_ayguen@web.de)
 *
 *    DAB-library is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    DAB-library is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with DAB-library; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *	E X A M P L E  P R O G R A M
 *	This program might (or might not) be used to mould the interface to
 *	your wishes. Do not take it as a definitive and "ready" program
 *	for the DAB-library
 */
#include	<unistd.h>
#include	<signal.h>
#include	<getopt.h>
#include        <cstdio>
#include        <iostream>
#include        <sstream>
#include        <utility>

#include	<atomic>

#include "dab_tables.h"

using std::cerr;
using std::endl;

#include <sys/time.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>

#include <unordered_map>
#include <map>
#include <vector>
#include <algorithm>

#include "fibbits.h"
#include "proc_fig0ext6.h"

static FILE * ficout = stdout;
static uint64_t msecs_progStart = 0;
static std::atomic<bool> run;


inline void sleepMillis(unsigned ms) {
	usleep (ms * 1000);
}

inline uint64_t currentMSecsSinceEpoch() {
	struct timeval te;
	gettimeofday (&te, 0); // get current time
	uint64_t milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // calculate milliseconds
//	printf("milliseconds: %lld\n", milliseconds);
	return milliseconds;
}

inline long sinceStart (void) {
	uint64_t n = currentMSecsSinceEpoch();
	return long (n - msecs_progStart);
}

void    printOptions(const unsigned default_nFIBsToFlush)
{
	fprintf(stderr, "usage: decodefic [-v] [-b <nFIBsToFlush>] -f <fic-file>\n");
	fprintf(stderr, "\t-v                  increase verbosity\n");
	fprintf(stderr, "\t-b <nFIBsToFlush>   number for FIBs to flush decoded output\n");
	fprintf(stderr, "\t                    default: %u for 19.2 sec\n", default_nFIBsToFlush);
	fprintf(stderr, "\t-f <fic-file>       filename of raw/binary FIC input file\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Copyright (C) 2020 Hayati Ayguen\n");
	fprintf(stderr, "  site: https://github.com/hayguen/dab-cmdline\n");
	fprintf(stderr, "\n");
}


void flush_fig_processings()
{
	flush_fig0_ext6();
}



static void sighandler (int signum) {
	fprintf (stderr, "Signal caught, terminating!\n");
	run. store (false);
}


void device_eof_callback (void * userData) {
	(void)userData;
	fprintf (stderr, "\nEnd-of-File reached, triggering termination!\n");
	flush_fig_processings();
	run. store (false);
	exit (30);
}


static unsigned fibCallbackNo = 0;


static
void fib_dataHandler( const uint8_t * fib )
{
    ++fibCallbackNo;
    const uint8_t	*d	= fib;
    int processed = 0;
    while (processed < 30)
    {
        if ( d[0] == 0xFF ) // end marker?
            break;
        const int FIGtype = getMax8Bits( 3, 0, d );   // getBits_3 (d, 0);
        const int FIGlen = getMax8Bits(5, 3, d) +1;  // getBits_5 (d, 3) + 1;
        switch (FIGtype) {
        case 0: {
            int extension = getMax8Bits(5, 8 + 3, d);   //getBits_5 (d, 8 + 3);  // FIG0
            if (extension == 6)
                proc_fig0_ext6(d+1, FIGlen -1, fibCallbackNo);
            break;
        }
        case 1:
        default:
            break;
        }
        processed += FIGlen;
        d = fib + processed;
    }
}


int	main (int argc, char **argv) {
	const char * inpFileName = NULL;
	FILE * ficf = NULL;
	struct sigaction sigact;
	int		opt, verbosity = 0;
	const unsigned default_nFIBsToFlush = 200U * 36U;       // mode I has 36 FIBs per DAB/OFDM frame of size 96 ms
	unsigned	nFIBsToFlush = default_nFIBsToFlush;    // == 200 frames == 19.2 sec with 7200 FIBs of 32 Bytes each
	                                                        // ==> 230400 Bytes ~= 230 kB in 19.2 sec

	msecs_progStart = currentMSecsSinceEpoch();  // for  long sinceStart()

	if (argc == 1) {
	   printOptions(default_nFIBsToFlush);
	   exit(1);
	}

	while ((opt = getopt (argc, argv, "b:f:v")) != -1) {
	   //fprintf (stderr, "opt = %c\n", opt);
	   switch (opt) {
	      case 'b':
	         nFIBsToFlush = atoi(optarg);
	         if ( nFIBsToFlush <= 0 ) {
	            nFIBsToFlush = default_nFIBsToFlush;
	            fprintf(stderr, "invalid argument nFIBsToFlush = '%s'. setting to default %u\n", optarg, default_nFIBsToFlush);
	         }
	         break;

	      case 'f':
	         ficf = fopen(optarg, "rb");
	         inpFileName = optarg;
	         if (!ficf)
	           fprintf(stderr, "opened file '%s' for read %s\n", optarg, ((ficf)?"successful":"failed") );
	         break;

	      case 'v':
	         ++verbosity;
	         break;
	
	      default:
	         printOptions(default_nFIBsToFlush);
	         exit (1);
	   }
	}

	if ( verbosity )
	   fprintf (stderr, "decodefic - Copyright 2020 Hayati Ayguen <h_ayguen@web.de>\n");

	sigact.sa_handler = sighandler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction( SIGINT, &sigact, nullptr );

	run. store (true);
	uint8_t fib[32];
	unsigned fibNo = 0;
	unsigned nGoodCRC = 0;
	unsigned nFIBsSinceLastFlush = 0;
	while ( run.load() && !feof(ficf) )
	{
		size_t rd = fread(fib, 32, 1, ficf);
		if (!rd)
		{
			if ( verbosity )
				fprintf(stderr, "Error reading next FIB after %u FIBs\n", fibNo);
			break;
		}
		bool crcOk = check_CRC_bits(fib, 32*8);
		if ( verbosity >= 2 )
			fprintf(stderr, "FIB %u: CRC %s\n", fibNo, crcOk?"OK":"ERR");
		if (crcOk)
		{
			++nGoodCRC;
			fib_dataHandler(fib);
		}

		++nFIBsSinceLastFlush;
		++fibNo;

		if ( nFIBsSinceLastFlush >= nFIBsToFlush )
		{
			flush_fig_processings();
			nFIBsSinceLastFlush = 0;
		}

	}


	if ( verbosity )
	   fprintf(stderr, "read %u FIBs with %u good CRC in\t%s\n", fibNo, nGoodCRC, inpFileName);

	flush_fig_processings();

	return 0;
}

