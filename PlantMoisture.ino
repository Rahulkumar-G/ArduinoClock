#define BLYNK_TEMPLATE_ID "BLYNK_TEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME "BLYNK_TEMPLATE_NAME"
#define BLYNK_AUTH_TOKEN "BLYNK_AUTH_TOKEN"

// Include necessary libraries

#include <BlynkSimpleEsp32.h>
#include <WiFi.h>
#include <esp_sleep.h>



// Blynk authentication token
char auth[] = BLYNK_AUTH_TOKEN;

// WiFi credentials
char ssid[] = "ssid";
char pass[] = "pass";

// Defining the sensor pin
const int sensorPin = 34;  // Changed pin to GPIO 34 for testing


// Declaring a global variable for sensor data
int sensorVal;

// Timer for Blynk
BlynkTimer timer;

// Time to sleep (in microseconds)
#define uS_TO_S_FACTOR 1000000  // Conversion factor for microseconds to seconds
#define TIME_TO_SLEEP  60       // Time ESP32 will go to sleep (in seconds)

// Function to send sensor data to Blynk
void myTimer() {
  // Add a short delay to stabilize the reading
  delay(10);

  // Read the sensor value
  sensorVal = analogRead(sensorPin);

  // Optionally, map the sensor value to a moisture percentage
  int moisturePercent = map(sensorVal, 4095, 0, 100, 0);  // Reverse the mapping order

  // Check if sensor value is valid, otherwise handle error
  if (sensorVal == 0) {
    Serial.println("Warning: Sensor value is 0. Please check sensor connection.");
  }

  // Writing sensor value to Blynk datastream V25
  Blynk.virtualWrite(V25, sensorVal);
  delay(10);  // Short delay to ensure stable write
  Blynk.virtualWrite(V26, moisturePercent);

  // Print sensor readings
  Serial.print("Sending data to Blynk - Soil Moisture Value: ");
  Serial.print(sensorVal);
  Serial.print(", Soil Moisture Percentage: ");
  Serial.print(moisturePercent);
  Serial.println("%");

  // Enter deep sleep to reduce power consumption
  Serial.println("Entering deep sleep...");
  delay(100);  // Give time for the serial print before sleeping
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);  // Initialize serial communication at 115200 baud rate
  delay(1000);           // Give time for the serial monitor to initialize

  // Connect to WiFi
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");

  // Connect to Blynk Cloud
  Blynk.config(auth);
  Blynk.connect();

  // Wait for Blynk to connect
  while (Blynk.connected() == false) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to Blynk");

  // Set timer to send data to Blynk Cloud every 2000ms (2 seconds)
  timer.setInterval(2000L, myTimer);
}

void loop() {
  // Runs all Blynk functions
  Blynk.run();

  // Runs Blynk timer to send data at set intervals
  timer.run();
}
