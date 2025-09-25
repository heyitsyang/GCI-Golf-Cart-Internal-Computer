#include <SPI.h>
#include <TFT_eSPI.h> // Include the TFT_eSPI library
#include <WiFi.h>
#include <esp_wifi.h>
#include "prototypes.h"

#define SCREEN_TIMEOUT 30 // seconds
#define BUTTON_PIN 27     // GPIO34-39 do not have pullups

TFT_eSPI tft = TFT_eSPI(); // Create a TFT_eSPI object

unsigned long screenStartTime = 0;
bool screenOn = true;

void setup(void) {
  Serial.begin(115200);

  // Initialize button pin with pullup
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Initialize WiFi to enable MAC address reading
  WiFi.mode(WIFI_STA);

  tft.init(); // Initialize the display
  tft.setRotation(1); // Set the rotation to landscape mode (optional)

  tft.fillScreen(TFT_BLACK); // Clear the screen to black
  tft.setTextColor(TFT_WHITE, TFT_BLACK); // Set text color to white
  tft.setCursor(1, 10); // Set cursor position

  tft.println();
  tft.setTextFont(2); // Set font size
  tft.print("MAC ");
  readMacAddress();

  // Record screen start time
  screenStartTime = millis();
}

void loop() {
  // Debug: Print button state
  static unsigned long lastDebugTime = 0;
  if (millis() - lastDebugTime > 2000) { // Every 2 seconds
    Serial.print("Button pin state: ");
    Serial.println(digitalRead(BUTTON_PIN) ? "HIGH" : "LOW");
    lastDebugTime = millis();
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
}

void readMacAddress(){
  uint8_t baseMac[6];
  esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
  if (ret == ESP_OK) {
    char macStr[18];
    sprintf(macStr, "%02x:%02x:%02x:%02x:%02x:%02x",
            baseMac[0], baseMac[1], baseMac[2],
            baseMac[3], baseMac[4], baseMac[5]);

    tft.setTextFont(4); // Set font size
    tft.println(macStr);        // Display on TFT
    Serial.println(macStr);     // Display on Serial
  }
  else {
    Serial.println("Failed to read MAC address");
    tft.println("MAC read failed");
  }
}
