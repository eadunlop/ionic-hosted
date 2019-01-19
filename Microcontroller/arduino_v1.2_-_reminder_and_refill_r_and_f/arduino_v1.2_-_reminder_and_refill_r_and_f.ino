// #include "FastLED.h"
#include <Adafruit_NeoPixel.h>

//pin number of data pin was 5
#define DATA_PIN 31
//pin number for the array of sensors. all sensors in the array
//use the same pin was 8
#define SENSORS 15
//pin number for the clock was 9
#define CLOCK 7
//pin number to communicate with the IR emitter for the current sensor was 10
#define SENSOR_EMITTER 11
//pin number to communicate with the IR receiver for the current sensor
#define SENSOR_RECEIVER A0
//number of shelfRokr units
#define NUM_SHELVES 1
//number of facings on a single shelfRokr
#define NUM_SLOTS 4
//delay time to allow the emitter to warm up to full strength. time is in MS
#define EMITTER_TIMER 10000
//initial value for thresholds. a low starting value will not effect the flow
//of logic and will balance out after the first few iterations
#define THRESHOLD_DEFAULT 256
//percent different a sensor value has to be to be considered picked up
#define PICKUP_RATIO .65
//delay time in ms between sensor poll loops
#define LOOP_DELAY 10

//boolean values to maintain picked up state of slots. false
//indicates that the product is not picked up
bool pickedUp[NUM_SLOTS];
//to store threshold values for each sensor
int thresholds[NUM_SLOTS];


//number of LEDS on the strip was 25

#define NUM_LEDS 12
//reference to the LED array for FASTLed
// CRGB leds[NUM_LEDS];
byte leds[NUM_LEDS];

//start led index for its corresponding product slot
const int LEDStart[] = {9, 6, 3, 0};
//ending led index for its corresponding product slot
const int LEDEnd[] = {12, 9, 6, 3};

//setup regimen
// number of phases
int nregPhases = 2; 
int nregItems[] = {3, 3}; 
int regPhase = 0;
int regItem = 0;
bool regActive = false;
 
// wait after last regimen
long regWait=1000;
long regStart= 500;
int regList[2][3] = { { 1, 0 , 2}, { 1, 0, 3 } };


//reminder time for product slots: old
//long reminderTime[] = {21000, 15000, 27000, 68000};
//long reminderInterval[] = {40000, 40000, 80000, 80000};
//reorder setting for product slots
const int reorderAfter[] = {10,10,2,10};
int pickupCount[] = {0,0,0,0};
//bool values to keep track of which slots have blinking LEDs
bool slotsBlinking[NUM_SLOTS];
//keeps track of the brightness value for updateLighting()
int brightness = 0;
//increment value for brightness to fade in and out
int brightnessIncrement = 5;
//global that keeps track if a chasing effect is playing
bool chasingEffect = false;
//the lead LED in the chase effect must be between 0 and NUM_LEDS
//should always start at 0
int chaseLeader = 0;
//the direction the chase is traveling. should always start as true
//or will interfere with chaseEffect logic
bool chaseRight = true;
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUM_LEDS, DATA_PIN, NEO_GRB + NEO_KHZ800);

// defines for setting and clearing register bits
#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

void setup() 
{
  //delay(10000);
  //initialize boolean values to to start as false because all
  //slots should have something in it
  initializeThresholds(); 
  initializeSlots();
  
  LEDSetup();
  
  //sets up special function registers for the arduino. not sure of particulars,
  //but this is standard for all arduino programs.
  // set prescale to 16
  // sbi(ADCSRA, ADPS2) ;
  // cbi(ADCSRA, ADPS1) ;
  // cbi(ADCSRA, ADPS0) ;

  //set up communication modes for pins
  pinMode(SENSORS, OUTPUT);
  pinMode(CLOCK, OUTPUT);
  pinMode(SENSOR_EMITTER, OUTPUT);
  Serial.begin(57600);

}

//arduino main loop
void loop() 
{
  //check the sensors to see if anything has been picked up or put down
  pollProductSensors();
  
  //receive any commands from the main program
  //readInput();
}

//reads through all of the product sensors to see which have been picked up
void pollProductSensors()
{
  
  //send signal to activate the first sensor
  digitalWrite(SENSORS, HIGH);

  //iterate through each shelf and sensor
  //Serial.print(millis());
  //Serial.print(",");
  for(int i = 0; i < NUM_SHELVES; i++){
    for(int j = 0; j < NUM_SLOTS; j++){
      
      //advances clock and iterates current sensor to the
      //next sensor in the array
      digitalWrite(CLOCK, HIGH);
      digitalWrite(CLOCK, LOW);
      //switch off current sensor so the previous sensor is the only one active
      digitalWrite(SENSORS, LOW);
      
      //actually check to see if the status of each slot has changed
      sensorWork(j);

      //if (millis()>reminderTime[j]) 
      //  activateSlot(j);

      if (millis()>regStart && !regActive) 
      {
         regActive=true;
         activateSlot(regList[regPhase][regItem]);
      }

      if(!chasingEffect){
        updateLighting();
      }
    }

    //delay to allow time between polling all of the sensors. accounts for slower pickups
    delay(LOOP_DELAY);

    updateLighting();
  }
  //Serial.println(",x1");
}

//checks if the product is being picked up
boolean sensorWork(int slot)
{
  //values of SENSOR_RECEIVER when emitter is on and off
  int sensorOn = 0;
  int sensorOff = 0;

  //activate emitter and take a reading then deactivate it and
  //take another reading
  //Serial.print(slot);
  //Serial.print(",");
  digitalWrite(SENSOR_EMITTER, HIGH);
  delayMicroseconds(EMITTER_TIMER);
  sensorOn = analogRead(SENSOR_RECEIVER);
  //Serial.print(sensorOn);
  //Serial.print(",");
  digitalWrite(SENSOR_EMITTER, LOW);
  delayMicroseconds(EMITTER_TIMER);
  sensorOff = analogRead(SENSOR_RECEIVER);
  //Serial.print(sensorOff);
  //Serial.print(",");
  int diff = sensorOn - sensorOff;
  //Serial.print(diff);
  //Serial.println(",x1");
  

  //if (millis()<2000) thresholds[slot] = diff;// updateThresholds(diff, slot); 
  if (millis()>1000) {
  if(pickedUp[slot] == false && diff < thresholds[slot] * PICKUP_RATIO)sendPickUp(slot, diff);
  else if(pickedUp[slot] == true && diff > thresholds[slot] * PICKUP_RATIO) sendPutDown(slot);
   }

if (millis()>500) {
  //only want to update the threshold if the product has been sitting in place
  if(pickedUp[slot] == false) updateThresholds(diff, slot);
}
}

//updates the threshold value to it reflects a more accurate mid-range value
void updateThresholds(int diff, int slot)
{
  //thresholds[slot] = (thresholds[slot] + sensorOn) / 2;
  thresholds[slot] -= (thresholds[slot] / 20);
  thresholds[slot] +=  (diff / 20);
  //Serial.print('T');
}

//sends message to host machine to indidcate that there was a pickup
void sendPickUp(int slot, int diff)
{
  pickedUp[slot] = true;
  Serial.print(slot);
  Serial.print('U');
  //Serial.print(',');
  //Serial.print(thresholds[slot]);
  //Serial.print(',');
  //Serial.print(diff);
  Serial.print('\n');
}

//sends message to host machine to indicate that there was a putdown
void sendPutDown(int slot)
{
  pickedUp[slot] = false;
  Serial.print(slot);
  Serial.print('D');
  Serial.print('\n');
  deactivateSlot(slot);
  regItem++;
  if (regItem >= nregItems[regPhase])
     {
      regActive = false;
      regItem=0;
      regStart= millis()+regWait;
      
      regPhase++;
      if (regPhase >= nregPhases)
         regPhase = 0;
     }
     else activateSlot(regList[regPhase][regItem]); 
  //reminderTime[slot]=millis()+reminderInterval[slot];
  pickupCount[slot]++;
    if (pickupCount[slot] > reorderAfter[slot])
  {
    pickupCount[slot]=0;
    fastFlash(slot);
  }
}

//reads input from the host machine and parses it to detemine how to call the
//triggerLighting function
void readInput()
{
  if(Serial.available() > 0){
    String command = Serial.readStringUntil('\n');
    int slot = -1;

    //using if block with function calls to allow for
    //extensibility in the future for other commandsS
    if(command.startsWith("LED")){
      parseCommandLED(command);
    }else{
      //not a valid command. can be altered later for additional commands
    }
  }
}

void parseCommandLED(String command)
{
  //char ledCommand[] = command;
  //command.toCharArray(ledCommand, command.length());
  
  char action = command.charAt(4);
  int slot = (int)(command.charAt(6)) - 48;
  if(action == 'U'){
    stopChaseEffect();
    activateSlot(slot);
  }else if(action == 'D'){
    stopChaseEffect();
    deactivateSlot(slot);
  }else if(action == 'C'){
    beginChaseEffect(0);
  }else if(action == 'O'){
    stopChaseEffect();
  }else{
    //do nothing. clear buffer. bad command
  }
}

void updateLighting()
{
  if(chasingEffect){    //code for simulating the chase effect
    if(chaseRight == true){
      // leds[chaseLeader - 1] = CRGB::Black;
      leds[chaseLeader - 1] = 1;
      if(chaseLeader >= NUM_LEDS - 2){
        chaseRight = false;
      }else{
        chaseLeader++;
      }
    }else if(chaseRight == false){
      // leds[chaseLeader + 1] = CRGB::Black;
      leds[chaseLeader + 1] = 0;
      if(chaseLeader <= 1){
        chaseRight = true;
      }else{
        chaseLeader--;
      }
    }else{
      //should not get here. it should be either one of the above
    }

    // leds[chaseLeader - 1] = CRGB::White;
    leds[chaseLeader - 1] = 1;
    // leds[chaseLeader] = CRGB::White;
    leds[chaseLeader] = 1;
    // leds[chaseLeader + 1] = CRGB::White;
    leds[chaseLeader + 1] = 1;

    // FastLED.show();
    strip_update();
  
  }else if(!chasingEffect){    //iterate through all the slots and do light effect for items that are picked up
    brightness += brightnessIncrement;
    // FastLED.setBrightness(brightness);
    // FastLED.show();
    strip_update();
    
    if((brightness >= 255 && brightnessIncrement > 0) || (brightness <= 0 && brightnessIncrement < 0)){
      brightnessIncrement *= -1;
    }
  }
}

void activateSlot(int slot)
{
  for(int i = LEDStart[slot]; i < LEDEnd[slot]; i++){
  //  leds[i] = CRGB::White;

  if (reorderAfter[slot] > pickupCount[slot]) 
    leds[i] = 7;
    else
    leds[i]=1;
  //Serial.println(leds[i]);
  }
}

void deactivateSlot(int slot)
{
  for(int i = LEDStart[slot]; i < LEDEnd[slot]; i++){
    // leds[i] = CRGB::Black;
    leds[i] = 0;
  }
}

//starts the chasing effect at the designated slot
void beginChaseEffect(int slot)
{
  chaseLeader = LEDStart[slot + 1];
  // leds[chaseLeader] = CRGB::White;
  leds[chaseLeader] = 0;
  // FastLED.setBrightness(255);
  brightness=255;
  strip_update;
  chaseRight = true;
  chasingEffect = true;
}

//cancels the chasing effect
void stopChaseEffect()
{
  chasingEffect = false;
  //reset the chase
  // FastLED.setBrightness(0);
  brightness=0;
  strip_update();
  chaseLeader = 0;
  
  //code to reset the LEDs back to black
  for(int i = 0; i < NUM_LEDS; i++){
    // leds[i] = CRGB::Black;
    leds[i] = 0;
  }
  // FastLED.show();
  strip_update();
  
}

//called in the beginning of the program. iterates over the pickedUp array
//and sets all values to false, which is the default value. a value of 
//false indicates that the product is still sitting in the slot.
void initializeSlots()
{
  for(int i = 0; i < NUM_SLOTS; i++){
    pickedUp[i] = false;
  }
}

//called at the beginning of the program. sets all of the threshold values to
//a default of 512, which is the middle range of analog values that can be sent
//by the sensor

void initializeThresholds()
{
  for(int i = 0; i < NUM_SLOTS; i++){
    thresholds[i] = THRESHOLD_DEFAULT;
  }
}

//sets up the LED array to be used by the FastLED library
void LEDSetup()
{
  // FastLED.addLeds<NEOPIXEL,DATA_PIN>(leds, NUM_LEDS);
    pixels.begin();
  
  for(int i = 0; i < NUM_SLOTS; i++){
    slotsBlinking[i] = false;
  }
}

void strip_update()
{
  //Serial.println(brightness);
  for(int i = 0; i < NUM_LEDS; i++){
    if (leds[i]&7) 
      {
        pixels.setPixelColor(i, pixels.Color(((leds[i]&4)>>2)*brightness,((leds[i]&2)>>1)*brightness,(leds[i]&1)*brightness));
        //Serial.println(leds[i]);
      }
      else 
      {
        pixels.setPixelColor(i, pixels.Color(0,0,0));
        //Serial.println(leds[i]);
      }
      }
       pixels.show();
  }

void fastFlash(int slot)
{
delay(500);
for(int k = 0; k<8; k++)
{
  for(int j = 5; j<255; j+=10)
    {
      for(int i = LEDStart[slot]; i < LEDEnd[slot]; i++)
      {
        pixels.setPixelColor(i, pixels.Color(0, j, 0));
        pixels.show();
       }  
    }
    for(int j = 255; j>5; j-=10)
    {
      for(int i = LEDStart[slot]; i < LEDEnd[slot]; i++)
      {
        pixels.setPixelColor(i, pixels.Color(0, j, 0));
        pixels.show();
      }  
    }
}
}
