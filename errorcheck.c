/*
Copyright (c) 2014- by John Chandler

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
//#include <getopt.h>

//#include <unistd.h>
//#include <signal.h>
//#include <assert.h>
//#include <fcntl.h>
//#include <sys/time.h>
#include <time.h>
//#include <sys/types.h>
//#include <sys/socket.h>
//#include <sys/uio.h>
//#include <netinet/in.h>
//#include <netinet/ip.h>
//#include <arpa/inet.h>
//#include <net/if.h>

#include "errorcheck.h"

int	debuglev = DEBUG_ESSENTIAL;		// Default to only essential debug level (unless changed by command line)

void die(char *format, ...) {
    fprintf(stderr, "FATAL: ");

    va_list args;
    va_start(args, format);

    vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n");
    exit(1);
}

void warn(char *format, ...) {
    time_t rawtime;

    time( &rawtime);
    fprintf(stderr,  "%s WARNING: ", strtok(ctime(&rawtime),"\n"));
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
}

void debug(int level, char *format, ...) {
    time_t rawtime;
    if (level > debuglev)
        return;

    time( &rawtime);
    fprintf(stderr,  "%s DEBUG: ", strtok(ctime(&rawtime),"\n"));
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}
