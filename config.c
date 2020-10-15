/*
Copyright (c) 2014-  by John Chandler

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
#include <getopt.h>
#include <sys/stat.h>

#include "config.h"
#include "errorcheck.h"

//
//	Local impleentation of Strpos
//
int	strpos(char *haystack, char *needle, int offset) {
    char *p = strstr((haystack+offset), needle);

    if (p)  return(p-haystack); else return(-1);
}

//
//	Extract the element in quotes after the defined key
//	Extract looking for newline if quotes not found
//

char	*find_key(char *haystack, char *key, char *field, char *upper) {
    int	o, x, y;

    o = strpos(haystack, key, 0);
    if ((o == -1) | (o > (upper - haystack))) return(NULL);
    o = o + strlen(key);
    x = strpos(haystack, "\"", o)+1;
    if (x > o) {
	y = strpos(haystack, "\"", x);
    } else {
	x = o;
   	y = strpos(haystack, "\n", x);
    }
    memcpy(field, haystack + x, y-x);
    field[y-x] = '\0';
    return (haystack + y);
}
//
//	Find Time
//
char	*find_time(char *haystack, char *key, time_t *time, char *upper) {
    char *p;
    char time_string[20];
    int	hours, minutes;

    p =  find_key(haystack, key, time_string, upper);
    sscanf(time_string, "%d:%d", &hours, &minutes);
    *time = (((hours * 60) + minutes)*60);

    return(p);
}
//
//	Extract the element in quotes after the defined key
//	in the format HH:MM, xx.y
//

char	*find_change(char *haystack, char *key, time_t *time, float *setpoint, char *upper) {
    int	o, x, y;
    char change[20];
    int	hours, minutes;

    o = strpos(haystack, key, 0);
    if ((o == -1) | (o > (upper-haystack))) return(NULL);
    o = o + strlen(key);
    x = strpos(haystack, "\"", o)+1;
    if (x > o) {
	y = strpos(haystack, "\"", x);
    } else {
	x = o;
   	y = strpos(haystack, "\n", x);
    }
    memcpy(change, haystack + x, y-x);
    change[y-x] = '\0';
    sscanf(change, "%d:%d, %f", &hours, &minutes, setpoint);
    *time = (((hours * 60) + minutes)*60);

    return (haystack + y);
}

//
//	Find Descriptor Block
//
char	*find_block(char *haystack, char *needle){
    char	*p;

    p = strstr(haystack, needle);
    if (p!=NULL) p = p + strlen(needle);
    return(p);
}


#define MAX_CONFIG_DATA 500
#define ENTRIES         6

//
//	Read Pi Revision code from /proc/CPUinfo
//
void PiRevision(char *rev) {
    FILE	*fp;					// File Descriptor
    char	file_data[MAX_CONFIG_DATA][1];		// Buffer for file data
    size_t	size;					// size in bytes of data read
    char	*p;					// data pointer

    fp = fopen("/proc/cpuinfo", "r");			// Open processor config file read only
    ERRORCHECK(fp==NULL, "CPUinfo Open Error", EndError);

    size = fread(file_data, MAX_CONFIG_DATA, ENTRIES, fp);	// Read the file into memory
    ERRORCHECK( size < 0, "CPUinfo Read Error", ReadError);

    p = find_block((char *)file_data, "Revision");		// Find the revision line
    ERRORCHECK(p==NULL, "CPUinfo Revision not found", ReadError);
    p = find_block(p, ": ");				// Find the revision line
    ERRORCHECK(p==NULL, "CPUinfo Revision not found", ReadError);

    sscanf(p,"%s", rev);				// Extract the string after the colon
    fclose(fp);


ERRORBLOCK(ReadError);
    fclose(fp);
ENDERROR;
}
