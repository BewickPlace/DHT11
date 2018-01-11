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

#define ERRORCHECK(Condition, Message, Label) \
	if (Condition) { warn(Message); goto Label; }

#define ERRORBLOCK(Label) \
	goto EndError;    \
	Label:;

#define ENDERROR  EndError: ;

#define	DEBUG_ESSENTIAL	0					// Debug error levels (set by command line)
#define	DEBUG_TRACE	1
#define	DEBUG_DETAIL	2

extern int debuglev;
void die(char *format, ...);
void warn(char *format, ...);
void debug(int level, char *format, ...);

#define DEBUG_FUNCTION(Level, Function) \
	if (Level <= debuglev) { Function; }
