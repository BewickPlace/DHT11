/*
copyright (c) 2017-  by John Chandler

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
#include <stdint.h>
#include <string.h>
#include <errno.h>


#include <pthread.h>
#include <wiringPi.h>
#include "errorcheck.h"
#include "config.h"

//
//	Button Device variables
//

#define BUTTON_READ_PIN		5		// Read & Write Pins for illuminated button
#define BUTTON_WRITE_PIN	7
#define BUTTON_LONG_PRESS	3000		// 3 sec long press

typedef struct {
	int last_pin_state;			// last know pin state
	int new_pin_state;			// latest pin state
	unsigned int edge1;			// edge timer
	unsigned int edge2;			// edge timer
    } pin;

static volatile pin Button_pin;			// Button related pin details

#define MAX_PULSE_RESPONSES	3		// DHT11 response pulses - transitions to be measured
#define MAX_PULSE_DATA		40		// DHT11 data pulses
#define MAX_PULSE_TIMINGS	(MAX_PULSE_RESPONSES+(2*MAX_PULSE_DATA)) // Maximum number of pulse transitions Response Pulses (2*2) + Data (2*40)
#define MAX_PULSE_WIDTH		49		// Maximum pulse width on data pulses
#define DHT_PIN			8		// DHT control pin (physical Pin number)
#define DHT_PRIORITY		90		// DHT process priority


static int last_pin_state;			// last known state of DHT11 pin
static uint8_t pulse_count;			// count of response pulses
static  int timings[MAX_PULSE_TIMINGS]; 	// record of pulse duration

static int dht11_data[5] = { 0, 0, 0, 0, 0 };

static int read_count;
static int success_count;
static int crc_count;
static int tot_read_count;
static int tot_success_count;
static int tot_crc_count;


//
//	Button Interrupot Handler
//
void	Button_interrupt() {

    piHiPri(DHT_PRIORITY);			// ensure interrupt is given highest priority
    Button_pin.new_pin_state = digitalRead(BUTTON_READ_PIN);// check the new state of the pin
    if (Button_pin.new_pin_state == Button_pin.last_pin_state) { goto EndError; } // debounce...
    Button_pin.edge2 = millis();		// record edge timestamp

    if (Button_pin.new_pin_state == HIGH) {	// if this is the transition back to high
						// handle the button press
        if ((Button_pin.edge2 - Button_pin.edge1) > BUTTON_LONG_PRESS) { // Check for LONG press
	    debug(DEBUG_TRACE, "Button - Long Press\n");
	} else {
	    debug(DEBUG_TRACE, "Button - Short Press\n");
	};
    };
    Button_pin.last_pin_state = Button_pin.new_pin_state;  // update last known pin status
    Button_pin.edge1 = Button_pin.edge2;

ENDERROR;
}

//
//	DHT Signal Read Request
//

void	dht_signal_read_request() {
    int	new_pin_state;					// Latest state of DHT11 pin
    uint8_t pulse_width;				// count of usec pulse width
    read_count++;
    int i;
							// initialise data handling variables
    for ( i = 0; i < MAX_PULSE_TIMINGS; i++ ) { timings[i] = 0; } // record of pulse durations

							// ADJUSTED to maximise performace
    last_pin_state = LOW;				// Assume we will miss first edge transition
							// and start looking from next transition to high

    pinMode( DHT_PIN, OUTPUT );				// Signal to DHT11 read request
    digitalWrite( DHT_PIN, LOW );
    delay( 18 );
    digitalWrite( DHT_PIN, HIGH );
    pinMode( DHT_PIN, INPUT );
    delayMicroseconds( 20 );


    for (pulse_count = 0; pulse_count < MAX_PULSE_TIMINGS; pulse_count++) { // For all of pulses in the expected pulse train
	pulse_width = 0;
	new_pin_state = digitalRead(DHT_PIN);		// check the new state of the pin
	while ((new_pin_state == last_pin_state)  &&	// loop measuing the width of the pulse in usec
	       (pulse_width < MAX_PULSE_WIDTH)) {	// stop if pulse width too high (implies missed pulse transition)
	    delayMicroseconds(1);
	    pulse_width++;
	    new_pin_state = digitalRead(DHT_PIN);	// check the new state of the pin
	}
 	timings[pulse_count] = pulse_width;		// save for later analysis
	if (pulse_width == MAX_PULSE_WIDTH) { break; }	// drop out if invalid pulse
	last_pin_state = new_pin_state;			// update last known pin status
    }
}

//
//	display captured timings
//


void	display_timings() {
    int i;
    char string[(30+(3*(MAX_PULSE_TIMINGS/2)))];// Risky string length - CAREFUL  if you change strings
    char reading[5];				// Reading: 2 digits + ":" + NULL + 1 spare

    debug(DEBUG_TRACE, "Pulse: %d\n", pulse_count);
    sprintf(string, "Timings:   Low - ");
    for ( i = 0; i < MAX_PULSE_TIMINGS; i+=2 ) { sprintf(reading, "%2d:", timings[i]); strcat(string, reading); }
    strcat(string, "\n");
    debug(DEBUG_TRACE, string);

    sprintf(string, "           High- ");
    for ( i = 1; i < MAX_PULSE_TIMINGS; i+=2 ) { sprintf(reading, "%2d:", timings[i]); strcat(string, reading); }
    strcat(string, "\n");
    debug(DEBUG_TRACE, string);
    debug(DEBUG_TRACE, "           DHT data [%d]{%d][%d][%d][%d]\n", dht11_data[0], dht11_data[1], dht11_data[2], dht11_data[3], dht11_data[4]);
}

#define NUMDEV	4				// Number of possible device types
#define DEVLEN  10				// Device Revision length

struct devblock {				// Device Block
    char revision[DEVLEN];			// Revision Id (from CPUinfo
    int  dht;					// DHT threshold for this device revision
};

struct devblock	devices[NUMDEV] =	{{"0008",    13},
					 {"0010",    13},
					 {"9000c1",  17},
					 {"unknown", 17}};

static int	dht_threshold;			// High/Low threshold
static int	min_high = MAX_PULSE_WIDTH;
static int	max_low = 0;
static int	tot_max_low = 0;
//
//	Set DHT threshold
//
void	set_dht_threshold() {
    char rev[DEVLEN];				// Device revision
    int	 i = 0;					// index

    PiRevision(rev);				// Obtain current device Revision from CPUinfo
    while (( i < NUMDEV) &&
	   ( strcmp(rev, devices[i].revision) != 0)) { // Find match or unknown entry
	i++;
    }
    if (i >= NUMDEV) { i = NUMDEV -1;}		// if not found use last entry in the table
    dht_threshold = devices[i].dht;		// set DHT threshold accordingly
    debug(DEBUG_TRACE, "DHT11 Threshold set to %d based on revision %s\n", dht_threshold, devices[i].revision);
}

//
//	Parse Data
//
int	parse_data(int dht) {
    int data_count = 0;
    int	i;
    int ret;

    min_high = MAX_PULSE_WIDTH;
    max_low = 0;
    for (i=0; i < 5; i++) dht11_data[i] = 0;	// initialise data

    for (i = MAX_PULSE_RESPONSES; i < pulse_count; i++) { // for each of the pulses (ignoring startup pulses)
	if (i % 2 == 1) {			// place bit in appropriate data array
	    dht11_data[data_count / 8] <<= 1;	// check pulse width for data 1 or 0
	    if ( timings[i] > dht ) {
		dht11_data[data_count / 8] |= 1;
		min_high = (timings[i] < min_high ? timings[i] : min_high);
	    } else {
		max_low  = (timings[i] > max_low  ? timings[i] : max_low );
	    }
	    data_count++;
	}
    }
    ret = (dht11_data[4] == ((dht11_data[0] + dht11_data[1] + dht11_data[2] + dht11_data[3]) & 0xFF));
    debug(DEBUG_TRACE, "DHT11 Result %d T/L/H [%2d:%2d:%2d] Data:[%3d][%3d][%3d][%3d][%3d]\n", ret, dht, max_low, min_high, dht11_data[0], dht11_data[1], dht11_data[2], dht11_data[3], dht11_data[4]);
    return(ret);
}
//
//	Interpret DHT response tinings & extract data
//

int	dht_interpret_data() {
    int good = 0;
    int dht_reparse_low;
    int dht_reparse_high;
						// Throws out incomplete data
    if (pulse_count <((2* MAX_PULSE_DATA)+MAX_PULSE_RESPONSES)) goto ReadError;

    good = parse_data(dht_threshold);		// Parse good data
    if (good) {					// If OK

    } else {					// otherwise
	dht_reparse_low = max_low - 1;
	dht_reparse_high= min_high + 0;
	if ((!good) &&
	    (dht_reparse_high <= (dht_threshold+2))) {// possible higher match
	    good = parse_data(dht_reparse_high);// Reparse using higher threshold
	    if (good) { debug(DEBUG_TRACE, "DHT11 Re-parsed, T   [%d>>%d]\n", dht_threshold, dht_reparse_high);}
	}
	if ((!good) &&
	    (dht_reparse_low >= dht_threshold-5)) {// possible lower match
	    good = parse_data(dht_reparse_low);	// Reparse using lower threshold
	    if (good) { debug(DEBUG_TRACE, "DHT11 Re-parsed, T   [%d<<%d]\n", dht_reparse_low, dht_threshold);}
	}
    }
    if (!good) goto CRCError;			// If still no good CRC error
    success_count++;
    tot_max_low =  tot_max_low + max_low;

ERRORBLOCK(ReadError);
    debug(DEBUG_INFO, "DHT11 Incomplete Read data, count[%d/%d]\n", pulse_count, MAX_PULSE_TIMINGS);
    DEBUG_FUNCTION( DEBUG_DETAIL, display_timings());
    return(0);
ERRORBLOCK(CRCError);
    debug(DEBUG_TRACE, "DHT11 CRC error, L/H [%d]\n", dht_threshold);
    DEBUG_FUNCTION( DEBUG_INFO, display_timings());
    crc_count++;
    return(0);
ENDERROR;
    DEBUG_FUNCTION( DEBUG_DETAIL, display_timings());
    return(1);
}

//
//	Read DHT11 Pressure & Temperature sensor device
//

#define		MAX_DHT_RETRYS	10		// Maximum nuber of reties to get a valid reading
#define         DHT_SIGN	0x80		// DHT22 Sign bit
#define         DHT_DATA	0x7F		// DHT22 Data bits

void read_dht11() {
    int	i;
    int raw_temperature;

    dht_signal_read_request();			// Signal to DHT11 read request
    i= 0;
    while ((!dht_interpret_data()) && 		// Interpret the data, check for completeness and CRC
	   (i < MAX_DHT_RETRYS)) {		// Too many Data Errors
	i++;					// Increment error count
	delay(2000);				// allow DHT11 to stabalise
	dht_signal_read_request();		// and retry Read request
    }
    ERRORCHECK(i== MAX_DHT_RETRYS, "DHT11 Persistant Read Failure", ReadError);  // Fail

    //	Support for DHT11 or DHT22 devices
    //	DHT11 - uses data[2].data[3]
    //  DHT22 - uses data[2]*256 + data[3] /10, plus sign bit in data[2]
    //

    if ((dht11_data[2] & DHT_DATA) < 30) {	// Data looks valid (have encountered some problems with adaptive crc check)
	if ((dht11_data[2] & DHT_DATA) < 4) {	// Device is most likely a DHT22
	    raw_temperature = ((dht11_data[2] & DHT_DATA) << 8) + dht11_data[3];
	    if(dht11_data[2] & DHT_SIGN) { raw_temperature  = -raw_temperature;}
	    debug(DEBUG_INFO, "DHT Device DHT22 [%d.%d] => %d.%d\n", dht11_data[2], dht11_data[3], raw_temperature/10, abs(raw_temperature%10));

	} else {				// Device is most liekly a DHT11
	    raw_temperature = (dht11_data[2] * 10) + dht11_data[3];
	    debug(DEBUG_INFO, "DHT Device DHT11 [%d.%d] => %d.%d\n", dht11_data[2], dht11_data[3], raw_temperature/10, abs(raw_temperature%10));
	}
//	app.temp = (float) raw_temperature / 10.0;

    } else {					// Data is out of realistic range - don't chage reported temp
	warn("DHT11 temperature out of range [%d.%d] - ignored", dht11_data[2], dht11_data[3]);
    }

ERRORBLOCK(ReadError);
//    app.temp = -0.1;
ENDERROR;
}

#define	DHT11_OVERALL	(120)			// DHT11 Cycle overall timer
#define DHT11_READ	(4)			// DHT11 Read cycle
#define DHT11_CYCLES	(5)			// Number of cycles in test

//
//	Initialise wiring Pi
//
void	initialise_GPIO() {
    if ( wiringPiSetupPhys() == -1 )
	exit( 1 );
}
//
//	Monitor Sensor Process
//

int	main(void)	{
    int	rc = -1;
    int cycles;
    int cycle_time = DHT11_OVERALL;
    float efficiency;

    printf( "Raspberry Pi wiringPi DHT11 Temperature test program\n" );
    initialise_GPIO();
    debuglev = DEBUG_TRACE;			// Always TRACE within test program

    read_count = 0;				// Initialise read status
    success_count = 0;
    crc_count = 0;
    tot_read_count = 0;				// Initialise read status
    tot_success_count = 0;
    tot_crc_count = 0;

    Button_pin.last_pin_state = HIGH;		// last known pin state
    Button_pin.edge1 = millis();		// record starting edge timestamp
    rc = wiringPiISR(BUTTON_READ_PIN, INT_EDGE_BOTH, &Button_interrupt);  // Interrupt on rise or fall of DHT Pin
    ERRORCHECK( rc < 0, "DHT Error - Pi ISR problem", EndError);

    if (piHiPri(DHT_PRIORITY) < 0) debug(DEBUG_ESSENTIAL, "Error setting priority: %d\n", errno);	// ensure thread is given highest priority

    set_dht_threshold();			// Set the threshold for DHT data values
    delay(2000);				// Allow time for DHT11 to settle
    for(cycles = 0; cycles < DHT11_CYCLES; cycles++){
    cycle_time = 1;

    while ( cycle_time )	{
	if ((cycle_time % DHT11_READ) == 0) {		// Every x seconds
	    read_dht11();
	}
	delay(1000);
	cycle_time =( cycle_time + 1) % DHT11_OVERALL;
    }
    efficiency = ((float)success_count/ (float)read_count)* 100.0;
    debug(DEBUG_ESSENTIAL, "DHT11 efficiency %2.0f%, read[%d], ok[%d], crc[%d] L/H[%d>>%d]\n", efficiency, read_count, success_count, crc_count, dht_threshold,
												(tot_max_low/success_count)+4);
    dht_threshold = (tot_max_low/success_count) + 4;
    tot_read_count = tot_read_count + read_count;
    tot_success_count = tot_success_count + success_count;
    tot_crc_count = tot_crc_count + crc_count;
    read_count = 0;
    success_count = 0;
    crc_count = 0;
    tot_max_low = 0;
    }
    efficiency = ((float)tot_success_count/ (float)tot_read_count)* 100.0;
    debug(DEBUG_ESSENTIAL, "DHT11 Overall efficiency %2.0f%, read[%d], ok[%d], crc[%d] L/H[%d]\n", efficiency, tot_read_count, tot_success_count, tot_crc_count, dht_threshold);

ENDERROR;
    return(0);
}
