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

#define GCD_PERIODIC_SEND_SECS 5  //update data to GCD

#define BUTTON_PIN 12       // GPIO34-39 do not have pullups
#define ONE_WIRE_PIN 13     // DS18B20 temperature sensor and any other One-Wire
#define ADC_FUEL_PIN 36     // 3.3v max
#define ADC_BATTERY_PIN 39  // 3.3v max

#ifndef GC_DISP_MAC_ADDR_STR                        // if not already defined in secrets.h
  #define GC_DISP_MAC_ADDR_STR "A1:B2:C3:D4:E5:F6"  // replace with MAC address of the Golf Cart Display
#endif

#define ESPNOW_CHANNEL 1

TFT_eSPI tft = TFT_eSPI(); // Create a TFT_eSPI object

OneWire oneWire(ONE_WIRE_PIN);        // Setup a oneWire instance to communicate with any OneWire device
DallasTemperature sensors(&oneWire);  // Pass oneWire reference to DallasTemperature library

unsigned long screenStartTime = 0;
bool screenOn = true;
unsigned long lastGcdSendTime = 0;

char thisDeviceMacStr[18], gcdMacStr[18];
uint8_t gcdAddress[6];

// variables for outbound data
int modeHeadLights = -99;
int outdoorLuminosity = -99;
float airTemperature = -99;
float battVoltage = -99;
float fuelLevel = -99;

typedef struct struct_msg_to_gcd {
  int modeLights;
  int outdoorLum;
  float airTemp;
  float battVolts;
  float fuel;
} structMsgToGcd;

structMsgToGcd dataToGcd;

// variables for inbound data
int cmdFromGcd;

typedef struct struct_msg_from_gcd {
  int cmdNumber;

} structMsgFromGcd;

structMsgFromGcd dataFromGcd;

esp_now_peer_info_t peerInfo;

String tx_success;   // Variable to store if sending data was successful


/**************
 *    SETUP   *
 **************/

void setup(void) {
  Serial.begin(9600);
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

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);

  // Register peer
  parseMacAddressStr(GC_DISP_MAC_ADDR_STR, gcdAddress);    // Parse GCD MAC address string into a byte array for ESP-NOW
  memcpy(peerInfo.peer_addr, gcdAddress, 6);
  peerInfo.channel = ESPNOW_CHANNEL;  // corresponds to WiFi channels 1-14 - must match for peers to communicate
  peerInfo.encrypt = false;
  
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  // Register for a callback function that will be called when data is received
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));


  screenStartTime = millis();   // Record screen start time

} /* END SETUP */


/**************
 *    LOOP    *
 **************/

void loop() {

  static float tempC_0, tempF_0;
  static int raw_fuel, raw_batt;
  static bool sendData = false;

  // Check for button press to turn screen back on
  if (!screenOn && digitalRead(BUTTON_PIN) == LOW) {
    // Turn on the backlight
    digitalWrite(TFT_BL, HIGH);  // Turn on backlight
    screenOn = true;
    sendData = true;
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

  // Read sensors and send data to GCD periodically
  if ( ((millis() - lastGcdSendTime) >= (GCD_PERIODIC_SEND_SECS * 1000) || sendData) ){

    // Read temperature sensor
    sensors.requestTemperatures();
    tempC_0 = sensors.getTempCByIndex(0);
    tempF_0 = tempC_0 * 9.0 / 5.0 + 32.0;

    // Read ADC values
    raw_fuel = analogRead(ADC_FUEL_PIN);
    raw_batt = analogRead(ADC_BATTERY_PIN);


    // stuff the dataToGcd structure for transmission
    dataToGcd.modeLights = modeHeadLights;
    dataToGcd.outdoorLum = outdoorLuminosity;
    dataToGcd.airTemp = tempF_0;
    dataToGcd.battVolts = raw_batt;
    dataToGcd.fuel = raw_fuel;

    sendData = true;

    // Send data to GCD (Golf Cart Display)
    esp_err_t result = esp_now_send(gcdAddress, (uint8_t *) &dataToGcd, sizeof(dataToGcd));
    lastGcdSendTime = millis();

    if (result == ESP_OK) {
      Serial.println("\nData sent to API successfully");
    } else {
      Serial.println("Error sending data to API");
    }
  }

  // Update display only when new data is available
  if (sendData) {
    // Clear temperature value area
    tft.fillRect(60, 36, 260, 20, TFT_BLACK);

    tft.setCursor(1, 36); // Set cursor position
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

    // Clear fuel and battery value areas
    tft.fillRect(60, 60, 260, 20, TFT_BLACK);

    tft.setCursor(1, 60); // Set cursor position
    tft.setTextFont(2); // Set font size small
    tft.print("FUEL ");
    tft.setTextFont(4); // Set font size medium
    tft.print(raw_fuel);
    Serial.print("raw_fuel: ");
    Serial.println(raw_fuel);

    tft.setTextFont(2); // Set font size small
    tft.print("        BATT ");
    tft.setTextFont(4); // Set font size medium
    tft.print(raw_batt);
    Serial.print("raw_batt: ");
    Serial.println(raw_batt);

    sendData = false; // Reset flag after display update
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

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Last Packet Send Status:    ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  if (status ==0){
    tx_success = "Delivery Success :)";
  }
  else{
    tx_success = "Delivery Fail :(";
  }
}

// Callback when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&dataFromGcd, incomingData, sizeof(dataFromGcd));
  Serial.print("Bytes received: ");
  Serial.println(len);
  cmdFromGcd = dataFromGcd.cmdNumber;

}