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

#include <wiringPi.h>
#include "errorcheck.h"

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

//
//	DHT11 device variables
//

#define MAX_PULSE_RESPONSES	3		// DHT11 response pulses - transitions to be measured
#define MAX_PULSE_DATA		40		// DHT11 data pulses
#define MAX_PULSE_TIMINGS	84		// Maximum number of pulse transitions Response Pulses (2*2) + Data (2*40)
#define MAX_PULSE_WIDTH		99		// Maximum pulse width on data pulses
#define DHT_PIN			8		// DHT control pin (physical Pin number)
#define DHT_PRIORITY		10		// DHT process priority


static int last_pin_state;			// last known state of DHT11 pin
static uint8_t pulse_count;			// count of response pulses
static  int timings[MAX_PULSE_TIMINGS]; 	// record of pulse duration

static int dht11_data[5] = { 0, 0, 0, 0, 0 };

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

							// ADJUSTED to maximise performace
    last_pin_state = LOW;				// Assume we will miss first edge transition
							// and start looking from next transition to high

    pinMode( DHT_PIN, OUTPUT );				// Signal to DHT11 read request
    digitalWrite( DHT_PIN, LOW );
    delay( 18 );
    digitalWrite( DHT_PIN, HIGH );
    delayMicroseconds( 30 );
    pinMode( DHT_PIN, INPUT );

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
//	Interpret DHT response tinings & extract data
//

int	dht_interpret_data() {
    int data_count = 0;
    int	i;

    for (i=0; i < 5; i++) dht11_data[i] = 0;	// initialise data

    for (i = MAX_PULSE_RESPONSES; i < pulse_count; i++) { // for each of the pulses (ignoring startup pulses)
	if (i % 2 == 1) {			// place bit in appropriate data array
	    dht11_data[data_count / 8] <<= 1;	// check pulse width for data 1 or 0
	    if ( timings[i] > 16 ) { dht11_data[data_count / 8] |= 1; }
	    data_count++;
	}
    }

    if (data_count < MAX_PULSE_DATA) goto ReadError;
    if (dht11_data[4] != ((dht11_data[0] + dht11_data[1] + dht11_data[2] + dht11_data[3]) & 0xFF)) goto CRCError;

ERRORBLOCK(ReadError);
    char	string[(20+3*MAX_PULSE_TIMINGS)];	// Risky string length - CAREFUL  if you change strings

    debug(DEBUG_TRACE, "DHT11 Incomplete Read data\n");
    sprintf(string, "Timings:   Low - ");
    for ( i = 0; i < MAX_PULSE_TIMINGS; i+=2 ) { sprintf(string, "%s%2d:", string, timings[i]); }
    sprintf(string, "%s\n", string);
    debug(DEBUG_DETAIL, string);

    sprintf(string, "           High- ");
    for ( i = 1; i < MAX_PULSE_TIMINGS; i+=2 ) { sprintf(string, "%s%2d:", string, timings[i]); }
    sprintf(string, "%s\n", string);
    debug(DEBUG_DETAIL, string);

    debug(DEBUG_DETAIL, "Pulse & Data: %d:%d\n", pulse_count, data_count);

    return(0);
ERRORBLOCK(CRCError);
    debug(DEBUG_TRACE, "DHT11 CRC error\n");
    return(0);
ENDERROR;
    return(1);
}

//
//	Read DHT11 Pressure & Temperature sensor device
//

#define		MAX_DHT_RETRYS	10		// Maximum nuber of reties to get a valid reading

void read_dht11() {
    int	i;
    float	f;
						// initialise data handling variables
    for ( i = 0; i < MAX_PULSE_TIMINGS; i++ ) { timings[i] = 0; } // record of pulse durations

    piHiPri(DHT_PRIORITY);			// ensure thread is given highest priority
    pinMode(BUTTON_WRITE_PIN, OUTPUT );
    digitalWrite(BUTTON_WRITE_PIN, 1);
    dht_signal_read_request();			// Signal to DHT11 read request
    i= 0;
    while ((!dht_interpret_data()) && 		// Interpret the data, check for completeness and CRC
	   (i < MAX_DHT_RETRYS)) {		// Too many Data Errors
	i++;					// Increment error count
	delay(2000);				// allow DHT11 to stabalise
	dht_signal_read_request();		// and retry Read request
    }
    ERRORCHECK(i== MAX_DHT_RETRYS, "DHT11 Persistant Read Faulure", EndError);
    f = dht11_data[2] * 9. / 5. + 32;
    debug(DEBUG_ESSENTIAL, "Humidity = %d.%d %% Temperature = %d.%d C (%.1f F)\n", dht11_data[0], dht11_data[1], dht11_data[2], dht11_data[3], f );

ENDERROR;
    digitalWrite(BUTTON_WRITE_PIN, 0);
}

//
//	DHT11 Example test programe
//

int main( void )
{
    int rc;

    printf( "Raspberry Pi wiringPi DHT11 Temperature test program\n" );

    if ( wiringPiSetupPhys() == -1 )
	exit( 1 );

    debuglev = DEBUG_TRACE;			// Always TRACE within test program

    Button_pin.last_pin_state = HIGH;		// last known pin state
    Button_pin.edge1 = millis();		// record starting edge timestamp
    rc = wiringPiISR(BUTTON_READ_PIN, INT_EDGE_BOTH, &Button_interrupt);  // Interrupt on rise or fall of DHT Pin
    ERRORCHECK( rc < 0, "DHT Error - Pi ISR problem", EndError);
    delay( 2000 );				// Allow time for DHT11 to settle

    while ( 1 )	{
	read_dht11();
	delay( 10000 );
    }

ENDERROR;
    return(0);
}
