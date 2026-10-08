// todo - Given the speed we should be pulsing one of our pings at a specific rate
// Pulses: 20,000 p/nm* (5.6 Hz per knot)—*p/nm = pulses per nautical mile
// Up to 45 miles per hour
#include <SoftwareSerial.h>
#include <TinyGPS++.h>

// Create a GPS object
TinyGPSPlus gps;

// Define RX and TX pins for SoftwareSerial
SoftwareSerial ss(4, 3);

void setup() {
  Serial.begin(9600);  // Start the Serial Monitor
  ss.begin(9600);      // Start communication with the GPS module
  Serial.println(F("GPS NEO-6M Test"));
}

void loop() {
  while (ss.available() > 0) {
    gps.encode(ss.read());  // Feed data to the GPS object
    if (gps.location.isUpdated()) {
      gps.location.lat();
      gps.location.lng();
      Serial.print(F("Speed: "));
      Serial.println(gps.speed.mph());
    }
  }
}

// void loop() {
//   while (ss.available() > 0) {
//     gps.encode(ss.read());  // Feed data to the GPS object
//     if (gps.location.isUpdated()) {
//       // Print latitude, longitude, and other data
//       // Serial.print(F("Latitude: "));
//       // Serial.println(gps.location.lat(), 6);  // 6 decimal places
//       // Serial.print(F("Longitude: "));
//       // Serial.println(gps.location.lng(), 6);
//       gps.location.lat();
//       gps.location.lng();
//       // Serial.print(F("Satellites: "));
//       // Serial.println(gps.satellites.value());
//       // Serial.print(F("Altitude: "));
//       // Serial.println(gps.altitude.meters());
//       Serial.print(F("Speed: "));
//       Serial.println(gps.speed.mph());
//       // Serial.print(F("Date: "));
//       // Serial.print(gps.date.day());
//       // Serial.print(F("/"));
//       // Serial.print(gps.date.month());
//       // Serial.print(F("/"));
//       // Serial.println(gps.date.year());
//       // Serial.print(F("Time: "));
//       // Serial.print(gps.time.hour());
//       // Serial.print(F(":"));
//       // Serial.print(gps.time.minute());
//       // Serial.print(F(":"));
//       // Serial.println(gps.time.second());
//       // delay(1000);
//     }
//   }
// }
