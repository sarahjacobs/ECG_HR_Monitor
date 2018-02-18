// include all the libraries!
#include <Wire.h>
#include <math.h>
#include <Adafruit_RGBLCDShield.h>
#include <utility/Adafruit_MCP23017.h>
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();
#define WHITE 0x7

// declare pins
int digHR = 8; // digital input pin
int analogHR = A2; // analog input pin
int rangePin = 4; // range LED output
int blinkPin = 6; // blink LED output
int resetPin = 2; // reset output

//assign integer values
int digMin;
int maxPeak;
int track;
int anlgMin;

// assign floats (for math functions)
float digAvg;
float anlgAvg;

// declare boolean variables
boolean beat; 
boolean change;
boolean pause;
boolean diff;
boolean aBeat;
boolean aChange;

//assign timers (longs)
unsigned long tstart;
unsigned long timer;
unsigned long timer2;
unsigned long timer3;
unsigned long atStart;


// assign time values (longs)
long t1end;
long t2end;
long tdiff;
long tdiffOld;
long at1end;
long at2end;
long atdiff;

// assign time vector to collect minute averages
long times[201];  // arduino is c based which autoinitializes undeclared values to 0
long atimes[201];


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600); // allow serial monitor time to initialize
  lcd.clear(); //clear LCD

  // assign input or output for pins
  pinMode(digHR, INPUT);
  pinMode(analogHR, INPUT);
  pinMode(resetPin, OUTPUT);
  pinMode(blinkPin, OUTPUT);
  pinMode(rangePin, OUTPUT);

  // initialize booleans
  beat = false;
  aBeat = false;
  pause = true;
  diff = false;

  // run analog calibration code 
  analogCal(); 

  // put in interrupts for digital signal 
  attachInterrupt(digitalPinToInterrupt(digHR), instantD, RISING);

  //LCD screen code for calibrating screen
  lcd.begin(16, 2); 
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Calibrating");
  lcd.setCursor(0, 1);
  delay(1000);
  lcd.clear();
}

void loop() {
  uint8_t buttons = lcd.readButtons(); //initialize buttons

  // pause function! *when pause is true - it's not paused*
  if (pause) {
    if (beat == true) {
      t1end = (long) tstart; //sets start of heartbeat 
    }
    if (beat == false) {
      t2end = (long) tstart; //sets end of heartbeat 
    }
    tdiff = (long)t2end - t1end; // finds duration of 1 beat
    tdiff = abs(tdiff);
    timeUpdate(tdiff); // tdiff put into time vector
    digAvg = average(times); // This should take the instantaneous digital HR 
    digMin = minAvg(times); // Finds minute average of digital
    instantA(); // runs instantaneous analog code
    
    if (millis() >= timer3 + 3000) { //every 3 seconds update screen
      timer3 = millis();
      Print(); // go to print function! ~so modular~
    }
  }
  if (pause == false) { //if it is paused
    lcd.setCursor(0, 0);
    lcd.print("Dig:");
    lcd.setCursor(5, 0);
    lcd.print(digMin); //display dig minute
    lcd.setCursor(9, 0);
    lcd.print("Ana:");
    lcd.setCursor(13, 0);
    lcd.print(anlgMin); //display ana minute
    lcd.setCursor(0, 1);
    lcd.print("Paused");
  }
  if (buttons & BUTTON_UP) { //if up button pushed
    delay(500); // keeps one push of button from toggling multiple times
    pause = !pause; // flip pause boolean (pause and unpause)
    lcd.clear();
  }
  if (buttons & BUTTON_DOWN) { // this is the reset code
    detachInterrupt(digitalPinToInterrupt(digHR)); // detach interrupt
    for (int i = 0; i < 201; i++) { //resets all values to 0
      times[i] = 0;
      atimes[i] = 0;
    }
    digitalWrite(resetPin, HIGH); //outputs voltage to MOSFET
    delay(1000);
    digitalWrite(resetPin, LOW); // stops outputting voltage to MOSFET
    digitalWrite(blinkPin, LOW); // don't blink
    digitalWrite(rangePin, LOW); // reset range pin
    beat = false;// reset to initialized variables  
    aBeat = false;
    pause = true;
    attachInterrupt(digitalPinToInterrupt(digHR), instantD, RISING); // reset interrups
    lcd.begin(16, 2); //initialize LCD 
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Calibrating");
    lcd.setCursor(0, 1);
    analogCal(); // calibrate the analog signal
    delay(1000);
    lcd.clear();
  }

  if (anlgAvg > 40.0 && anlgAvg < 180.0) { //if in range of 40-180, pin should be high
    digitalWrite(rangePin, HIGH);
  }

  if (anlgAvg > 180.0 || anlgAvg < 40.0) {
    digitalWrite(rangePin, LOW);
  }

  if (millis() >= timer2 + 250) { //every 250 milliseconds, check parameters for blinking
    timer2 = millis();
    Blink();
  }
}

void instantD() { //interrupt function -> flips boolean, starts timer
  beat = !beat;
  tstart = millis();
  change = true;
}

void timeUpdate(long tdiff) { //updates times array
  if (change == true) {
    for (int i = 0; i < 201; i++) {
      times[i] = times[i + 1]; // this should shift the whole array to the left by 1 for each time update
    }
    if (tdiff > 2000 || tdiff < 250) {
      times[200] = times[199];
    }
    else {
      times[200] = tdiff; // add in the newest value
    }
    change = false;
  }
}

float average(long times[]) { //average times 
  float sum = 0; //clear the sum at the beginning of each call to the function
  long ans;
  for (long i = 0; i < 5; i++) {
    ans = times[i + 196];
    sum = (60000 / (float)ans) * exp(((float)i - 4) / 2) + sum; // exponential average of the 5 most recent values
  }
  sum = sum / 2.333;
  if (sum != INFINITY && sum > 250) { // if sum is within bounds, sum is equal to digital avg
    sum = digAvg;
  }
  if (anlgAvg > sum && anlgAvg != INFINITY) { 
    sum = digAvg;
//If analog average is greater than calculated average, return previous digital average to avoid too low reads from missed interrupts
  }
  return sum;
}

int minAvg(long times[]) { //find minute average
  long sum = 0; // clear sum every time
  int count = 0;
  for (long i = 200; i >= 0; i--) {
    sum = times[i] + sum;
    if (sum < 60000) { //if sum of times is less than one minute, keep counting beats
      count++;
    }
  }
  return count; // return number of beats
}

void analogCal() { // analog calibration! 
  while (timer < 5000) {
    timer = millis(); //set timer
    track = analogRead(analogHR); //analog read vals

    if (track > maxPeak) { //find max val for analog signal
      maxPeak = track;
    }
  }
}

void instantA() { //instantaneous analog values
  track = analogRead(analogHR); //reads signal
  int threshold = ceil(0.8 * maxPeak); //sets threshold
  if (track >= threshold) {
    aBeat = !aBeat; // flip a boolean
    atStart = millis(); //set a time
    aChange = true; 
    if (aBeat == true) {
      at1end = (long) atStart; //sets start of beat 1
    }
    if (aBeat == false) {
      at2end = (long) atStart; //sets start of beat 2
    }
    atdiff = at2end - at1end; //finds time between beats (length of 1 beat)
    atdiff = abs(atdiff);
    analogUpdate(atdiff); //adds it to array
    anlgAvg = anlgAverage(atimes); //find instantaneous
    anlgMin = aminAvg(atimes); // find minute
    delay(57);// delays long enough that 1 heartbeat doesn't read as multiple heartbeats 
  }
}

void analogUpdate(long atdiff) { //adds time of heart beat to array

  if (aChange == true) { //if a beat has occurred
    for (int i = 0; i < 201; i++) {
      atimes[i] = atimes[i + 1]; // this should shift the whole array to the left by 1 for each time update
    }
    atimes[200] = atdiff; // this should add in the newest value
    aChange = false; //ssets aChange back to false (will flip when next beat occurs)
  }
}

float anlgAverage(long atimes[]) { //find instantaneous analog HR
  float sum = 0; //clear the sum at the beginning of each call to the function
  long ans;
  for (long i = 0; i < 5; i++) {
    ans = atimes[i + 196];
    sum = (60000 / (float)ans) * exp(((float)i - 4) / 2) + sum; /// exponential average of the 5 most recent values
  }
  sum = sum / 2.333; //divides by exponential 'weights'

  return sum;
}

int aminAvg(long atimes[]) { //find minute average analog HR
  long sum = 0; // clear sum every time
  int count = 0;
  for (long i = 200; i >= 0; i--) { 
    sum = atimes[i] + sum;
    if (sum < 60000) { //if sum of times for hearbeat is less than a minute, add another heartbeat
      count++;
    }
  }
  return count;
}

void Print() { //print all the values!
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("AI:");
  lcd.setCursor(4, 0);
  lcd.print(anlgAvg);
  lcd.setCursor(8, 0);
  lcd.print("DI:");
  lcd.setCursor(11, 0);
  lcd.print(digAvg);
  lcd.setCursor(0, 1);
  lcd.print("AM:");
  lcd.setCursor(4, 1);
  lcd.print(anlgMin);
  lcd.setCursor(8, 1);
  lcd.print("DM:");
  lcd.setCursor(11, 1);
  lcd.print(digMin);
}

void Blink() {
  float test1;
  float test2;
  //check if > 10% diff between dig and ana
  test1 = abs((digAvg - anlgAvg) / digAvg * 100); 
  test2 = abs((digAvg - anlgAvg) / anlgAvg * 100);
  if (test1 > 10 || test2 > 10) { //if there is a >10%
    diff = !diff; 
    // flip boolean -> running this function every 250 ms will lead to blinking as it alternates high and low
    if (diff == true) {
      digitalWrite(blinkPin, HIGH);
    }
    if (diff == false) {
      digitalWrite(blinkPin, LOW);
    }
  }
  else { // <10% difference
    digitalWrite(blinkPin, LOW); //pin should not blink
  }

}

