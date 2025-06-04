#include <WiFiS3.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <FastLED.h>
#include "arduino_secrets.h" // Contains your Wi-Fi credentials

#define LED_PIN       6
#define MATRIX_WIDTH  32  // Number of columns
#define MATRIX_HEIGHT 8   // Number of rows
#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)
#define COLOR_ORDER   GRB
#define BRIGHTNESS    100

CRGB leds[NUM_LEDS];

// Wi-Fi credentials (stored in arduino_secrets.h)
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

// Timezone offset for PST (UTC-8) and PDT (UTC-7)
const long utcOffsetPST = -8 * 3600;
const long utcOffsetPDT = -7 * 3600;

// NTP client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetPST, 60000);  // Default to PST, update every minute

// Zigzag XY mapping function for the 8x32 matrix
uint16_t XY(uint8_t x, uint8_t y) {
  if (x >= MATRIX_WIDTH || y >= MATRIX_HEIGHT) {
    return NUM_LEDS; // Invalid index
  }
  // Even columns light top to bottom, odd columns light bottom to top
  if (x % 2 == 0) {
    return (x * MATRIX_HEIGHT) + y;
  } else {
    return (x * MATRIX_HEIGHT) + (MATRIX_HEIGHT - 1 - y);
  }
}

// 5x7 pixel digit bitmaps for time display
const uint8_t PROGMEM digitBitmaps[10][5] = {
  {0x1F, 0x11, 0x11, 0x11, 0x1F}, // 0
  {0x00, 0x00, 0x00, 0x00, 0x1F}, // 1
  {0x1D, 0x15, 0x15, 0x15, 0x17}, // 2
  {0x11, 0x15, 0x15, 0x15, 0x1F}, // 3
  {0x07, 0x04, 0x04, 0x04, 0x1F}, // 4
  {0x17, 0x15, 0x15, 0x15, 0x1D}, // 5
  {0x1F, 0x15, 0x15, 0x15, 0x1D}, // 6
  {0x01, 0x01, 0x01, 0x01, 0x1F}, // 7
  {0x1F, 0x15, 0x15, 0x15, 0x1F}, // 8
  {0x17, 0x15, 0x15, 0x15, 0x1F}  // 9
};

// Function to convert epoch time to components (year, month, day, etc.)
void breakTime(unsigned long epochTime, int &year, int &month, int &day, int &hour, int &minute, int &second, int &weekday) {
  epochTime += 946684800; // Adjust epoch time to start from year 2000 (Y2K)
  
  second = epochTime % 60;
  epochTime /= 60;
  minute = epochTime % 60;
  epochTime /= 60;
  hour = epochTime % 24;
  epochTime /= 24;
  
  weekday = (epochTime + 6) % 7; // Sunday is day 0

  // Days since the year 2000
  int days = epochTime;

  // Year calculation
  year = 2000;
  while (true) {
    int daysInYear = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 366 : 365;
    if (days >= daysInYear) {
      days -= daysInYear;
      year++;
    } else {
      break;
    }
  }

  // Month calculation
  static const int daysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  month = 0;
  while (true) {
    int daysInThisMonth = daysInMonth[month];
    if (month == 1 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))) {
      daysInThisMonth++; // February has 29 days in a leap year
    }
    if (days >= daysInThisMonth) {
      days -= daysInThisMonth;
      month++;
    } else {
      break;
    }
  }
  day = days + 1;
}

// Function to check if Daylight Saving Time (DST) is active
bool isDSTActive(unsigned long epochTime) {
  int year, month, day, hour, minute, second, weekday;
  breakTime(epochTime, year, month, day, hour, minute, second, weekday);

  if (month < 3 || month > 11) return false;  // Before March or after November: no DST
  if (month > 3 && month < 11) return true;   // Between April and October: DST is active

  // Handle March (start of DST) and November (end of DST)
  if (month == 3) {  // DST starts at 2 AM on the second Sunday of March
    int secondSunday = 8 + (7 - weekday) % 7;  // Calculate actual date
    return (day >= secondSunday);
  } else if (month == 11) {  // DST ends at 2 AM on the first Sunday of November
    int firstSunday = 1 + (7 - weekday) % 7;   // Calculate actual date
    return (day < firstSunday || (day == firstSunday && hour < 2));
  }
  return false;
}

// Function to display a single digit on the matrix (5 pixels wide)
void displayDigit(int digit, int xOffset, int yOffset, CRGB color) {
  for (uint8_t x = 0; x < 5; x++) {
    uint8_t col = pgm_read_byte(&(digitBitmaps[digit][x]));
    for (uint8_t y = 0; y < 7; y++) {
      if (col & (1 << y)) {
        leds[XY(x + xOffset, y + yOffset)] = color;
      }
    }
  }
}

// Function to display the colon, which blinks every second
void displayColon(int xOffset, int yOffset, bool showColon, CRGB color) {
  if (showColon) {
    leds[XY(xOffset, yOffset + 2)] = color;  // Upper dot of colon
    leds[XY(xOffset, yOffset + 4)] = color;  // Lower dot of colon
  }
}

// Function to display the time in HH:MM format (12-hour format)
void displayTime(int hour, int minute, bool showColon, CRGB color) {
  // Convert 24-hour format to 12-hour format
  int displayHour = hour;
  if (hour == 0) {
    displayHour = 12;  // Midnight case
  } else if (hour > 12) {
    displayHour = hour - 12;  // Convert to 12-hour format
  }

  // Extract hour and minute digits
  int hourTens = displayHour / 10;
  int hourUnits = displayHour % 10;
  int minuteTens = minute / 10;
  int minuteUnits = minute % 10; 
  // Display the digits
  if (hourTens > 0) {
    displayDigit(hourTens, 0, 0, CHSV(random(0, 255), 255, 255));    // Hour tens
  }
  displayDigit(hourUnits, 6, 0, CHSV(random(0, 255), 255, 255));   // Hour units
  displayColon(12, 0, showColon, color);  // Colon, blinking
  displayDigit(minuteTens, 14, 0, CHSV(random(0, 255), 255, 255)); // Minute tens
  displayDigit(minuteUnits, 20, 0, CHSV(random(0, 255), 255, 255)); // Minute units
}

void setup() {
  // Initialize FastLED
  FastLED.addLeds<WS2812B, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);

  // Start serial for debugging
  Serial.begin(9600);

  // Connect to Wi-Fi
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");

  // Initialize the NTP client
  timeClient.begin();
  randomSeed(analogRead(0));
}

void loop() {
  // Update the NTP client
  timeClient.update();

  // Get the current epoch time
  unsigned long epochTime = timeClient.getEpochTime();

  // Adjust for Daylight Saving Time (DST) if necessary
  if (isDSTActive(epochTime)) {
    epochTime += 3600;  // Add one hour for DST
  }

  // Break the time into components
  int year, month, day, hour, minute, second, weekday;
  breakTime(epochTime, year, month, day, hour, minute, second, weekday);

  // Create a smooth color transition every second using HSV
  CRGB color = CHSV((second * 4) % 255, 255, 255);  // Hue changes every second

  // Clear the matrix
  FastLED.clear();

  // Display the time
  displayTime(hour, minute, second % 2 == 0, color);

   CRGB color1 = CHSV(random(0, 29) , 255, 255);
   leds[184] = color1; 
   leds[182] = color1; 
   leds[181] = color1; 
   leds[197] = color1;
   leds[198] = color1; 
   leds[185] = color1; 

  // Show the updated matrix
  FastLED.show();

  // Wait for one second
  delay(1000);
}
