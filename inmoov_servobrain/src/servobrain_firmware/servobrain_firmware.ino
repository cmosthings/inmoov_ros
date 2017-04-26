/**
   Pin notes by Suovula, see also http://hlt.media.mit.edu/?p=1229

   DIP and SOIC have same pinout, however the SOIC chips are much cheaper, especially if you buy more than 5 at a time
   For nice breakout boards see https://github.com/rambo/attiny_boards

   Basically the arduino pin numbers map directly to the PORTB bit numbers.

  // I2C
  arduino pin 0 = not(OC1A) = PORTB <- _BV(0) = SOIC pin 5 (I2C SDA)
  arduino pin 2 =           = PORTB <- _BV(2) = SOIC pin 7 (I2C SCL)
  // Timer1 -> PWM
  arduino pin 1 =     OC1A  = PORTB <- _BV(1) = SOIC pin 6 (LED)
  arduino pin 3 = not(OC1B) = PORTB <- _BV(3) = SOIC pin 2 (Servo Pulse)
  arduino pin 4 =     OC1B  = PORTB <- _BV(4) = SOIC pin 3 (ADC Servo)
  arduino pin 5 =     RESET            _BC(5) = SOIC pin 1 (RESET)

  Digital 2 is analog (ADC channel) 1
  Digital 3 is analog (ADC channel) 3
  Digital 4 is analog (ADC channel) 2
  Digital 5 is analog (ADC channel) 0
*/

// Get this from https://github.com/rambo/TinyWire
#include <TinyWireS.h>
//#include <EEPROM.h>
#include "SoftServo.h"
#include "protocol.h"

// The default buffer size, Can't recall the scope of defines right now
#ifndef TWI_RX_BUFFER_SIZE
#define TWI_RX_BUFFER_SIZE ( 32 )
#endif

#ifndef TWI_TX_BUFFER_SIZE
#define TWI_TX_BUFFER_SIZE ( 32 )
#endif

// Proof of Concept Configuration
//#define LED     1  // no LED in POC
#define SERVOPIN  1
#define SENSORPIN 0 //digital 5 is adc channel 0

#define INTERNAL2V56_NO_CAP (6)

/* Prototype Configuration
  #define LED       1  // no LED in POC
  #define SERVOPIN  3
  #define SENSORPIN 2 //digital 4 is adc channel 2
*/

SoftServo servo;  //FIXME no need to make this a class


short pos = 0;
short Commandpos = 0;
short Position = 0;

//#define WEEPROM     47  //flag for which eeprom value needs to be written out

#define EEPROM_SIZE 64

short reg[EEPROM_SIZE];  // all configurable values for smartservo
byte i2c_reg;            // which register is subject for r/w

byte excessbyte;

union CShort {
  byte b[2];
  signed short val;
} cshort;


unsigned long timermillis = 0;


/**
   This is called for each read request we receive, never put more than one byte of data (with TinyWireS.send) to the
   send-buffer when using this callback
*/
void requestEvent()
{
  cshort.val = reg[i2c_reg];
  TinyWireS.send(cshort.b[0]);
  TinyWireS.send(cshort.b[1]);

}



/**
   The I2C data received -handler

   This needs to complete before the next incoming transaction (start, data, restart/stop) on the bus does
   so be quick, set flags for long running tasks to be called from the mainloop instead of running them directly,
*/
void receiveEvent(uint8_t howMany)
{
  //set register for i2c r/w operation
  i2c_reg = TinyWireS.receive();
  reg[VALUE1] = i2c_reg;
  howMany--;

  // now, if there are two more bytes, then this is a write operation
  while (howMany > 0) {
    cshort.b[0] = TinyWireS.receive();
    cshort.b[1] = TinyWireS.receive();

    reg[i2c_reg] = cshort.val;
    reg[VALUE2] = cshort.val;
    //    if (i2c_reg < 32){
    //      EEPROM.put(0,reg);
    //    }

    howMany -= 2;

    if (howMany > 0) {
      excessbyte = TinyWireS.receive();
      reg[VALUE5]++;
      howMany--;
    }
  }
}

void setup()
{
  pinMode(SERVOPIN, OUTPUT);
  digitalWrite(SERVOPIN, LOW);
  pinMode(SENSORPIN, INPUT);

  //EEPROM.get(0,reg);

  reg[ID]         = 8;
  reg[GOAL]       = 0 * 100;       //goals are always * 100, short with two decimal places
  reg[MINGOAL]    = -90 * 100;     //goals are always * 100, short with two decimal places
  reg[MAXGOAL]    = 90 * 100;      //goals are always * 100, short with two decimal places
  reg[MINPULSE]   = 660;
  reg[MAXPULSE]   = 2325;
  reg[MINSENSOR]  = 204;
  reg[MAXSENSOR]  = 867;
  reg[ENABLED]    = 1;
  reg[POWER]      = 1;
  reg[HEARTBEAT]  = B10100000;
  reg[MAXTEMP]    = 40;      // hardcode to 40c for now
  reg[VALUE5]     = 0;

  TinyWireS.begin(8);
  TinyWireS.onReceive(receiveEvent);
  TinyWireS.onRequest(requestEvent);

  analogReference( INTERNAL2V56_NO_CAP );

  // Set up the interrupt that will refresh the servo for us automagically
  OCR0A = 0xAF;            // any number is OK
  TIMSK |= _BV(OCIE0A);    // Turn on the compare interrupt (below!)

  servo.attach(SERVOPIN);   // Attach the servo to pin 0 on Trinket
  servo.write(reg[GOAL]);           // Tell servo to go to position per quirk

  //initialize timermillis
  timermillis = millis();
}

#define MAX_SAMPLES 20
#define BATCH_SAMPLES 20
#define SHIFT 3

unsigned short sampleCount = 0;
unsigned long int sampleBucket = 0;
long int samplestart = 0;

void updatePos() {
  for (int i = 0; i < BATCH_SAMPLES; i++) {
    sampleBucket += analogRead(SENSORPIN);
    TinyWireS_stop_check();
  }
  sampleCount += BATCH_SAMPLES;

  if (sampleCount >= MAX_SAMPLES) {
    sampleBucket /= MAX_SAMPLES;
    reg[RAWPOSITION] = sampleBucket;
    reg[POSITION] = map(sampleBucket, reg[MINSENSOR], reg[MAXSENSOR], reg[MINGOAL], reg[MAXGOAL]);
    //reg[VALUE1] = millis() - samplestart;
    samplestart = millis();
    sampleBucket = 0.0f;
    sampleCount = 0;
  }
}

short get_temp() {
  analogReference(INTERNAL1V1);
  short raw = analogRead(A0 + 15);
  /* Original code used a 13 deg adjustment. But based on my results, I didn't seem to need it. */
  //raw -= 13; // raw adjust = kelvin //this value is used to calibrate to your chip
  short in_c = raw - 273; // celcius
  analogReference(INTERNAL2V56_NO_CAP);
  return in_c;
}

void loop()
{
  /**
     This is the only way we can detect stop condition (http://www.avrfreaks.net/index.php?name=PNphpBB2&file=viewtopic&p=984716&sid=82e9dc7299a8243b86cf7969dd41b5b5#984716)
     it needs to be called in a very tight loop in order not to miss any (REMINDER: Do *not* use delay() anywhere, use tws_delay() instead).
     It will call the function registered via TinyWireS.onReceive(); if there is data in the buffer on stop.
  */
  TinyWireS_stop_check();



}

// We'll take advantage of the built in millis() timer that goes off
// to keep track of time, and refresh the servo every 20 milliseconds

// I think, seems right, need to look into timer settings
//#define INTERVAL 20  // for 16mhz clock?
#define INTERVAL 10    // for 8mhz clock?
volatile uint8_t counter = 0;
SIGNAL(TIMER0_COMPA_vect) {
  // this gets called every 2 milliseconds
  counter += 1;
  // every 20 milliseconds, refresh the servos!
  if (counter >= INTERVAL) {
    counter = 0;
    if (reg[ENABLED]) {
      servo.writeMicroseconds(map(reg[GOAL], reg[MINGOAL], reg[MAXGOAL], reg[MINPULSE], reg[MAXPULSE]));
      reg[VALUE4] = map(reg[GOAL], reg[MINGOAL], reg[MAXGOAL], reg[MINPULSE], reg[MAXPULSE]);
      servo.refresh();
    }

    updatePos();

    reg[TEMP] = get_temp();

    if (reg[TEMP] >= reg[MAXTEMP]) {
      reg[ENABLED] = 0;
    }

  }
}