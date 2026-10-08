int mosfetPin = 6; // Choose a digital pin
float speed = 1.0; // Variable to hold the speed value in miles per hour

const float pulsesPerNauticalMile = 20000;  // Pulses per nautical mile
// const float frequencyMile = 50.0;           // Frequency in Hz per knot - Original - off by 13.5x
// const float frequencyMile = 675.0;           // Frequency in Hz per knot - Tuned
// const float frequencyMile = 3.7;           // Frequency in Hz per knot - Tuned
const float frequencyMile = 4.75;

String inputString = "";  // A string to hold incoming data

void setup() {
  pinMode(mosfetPin, OUTPUT); // Set the MOSFET pin as output
  Serial.begin(9600);         // Start the serial communication
}

void loop() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if (inChar == '\n') {  // Check for newline character
      speed = inputString.toFloat();  // Convert to float
      Serial.print("Speed set to: ");
      Serial.println(speed);
      inputString = "";  // Clear the string for the next input
    } else {
      inputString += inChar;  // Add the incoming character to the string
    }
  }

  // Call the helper function to modulate the output based on speed
  modulateOutput(speed);
}

// Helper function to modulate the output pin based on the speed in knots
void modulateOutput(float speed) {
  // Calculate the frequency based on speed in knots
  float frequency = speed * frequencyMile; // Frequency in Hz

  // Calculate the delay time for 50% duty cycle
  int delayTime = (int)(1000.0 / (2 * frequency)); // Delay in milliseconds

  // Ensure the delayTime is not too small or too large
  if (delayTime < 1) delayTime = 1;
  if (delayTime > 1000) delayTime = 1000;

  digitalWrite(mosfetPin, HIGH); // Turn on the MOSFET (and the 12V load)
  delay(delayTime);              // Wait for the calculated time

  digitalWrite(mosfetPin, LOW);  // Turn off the MOSFET (and the 12V load)
  delay(delayTime);              // Wait for the same time to maintain 50% duty cycle
}
