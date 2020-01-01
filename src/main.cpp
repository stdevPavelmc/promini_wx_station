#include <Arduino.h>
/* Used pins:

	// the order of the info via the I2C protocol is the same of the display here

	-	// Analog pins
	A0 - Battery sensor
	A1 - LM35 Temp Sensor
	A2 - Current sensor
	A3 - Wind direction
	A6 -
	A7 -

	-	 // interrupt pins [[[ Both as pull ups resistors ]]]
	D2 - Lighting detector
	D3 - Wind speed

	-	 // I2C
	A4 - SDA I2C communication
	A5 - SCL I2C communication
	
	-	 // on/off sensors
	D4 - Rain counter
	D5 - 

	NOTE: 
	all values sent by the I2C routine are expressed in word values (two bytes)
	as a raw ADC measurement, the max adc value depends on the OverSampling defines
	for this matter the first value sent in the I2c exchange is the max adc sampling

	Un comment the line below to activate serial debug

*/

//#define DEBUG true

// low power
#include "LowPower.h"

// include the I2C lib
#include <Wire.h>

// Constants
#define BATTPIN		A0		// Battery voltage measurements
#define LM35		A1		// ADC pin for battery measurements
#define CURRENT		A2		// ADC pin for battery measurements
#define WINDDIRPIN	A3		// ADC pin for wind speed measurements
#define LIGHTNING	2		// pin to use as interrupt for lightnings
#define WINDSPEED	3		// pin to use as interrupt for wind speed
#define RAINFLIP	4		// pin to use as interrupt for rain bucket flip

/****** WARNING *****/
// the interval defined above must match the time interval of the pull in the I2C
// if not, the readings can be quite erroneous 

// ADC samples for oversampling
// how many EXTRA bits we want to get over the default 10 bits
#define ADC_OS      2   // max bits (13)
word max_samples = pow(2, (10 + ADC_OS)) - 1;

// vars
byte adccount  = 0;
word adcBatt   = 0;
word adcLM35   = 0;
int adcCurr   = 0;
word adcWinDir = 0;
word rainCount = 0;
volatile word lightning = 0;
volatile word windSpeed = 0;

// vars values to array
byte arr[2] = {0, 0};

// take samples of one ADC at a time, with oversampling if set
word takeSample(byte adc) {
    // take the adc samples
    if (ADC_OS > 0) {
        // temp var
        unsigned long total = 0;

        // oversampling (sample * 4^n)
        for (byte i = 0; i < pow(4, ADC_OS); i++)
            total += analogRead(adc);

        total = total >> ADC_OS;

        return (word)total;
    } else {
        return analogRead(adc);
    }

}

// INTERRUPT lightning strikes
void incLightning() {
	lightning++;
}

// INTERRUPT wind speed
void incWindSpeed() {
	windSpeed++;
}

// convert a word to an array of two bytes
void toArray(word value) {
	arr[0] = byte(value >> 8);
	arr[1] = byte(value & 0xFF);
}

// reset values that must be cleared up on informed
void resetValues() {
	// lightning
	lightning = 0;
	windSpeed = 0;
	rainCount = 0;
}

// answer to I2C readings 
void requestEvent() {
	// need to send a few values, this is the order
	// the order is the same os the listing in the 
	// comments on to of the code

	// max sampling
	// this is a measure to inform to the ESP8266 about the maximum
	// value of the ADC measurements
	toArray(max_samples);
	Wire.write(arr, 2);

	// battery
	toArray(adcBatt);
	Wire.write(arr, 2);

	// Temp LM35
	toArray(adcLM35);
	Wire.write(arr, 2);

	// current
	toArray(adcCurr);
	Wire.write(arr, 2);

	// windir
	toArray(adcWinDir);
	Wire.write(arr, 2);

	// lightning
	toArray(lightning);
	Wire.write(arr, 2);

	// windSpeed
	toArray(windSpeed);
	Wire.write(arr, 2);

	// raincount
	toArray(rainCount);
	Wire.write(arr, 2);

	// reset the values that are read as unique each time
	resetValues();
}

// software interrupt check for the bucket flip
void softInt() {
	static bool lastStatus = 0;
	bool state = digitalRead(RAINFLIP);

	if (lastStatus != state) {
		rainCount++;
		lastStatus = state;
	}
} 

// setup
void setup() {
	// Serial
	#ifdef DEBUG
	Serial.begin(115200);
	Serial.println(" ");
	#endif

	// declare the pins as input
	pinMode(BATTPIN, INPUT);	// battery
	pinMode(LM35, INPUT);		// temp M35
	pinMode(CURRENT, INPUT);	// current
	pinMode(WINDDIRPIN, INPUT);	// wind direction
	pinMode(RAINFLIP, INPUT);	// rain count of flipped buckets

	// interrupts settings (RAISING, FALLING, CHANGE)
	attachInterrupt(digitalPinToInterrupt(LIGHTNING), incLightning, FALLING);
	attachInterrupt(digitalPinToInterrupt(WINDSPEED), incWindSpeed, FALLING);

	Wire.begin(0x21);                
	Wire.onRequest(requestEvent);
}

// loop
void loop() {
	// low p0wer, sleep for 120msec, with the TWI on and attending irqs
	LowPower.idle(SLEEP_120MS, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_OFF, USART0_OFF, TWI_ON);
	// LowPower.idle(SLEEP_1S, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_OFF, USART0_OFF, TWI_ON);

	// adc measurements only about 1 second 
	if (adccount >= 7 ) {
		// reset count 
		adccount = 0;

		// measure battery voltage
		adcBatt = word(takeSample(BATTPIN));

		// temperature from LM35
		adcLM35 = word(takeSample(LM35));

		// temperature from LM35
		adcCurr = word(takeSample(CURRENT));
		
		// temperature from LM35
		adcWinDir = word(takeSample(WINDDIRPIN));

		#ifdef DEBUG
			// calc voltage
			float V = adcBatt * 4.934 * 3.102 / max_samples;
			Serial.print("Vbat: ");
			Serial.println(V);
		#endif
	} else {
		adccount++;
	}
}