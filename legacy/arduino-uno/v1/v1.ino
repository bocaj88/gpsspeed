/**
 * @file MSWorking.ino
 * @author Jake Michalski
 * @brief 
 * @version 0.2
 * @date 2025-06-23
 * 
 * @copyright Copyright MIT License (c) 2025
 * @comments 
 * - Here's the GPS Module I got MakerFocus GT-U7 GPS Module Satellite Navigation Positioning GPS Receiver Drone Microcontroller Compatible with NEO-6M 51 Microcontroller STM32 Arduino UNO R3 with IPEX Antenna High Sensitivity
 *     - https://www.amazon.com/dp/B07P8YMVNT?ref_=ppx_hzsearch_conn_dt_b_fed_asin_title_2&th=1
 * - Knowing when you have GPS lock. At first the module will be solid red, then it will blink, then it will be solid green.
 * - You do need line of sight to the sky to get a fix... but once we have a lock, you can bring it inside....
 * - Problems. there is only one serial port on the Arduino Uno, so we can't use the GPS module and the serial monitor at the same time....
 * - We can do it via software serial, but the hit is so great that we can't imitate speeds > 18MPH, so on the board there are 2 connectors, one for GPS serial and one for GPS Software serial...
 * 
 * - To flip between them we have to uncomment a few things and change some code.... 
 * - We use Software serial for the GPS, and Serial for the serial monitor.
 * - Uncomment the SoftwareSerial line and comment out the Serial line.
 */

#include <SoftwareSerial.h>
#include <TinyGPS++.h>

// Globals for GPS 
TinyGPSPlus gps;  // Create a GPS object
SoftwareSerial ss(4, 3); // Define RX and TX pins for SoftwareSerial GPS

// Globals for the MOSFET
int mosfetPin = 6; // Choose a digital pin
float speedGlobal = 8.0; // Variable to hold the speed value in miles per hour (Used to be 1. At least now we know it's searching @ 8)
unsigned long nextMosfetPulse = 0;  // Will store the next time we should pulse (rather than delaying)

// const float pulsesPerNauticalMile = 20000;  // Pulses per nautical mile
// const float frequencyMile = 50.0;           // Frequency in Hz per knot - Original - off by 13.5x
// const float frequencyMile = 675.0;           // Frequency in Hz per knot - Tuned
// const float frequencyMile = 3.7;           // Frequency in Hz per knot - Tuned
// If we Command 1mph what's the frequency?
const float frequencyMile = 4.22;   

String inputString = "";  // A string to hold incoming data

void setup() {
  pinMode(mosfetPin, OUTPUT); // Set the MOSFET pin as output
  Serial.begin(9600);         // Start the serial communication
  ss.begin(9600);      // Start communication with the GPS module
  Serial.println("Jake's GPS Module");
}

void loop() {
  unsigned long currentMicros = micros();
  
  // Make sure the global - Speed is up to date.
  updateSpeedGPS();     // GPS
  // updateSpeedSerial();  // Serial
  
  // Check if it's time to update the pulse (Rather than internal delay)
  if (currentMicros >= nextMosfetPulse) {
    // if (currentMicros - nextMosfetPulse > 400) {
    //   Serial.print("missed by this much: ");
    //   Serial.println(currentMicros - nextMosfetPulse);
    // }
    // Flips the pin and recalculates the next pulse time
    modulateOutput(speedGlobal); 
  }
}

void updateSpeedSerial() {
  // Check the Serial for the Speed Directly rather than the GPS
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if (inChar == '\n') {  // Check for newline character
      speedGlobal = inputString.toFloat();  // Convert to float
      Serial.print("Speed set to: ");
      Serial.println(speedGlobal);
      inputString = "";  // Clear the string for the next input
    } else {
      inputString += inChar;  // Add the incoming character to the string
    }
  }
}

void updateSpeedGPS() {
  // Try to get the speed
  while (ss.available() > 0) {
    gps.encode(ss.read());  // Feed data to the GPS object

    // This actually isn't true that often  so we can't assume we can just call it any time, we have
    //     to call this rather frequently
    if (gps.location.isUpdated()) {
      // For some reason we have to poll the lat and lng when before getting speed
      gps.location.lat();
      gps.location.lng();
      speedGlobal = gps.speed.mph();
      Serial.print(F("Speed: "));
      Serial.println(speedGlobal);

    }
  }
}

// Helper function to modulate the output pin based on the speed in miles per hour
void modulateOutput(float speed) {
  // Set a minimum and maximum speed to not break anything 
  if (speed < 0.1) speed = 0.1;
  if (speed > 60) speed = 60;

  // Calculate the frequency based on speed in miles per hour
  float frequency = speed * frequencyMile; // Frequency in Hz

  // Calculate the delay time for 50% duty cycle in microseconds
  unsigned long delayTimeMicros = (unsigned long)(1000000.0 / (2 * frequency)); // Delay in microseconds

  // For it to be a multiple of 16
  delayTimeMicros = (delayTimeMicros / 128) * 128;

  // Ensure the delayTimeMicros is not too small or too large
  if (delayTimeMicros < 1) delayTimeMicros = 1;
  if (delayTimeMicros > 500000) delayTimeMicros = 500000;

  static bool mosfetState = LOW;
  mosfetState = !mosfetState;  // Toggle the MOSFET state every time we call
  digitalWrite(mosfetPin, mosfetState);
  nextMosfetPulse = micros() + delayTimeMicros;
  // nextMosfetPulse = micros() + 1784;
}
