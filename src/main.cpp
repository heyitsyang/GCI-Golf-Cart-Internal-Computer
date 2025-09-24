#include <SPI.h>
#include <TFT_eSPI.h> // Include the TFT_eSPI library
#include <WiFi.h>
#include <esp_wifi.h>

TFT_eSPI tft = TFT_eSPI(); // Create a TFT_eSPI object

void setup(void) {
  Serial.begin(115200);

  tft.init(); // Initialize the display
  tft.setRotation(1); // Set the rotation to landscape mode (optional)

  tft.fillScreen(TFT_BLACK); // Clear the screen to black
  tft.setTextFont(4); // Set font size
  tft.setTextColor(TFT_WHITE, TFT_BLACK); // Set text color to white
  tft.setCursor(10, 10); // Set cursor position

  tft.println("Hello, IdeaSpark!");
  tft.println("ST7789 Display");
}

void loop() {
  // Your code here
}
