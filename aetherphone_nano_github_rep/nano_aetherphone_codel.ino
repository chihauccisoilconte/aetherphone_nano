#include <MozziGuts.h>

/*  

 This is the code used to program the Nano Aetherphone.
 It is based on the original code by Tim Barrass (2012-2016, CC by-nc-sa) and 
    
 HC-SR04 code informed by this: https://arduinobasics.blogspot.com.au/2012/11/arduinobasics-hc-sr04-ultrasonic-sensor.html
 
 Circuit: Audio output on digital pin 9 on a Uno or similar, or
 DAC/A14 on Teensy 3.1, or 
 check the README or http://sensorium.github.com/Mozzi/
 
 HC-SR04 sensor circuit:
 VCC to arduino 5v 
 GND to arduino GND
 Echo to Arduino pin 4
 Trig to Arduino pin 2
 LDR to Arduino pin A0
 9V battery to Vin
 LED to Arduino pin 10
 
check the schematics for more details - 
 
 Mozzi help/discussion/announcements:
 https://groups.google.com/forum/#!forum/mozzi-users

 More info - chihauccisoilconte.eu
 
 Nicolò Merendnino (aka "Chi ha ucciso Il Conte?") 2019, CC by-nc-sa
 */

//#include <ADC.h>  // Teensy 3.1 uncomment this line and install http://github.com/pedvide/ADC
#include <MozziGuts.h>
#include <Oscil.h> // oscillator template
#include <tables/sin2048_int8.h> // sine table for oscillator
#include <RollingAverage.h>
#include <ControlDelay.h>
unsigned int echo_cells_1 = 32;
unsigned int echo_cells_2 = 60;
unsigned int echo_cells_3 = 127;

#define CONTROL_RATE 64
ControlDelay <128, int> kDelay; // buffer length. max 512 @ 31 sec. must be factor of 8.

const byte TRIGGER_PIN = 2;
const byte ECHO_PIN = 4;
const byte LED_PIN = 10;   // set the LED that will dim with pitch
const char INPUT_PIN = 0; // set the input for the knob to analog pin 0
byte volume;

int distance, freq, calc, light;
int maximumRange = 500; // Maximum range needed
int minimumRange = 0; // Minimum range needed
int minFreq = 0; // Minimum sound frequency
int maxFreq = 2489; // Maximum sound frequency

// use: Oscil <table_size, update_rate> oscilName (wavetable), look in .h file of table #included above
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> aSin0(SIN2048_DATA);
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> aSin1(SIN2048_DATA);
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> aSin2(SIN2048_DATA);
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> aSin3(SIN2048_DATA);
// use: RollingAverage <number_type, how_many_to_average> myThing
RollingAverage <int, 16> kAverage; // how_many_to_average has to be power of 2
int averaged;

unsigned long  start_micros, end_micros;
byte phase;

enum PHASES {
  LOW_PULSE,HIGH_PULSE,HIGH_PULSE_END,GET_DISTANCE_START,GET_DISTANCE_END,DONE};

byte count; // to check if echo pulse happens

void setup(){
  kDelay.set(echo_cells_1);
  pingSetup();
  Serial.begin(115200);
  startMozzi();
}


void updateControl(){
  int sensor_value = mozziAnalogRead(INPUT_PIN); // value is 0-1023
  volume = map(sensor_value, 0, 1023, 0, 255); 

  if (volume < 30) {
volume = 0;
  
  }

  
  
  //Start Mozzi code
  
  pingControl();
  averaged = kAverage.next(freq);
  aSin0.setFreq(averaged);
  aSin1.setFreq(kDelay.next(averaged));
  aSin2.setFreq(kDelay.read(echo_cells_2));
  aSin3.setFreq(kDelay.read(echo_cells_3));
}


int updateAudio(){
  pingEcho();
    return (((3*((int)aSin0.next() +aSin1.next() +aSin2.next() >>1)
    +(aSin3.next()  >>2)) >>3)* volume)>>8; // End of Variable Mozzi code
}


void loop(){
  audioHook();
}
//End of total Mozzi code
//Begin Ultrasonic Sensor Code

void pingSetup() {
  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT); // Use LED indicator (if required)
  phase = LOW_PULSE;
}


void pingControl() {
  // The PING))) is triggered by a HIGH pulse of 2 or more microseconds.
  // Give a short LOW pulse beforehand to ensure a clean HIGH pulse:
  switch(phase) {

  case LOW_PULSE:
    digitalWrite(TRIGGER_PIN, LOW);
    phase = HIGH_PULSE;
    break;

  case HIGH_PULSE:
    digitalWrite(TRIGGER_PIN, HIGH);    
    phase = HIGH_PULSE_END;
    break;

  case HIGH_PULSE_END:
    digitalWrite(TRIGGER_PIN, LOW);
    count = 0;
    phase = GET_DISTANCE_START;
    break;

  case DONE:
    //Calculate the distance (in cm) based on the speed of sound.
    distance = (end_micros - start_micros)/5.82;
    calc = map(distance, minimumRange, maximumRange, minFreq, maxFreq);
    light = map(distance, minimumRange, maximumRange, 0, 255);
    if (distance >= maximumRange || distance <= minimumRange){
      // Send a negative number to computer and Turn LED ON to indicate "out of range"
      Serial.println("Out of Range");
      freq = 0;
      analogWrite(LED_PIN, 0); 
    }
    else {
      /* Send the distance to the computer uSing Serial protocol, and
       turn LED OFF to indicate successful reading. */
      Serial.println(distance);
      freq = maxFreq-calc;
      analogWrite(LED_PIN, 255-light);
    }
    phase = LOW_PULSE;
    break;
  }
}


void pingEcho() {
  if(phase==GET_DISTANCE_START) 
  {

    if(count++>AUDIO_RATE/10) phase=DONE; // no pulse

    if(digitalRead(ECHO_PIN)==HIGH) 
    {
      start_micros = mozziMicros();
      phase=GET_DISTANCE_END;
    }
  }
  else
  {
    if(phase==GET_DISTANCE_END) 
    {
      if(digitalRead(ECHO_PIN)==LOW) 
      {
        end_micros = mozziMicros();
        phase=DONE;
      }
    }
  }
}
