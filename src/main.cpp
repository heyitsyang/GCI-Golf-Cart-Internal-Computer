#include <SPI.h>
#include <TFT_eSPI.h> // Include the TFT_eSPI library
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "prototypes.h"
#include "secrets.h"

#define SCREEN_TIMEOUT 45 // seconds

#define BUTTON_PIN 12     // GPIO34-39 do not have pullups
#define ONE_WIRE_PIN 13   // DS18B20 temperature sensor and any other One-Wire

//#define GC_DISP_MAC_ADDR_STR "14:33:5C:6B:E8:EC"  // MAC address of the remote device goes here


TFT_eSPI tft = TFT_eSPI(); // Create a TFT_eSPI object

OneWire oneWire(ONE_WIRE_PIN);        // Setup a oneWire instance to communicate with any OneWire device
DallasTemperature sensors(&oneWire);  // Pass oneWire reference to DallasTemperature library

unsigned long screenStartTime = 0;
bool screenOn = true;

char thisDeviceMacStr[18], gcdMacStr[18];
uint8_t gcdAddress[6];

// varibales for outbound data
int modeHeadLights;
int outdoorLuminosity;
float airTemperature;
float battVoltage;
float fuelLevel;

typedef struct struct_message_out {
  int modeLights;
  int outdoorLum;
  float airTemp;
  float battVolts;
  float fuel;
} struct_message_out;

// vawriables for inbound data
int incomingCmd;

typedef struct struct_message_in {
  int gcdCmd;

} struct_message_in;

// Create a struct_message called BME280Readings to hold sensor readings
struct_message_out dataOut;

// Create a struct_message to hold incoming sensor readings
struct_message_in dataIn;

esp_now_peer_info_t peerInfo;


/**************
 *    SETUP   *
 **************/

void setup(void) {
  Serial.begin(115200);
  sensors.begin();

  // Initialize button pin with pullup
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Initialize WiFi to enable MAC address reading
  WiFi.mode(WIFI_STA);
  
  tft.init(); // Initialize the display
  tft.setRotation(1); // Set the rotation to landscape mode (optional)

  tft.fillScreen(TFT_BLACK); // Clear the screen to black
  tft.setTextColor(TFT_WHITE, TFT_BLACK); // Set text color to white

  tft.setCursor(1, 10); // Set cursor position
  tft.setTextFont(2); // Set font size
  tft.print("MAC ");
  createMacAddressStr(thisDeviceMacStr);
  tft.setTextFont(4); // Set font size
  tft.println(thisDeviceMacStr);        // Display on TFT
  Serial.println(thisDeviceMacStr);     // Display on Serial

  parseMacAddressStr(GC_DISP_MAC_ADDR_STR, gcdAddress);    // Parse GCD MAC address string into a byte array for ESP-NOW

  screenStartTime = millis();   // Record screen start time

} /* END SETUP */


/**************
 *    LOOP    *
 **************/

void loop() {

  float tempC_0, tempF_0;
  
  sensors.requestTemperatures();    // Send the command to get temperatures
  tempC_0 = sensors.getTempCByIndex(0);
  tempF_0 = tempC_0 * 9.0 / 5.0 + 32.0;

  tft.setCursor(1, 40); // Set cursor position
  tft.setTextFont(2); // Set font size small
  tft.print("TEMP ");
  tft.setTextFont(4); // Set font size medium

  if (tempC_0 != DEVICE_DISCONNECTED_C) {
    tft.print(tempF_0);
    tft.println("°F");
    Serial.print(tempF_0);
    Serial.println("°F");
  } else {
    tft.println("No sensor");
    Serial.println("No sensor");
  }


  // Check for button press to turn screen back on
  if (!screenOn && digitalRead(BUTTON_PIN) == LOW) {
    // Turn on the backlight
    digitalWrite(TFT_BL, HIGH);  // Turn on backlight
    screenOn = true;
    screenStartTime = millis();  // Reset timeout timer
    Serial.println("Backlight turned on by button press");
    delay(200); // Debounce delay
  }

  // Check if screen timeout has been reached
  if (screenOn && (millis() - screenStartTime) >= (SCREEN_TIMEOUT * 1000)) {
    // Turn off the backlight only
    digitalWrite(TFT_BL, LOW);  // Turn off backlight
    screenOn = false;
    Serial.println("Backlight turned off after timeout");
  }

  delay(100); // Check every 100ms for responsive button

}  /* END LOOP */


/*******************
 *    FUNCTIONS    *
 *******************/

void createMacAddressStr(char* MacStr){
  uint8_t baseMac[6];
  esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
  if (ret == ESP_OK) {

    sprintf(MacStr, "%02x:%02x:%02x:%02x:%02x:%02x",
            baseMac[0], baseMac[1], baseMac[2],
            baseMac[3], baseMac[4], baseMac[5]);
  }
  else {
    Serial.println("Failed to read MAC address");
    tft.println("MAC read failed");
  }
}

void parseMacAddressStr(const char* macStr, uint8_t* macBytes) {
  sscanf(macStr, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
         &macBytes[0], &macBytes[1], &macBytes[2],
         &macBytes[3], &macBytes[4], &macBytes[5]);
}
