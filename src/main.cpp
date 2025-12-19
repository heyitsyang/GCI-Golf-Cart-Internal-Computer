#include <SPI.h>
#include <TFT_eSPI.h> // Include the TFT_eSPI library
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h>
#include <esp_sleep.h>

#include "prototypes.h"
#include "display.h"

#define SCREEN_TIMEOUT 60 // seconds
#define PAIRED_MAC_MSG_TIMEOUT 5 // seconds
#define BUTTON_HOLD_ERASE_SECS 5 // seconds to hold button to erase paired MAC

#define GCD_PERIODIC_SEND_SECS 5  //update data to GCD
#define HEARTBEAT_MISS_THRESHOLD 4  // Number of missed heartbeats before connection is considered lost

#define SLEEP_PIN 5         // pull LOW to sleep
#define BUTTON_PIN 12       // GPIO34-39 do not have pullups
#define ONE_WIRE_PIN 13     // DS18B20 temperature sensor and any other One-Wire
#define ADC_FUEL_PIN 36     // 3.3v max
#define ADC_BATTERY_PIN 39  // 3.3v max

#define ESPNOW_CHANNEL 1
#define ESPNOW_MAX_PAYLOAD 240  // Max payload after wrapper overhead subtracted (ESP-NOW limit: 250 bytes, wrapper: 9 bytes, payload: 241 bytes)

#define DISPLAY_ORIENTATION 1       // 0 = Normal, 1 = Flipped (180 degrees)

// ESP-NOW message types (must match GCD)
typedef enum {
    ESPNOW_MSG_TEXT = 0,
    ESPNOW_MSG_GPS_DATA = 1,
    ESPNOW_MSG_TELEMETRY = 2,
    ESPNOW_MSG_COMMAND = 3,
    ESPNOW_MSG_ACK = 4,
    ESPNOW_MSG_HEARTBEAT = 5
} espnow_msg_type_t;

// ESP-NOW message structure (must match GCD)
typedef struct __attribute__((packed)) {
    uint8_t type;
    uint32_t timestamp;
    uint16_t msg_id;
    uint16_t data_len;
    uint8_t data[ESPNOW_MAX_PAYLOAD];
} espnow_message_t;

// Calculate actual packet size for sending
#define ESPNOW_PACKET_HEADER_SIZE 9
#define ESPNOW_PACKET_SIZE(data_len) (ESPNOW_PACKET_HEADER_SIZE + (data_len))

TFT_eSPI tft = TFT_eSPI(); // Create a TFT_eSPI object

OneWire oneWire(ONE_WIRE_PIN);        // Setup a oneWire instance to communicate with any OneWire device
DallasTemperature sensors(&oneWire);  // Pass oneWire reference to DallasTemperature library

Preferences preferences;

unsigned long screenStartTime = 0;
bool screenOn = true;
unsigned long lastGcdSendTime = 0;
uint16_t next_msg_id = 0;

unsigned long buttonPressStartTime = 0;
bool buttonWasPressed = false;
bool enteringSleep = false;  // Flag to prevent multiple sleep attempts

char thisDeviceMacStr[18];
char pairedMacStr[18] = "";  // Store the paired MAC address string for display updates
int consecutiveHeartbeatsMissed = HEARTBEAT_MISS_THRESHOLD;  // Start disconnected until first heartbeat
unsigned long lastHeartbeatCheckTime = 0;

// variables for outbound data
int modeHeadLights = -99;
int outdoorLuminosity = -99;
float airTemperature = -99;
float battVoltage = -99;
float fuelLevel = -99;

// Golf cart command codes (must match GCD)
typedef enum {
    GCI_CMD_NONE = 0,
    GCI_CMD_ADD_PEER = 1,     // Add GCD MAC to GCI peer list
    GCI_CMD_REMOVE_PEER = 2,  // Future: remove peer
    GCI_CMD_REBOOT = 3,       // Future: reboot GCI
    // Add more commands as needed
} gci_command_t;

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
  uint8_t macAddr[6];  // For pairing command and future use
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

  //set pinModes
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(SLEEP_PIN, INPUT_PULLUP);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH); // Turn on backlight initially

  // Initialize WiFi to enable MAC address reading
  WiFi.mode(WIFI_STA);

  tft.init(); // Initialize the display
  tft.setRotation(DISPLAY_ORIENTATION == 0 ? 1 : 3); // 1 = landscape normal, 3 = landscape flipped

  clearScreen(tft);
  tft.setTextColor(TFT_WHITE, TFT_BLACK); // Set text color to white

  // Display this device's MAC address
  createMacAddressStr(thisDeviceMacStr);
  displayMacLine(tft, thisDeviceMacStr);
  Serial.println(thisDeviceMacStr);     // Display on Serial

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register callbacks
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

  Serial.println("ESP-NOW initialized");

  // Open preferences and check for saved peer MAC
  preferences.begin("gci", false);

  // Try to load saved peer MAC address
  uint8_t savedMac[6] = {0};
  size_t macLen = preferences.getBytes("peer_mac", savedMac, 6);

  bool hasSavedPeer = false;
  if (macLen == 6) {
    // Check if MAC is valid (not all zeros)
    bool isValid = false;
    for (int i = 0; i < 6; i++) {
      if (savedMac[i] != 0) {
        isValid = true;
        break;
      }
    }

    if (isValid) {
      // Add saved peer
      esp_now_peer_info_t peerInfo = {};
      memcpy(peerInfo.peer_addr, savedMac, 6);
      peerInfo.channel = ESPNOW_CHANNEL;
      peerInfo.ifidx = WIFI_IF_STA;
      peerInfo.encrypt = false;

      if (esp_now_add_peer(&peerInfo) == ESP_OK) {
        sprintf(pairedMacStr, "%02x:%02x:%02x:%02x:%02x:%02x",
                savedMac[0], savedMac[1], savedMac[2],
                savedMac[3], savedMac[4], savedMac[5]);
        Serial.printf("Loaded saved peer: %s\n", pairedMacStr);

        // Display paired status (starts RED until heartbeat received)
        displayPairingLine(tft, PAIRED_DISCONNECTED, pairedMacStr);

        hasSavedPeer = true;
      }
    }
  }

  if (!hasSavedPeer) {
    Serial.println("No saved peer - waiting for pairing commands from GCD");

    // Display pairing status
    displayPairingLine(tft, WAITING_FOR_PAIRING, "");
  }
  screenStartTime = millis();   // Record screen start time

} /* END SETUP */


/**************
 *    LOOP    *
 **************/

void loop() {

  static float tempC_0, tempF_0;
  static int rawFuelADC, rawBattADC;
  static float voltsFuelADC, voltsBattADC;
  static int percentFuel;
  static bool sendData = false;

  // Check if SLEEP_PIN is LOW (sleep requested)
  if (!enteringSleep && digitalRead(SLEEP_PIN) == LOW) {
    delay(50); // Debounce
    if (digitalRead(SLEEP_PIN) == LOW) { // Confirm still LOW
      enteringSleep = true;  // Prevent re-entry
      enterDeepSleep();
    }
  }

  // Check for button state
  bool buttonPressed = (digitalRead(BUTTON_PIN) == LOW);

  if (buttonPressed && !buttonWasPressed) {
    // Button just pressed
    buttonPressStartTime = millis();
    buttonWasPressed = true;
  } else if (buttonPressed && buttonWasPressed) {
    // Button is being held
    unsigned long pressDuration = millis() - buttonPressStartTime;

    if (pressDuration >= (BUTTON_HOLD_ERASE_SECS * 1000)) {
      // Long press threshold reached - erase paired MAC
      preferences.remove("peer_mac");
      Serial.println("Paired MAC address erased from EEPROM");

      // Display message
      tft.fillScreen(TFT_BLACK);
      tft.setCursor(10, 60);
      tft.setTextFont(4);
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.println("Paired MAC address");
      tft.setCursor(10, 90);
      tft.println("erased");

      // Wait for message timeout
      delay(PAIRED_MAC_MSG_TIMEOUT * 1000);

      // Reboot
      Serial.println("Rebooting...");
      ESP.restart();
    }
  } else if (!buttonPressed && buttonWasPressed) {
    // Button just released (short press)
    if (!screenOn) {
      // Turn screen back on
      digitalWrite(TFT_BL, HIGH);
      screenOn = true;

      // Redraw display header
      redrawDisplayHeader();

      sendData = true;
      screenStartTime = millis();
    }

    buttonWasPressed = false;
    delay(200); // Debounce delay
  }

  // Check if screen timeout has been reached
  if (screenOn && (millis() - screenStartTime) >= (SCREEN_TIMEOUT * 1000)) {
    // Turn off the backlight only
    digitalWrite(TFT_BL, LOW);  // Turn off backlight
    screenOn = false;
  }

  // Check if we have a paired device before attempting to send
  esp_now_peer_info_t peer;
  bool hasPeer = (esp_now_fetch_peer(true, &peer) == ESP_OK);

  // Check for heartbeat timeout and update connection status display
  static bool lastConnectionStatus = false;  // Track if we were connected last check
  bool isConnected = (consecutiveHeartbeatsMissed < HEARTBEAT_MISS_THRESHOLD);

  // Check if we need to increment missed heartbeat counter (every 10 seconds expected)
  if (hasPeer && (millis() - lastHeartbeatCheckTime) >= 10000) {
    consecutiveHeartbeatsMissed++;
    lastHeartbeatCheckTime = millis();

    if (consecutiveHeartbeatsMissed >= HEARTBEAT_MISS_THRESHOLD) {
      Serial.printf("Connection lost - missed %d heartbeats\n", consecutiveHeartbeatsMissed);
    }
  }

  // Update display if connection status changed
  if (hasPeer && (isConnected != lastConnectionStatus) && strlen(pairedMacStr) > 0) {
    PairingStatus status = isConnected ? PAIRED_CONNECTED : PAIRED_DISCONNECTED;
    displayPairingLine(tft, status, pairedMacStr);
  }

  lastConnectionStatus = isConnected;

  // Read sensors and send data to GCD periodically (only if paired)
  if (hasPeer && ((millis() - lastGcdSendTime) >= (GCD_PERIODIC_SEND_SECS * 1000) || sendData) ){

    // Read temperature sensor
    sensors.requestTemperatures();
    tempC_0 = sensors.getTempCByIndex(0);
    tempF_0 = tempC_0 * 9.0 / 5.0 + 32.0;

    // Read ADC values
    rawFuelADC = analogRead(ADC_FUEL_PIN);
    rawBattADC = analogRead(ADC_BATTERY_PIN);

    // Convert ADC values to volts (0-4095 -> 0-3.3V)
    voltsFuelADC = (rawFuelADC / 4095.0) * 3.3;
    voltsBattADC = (rawBattADC / 4095.0) * 3.3;

    // Calculate fuel percentage from voltage
    float fuelCalc = (-90.21 * voltsFuelADC) + 105.55;
    percentFuel = (int)fuelCalc;

    // Limit percentFuel to 0-100 range
    if (percentFuel > 100) percentFuel = 100;
    if (percentFuel < 0) percentFuel = 0;

    // stuff the dataToGcd structure for transmission
    dataToGcd.modeLights = modeHeadLights;
    dataToGcd.outdoorLum = outdoorLuminosity;
    dataToGcd.airTemp = tempF_0;
    dataToGcd.battVolts = voltsBattADC;
    dataToGcd.fuel = percentFuel;

    sendData = true;

    // Wrap telemetry data in espnow_message_t
    espnow_message_t msg;
    msg.type = ESPNOW_MSG_TELEMETRY;
    msg.timestamp = millis();
    msg.msg_id = next_msg_id++;
    msg.data_len = sizeof(dataToGcd);
    memcpy(msg.data, &dataToGcd, sizeof(dataToGcd));

    // Send header + actual telemetry payload
    esp_err_t result = esp_now_send(peer.peer_addr, (uint8_t *) &msg, ESPNOW_PACKET_SIZE(sizeof(dataToGcd)));
    lastGcdSendTime = millis();
  }

  // Update display only when new data is available
  if (sendData) {
    // Display temperature
    bool sensorConnected = (tempC_0 != DEVICE_DISCONNECTED_C);
    displayTempLine(tft, tempF_0, sensorConnected);

    // Display fuel and battery
    displayFuelBattLine(tft, voltsFuelADC, voltsBattADC);

    // Print compressed telemetry format to serial
    char gcdMacStr[18];
    if (hasPeer) {
      sprintf(gcdMacStr, "%02x:%02x:%02x:%02x:%02x:%02x",
              peer.peer_addr[0], peer.peer_addr[1], peer.peer_addr[2],
              peer.peer_addr[3], peer.peer_addr[4], peer.peer_addr[5]);
    }
    Serial.printf("Telemetry to %s : Lights=%d, Lum=%d, Temp=%.1f, Batt=%.2f, Fuel=%.1f\n",
                  hasPeer ? gcdMacStr : "No Peer",
                  modeHeadLights, outdoorLuminosity, tempF_0, voltsBattADC, (float)percentFuel);

    sendData = false; // Reset flag after display update
  }

  delay(100); // Check every 100ms for responsive button

}  /* END LOOP */


/*******************
 *    FUNCTIONS    *
 *******************/

void redrawDisplayHeader() {
  // Redraw MAC address line
  displayMacLine(tft, thisDeviceMacStr);

  // Redraw paired status line if we have a peer
  if (strlen(pairedMacStr) > 0) {
    bool isConnected = (consecutiveHeartbeatsMissed < HEARTBEAT_MISS_THRESHOLD);
    PairingStatus status = isConnected ? PAIRED_CONNECTED : PAIRED_DISCONNECTED;
    displayPairingLine(tft, status, pairedMacStr);
  }
}

void BeforeSleeping() {
  // Turn on backlight to ensure message is visible
  digitalWrite(TFT_BL, HIGH);

  // Clear display and show sleep message
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(1, 40);
  tft.setTextFont(4);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.println("ENTERING\nSLEEP MODE");

  // Display message for 2 seconds
  delay(2000);
}

void enterDeepSleep() {
  // Execute pre-sleep tasks
  BeforeSleeping();
  // Clear and turn off display
  tft.fillScreen(TFT_BLACK);
  digitalWrite(TFT_BL, LOW);  // Turn off backlight

  // Put display into sleep mode to release SPI bus
  tft.writecommand(0x10); // Sleep mode command for ST7789

  // Shutdown WiFi and ESP-NOW to save power
  esp_now_deinit();
  WiFi.disconnect(true);
  esp_wifi_stop();

  // Configure GPIO wake-up on SLEEP_PIN going HIGH
  esp_err_t err = gpio_wakeup_enable((gpio_num_t)SLEEP_PIN, GPIO_INTR_HIGH_LEVEL);
  if (err != ESP_OK) {
    return; // Failed to configure wake-up
  }

  err = esp_sleep_enable_gpio_wakeup();
  if (err != ESP_OK) {
    return; // Failed to enable GPIO wakeup
  }

  // Small delay to ensure operations complete
  delay(100);

  // Enter light sleep (will wake when SLEEP_PIN goes HIGH)
  esp_light_sleep_start();

  // After waking from light sleep, reboot for clean state
  ESP.restart();
}

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

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (status != ESP_NOW_SEND_SUCCESS) {
    Serial.println("Send Status: Fail");
  }
  if (status ==0){
    tx_success = "Delivery Success :)";
  }
  else{
    tx_success = "Delivery Fail :(";
  }
}

// Callback when data is received (promiscuous mode for pairing)
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  // Check for RAW pairing command (before any peers exist)
  if (len == sizeof(structMsgFromGcd)) {
    memcpy(&dataFromGcd, incomingData, sizeof(dataFromGcd));

    if (dataFromGcd.cmdNumber == GCI_CMD_ADD_PEER) {
      // Extract MAC address from command payload
      uint8_t newPeerMac[6];
      memcpy(newPeerMac, dataFromGcd.macAddr, 6);

      sprintf(pairedMacStr, "%02x:%02x:%02x:%02x:%02x:%02x",
              newPeerMac[0], newPeerMac[1], newPeerMac[2],
              newPeerMac[3], newPeerMac[4], newPeerMac[5]);

      // Check if peer already exists
      if (!esp_now_is_peer_exist(newPeerMac)) {
        // Add new peer
        esp_now_peer_info_t newPeer = {};  // Initialize all fields to zero
        memcpy(newPeer.peer_addr, newPeerMac, 6);
        newPeer.channel = ESPNOW_CHANNEL;
        newPeer.ifidx = WIFI_IF_STA;  // WiFi Station interface
        newPeer.encrypt = false;

        if (esp_now_add_peer(&newPeer) == ESP_OK) {
          Serial.printf("*** PAIRED with GCD (RAW MODE): %s ***\n", pairedMacStr);

          // Save peer MAC to preferences
          preferences.putBytes("peer_mac", newPeerMac, 6);
          Serial.println("Saved peer MAC to EEPROM");

          // Update display to show paired status (GREEN since we just received a message)
          displayPairingLine(tft, PAIRED_CONNECTED, pairedMacStr);


          // Send ACK back to GCD in WRAPPED mode
          espnow_message_t ackMsg;
          ackMsg.type = ESPNOW_MSG_ACK;
          ackMsg.timestamp = millis();
          ackMsg.msg_id = next_msg_id++;
          ackMsg.data_len = 0;

          Serial.printf("Sending ACK (%d bytes) to GCD...\n", ESPNOW_PACKET_SIZE(0));
          esp_err_t result = esp_now_send(newPeerMac, (uint8_t*)&ackMsg, ESPNOW_PACKET_SIZE(0));
          if (result == ESP_OK) {
            Serial.println("Sent ACK to GCD - Switched to WRAPPED mode for communication");
          } else {
            Serial.printf("Failed to send ACK to GCD - Error code: %d\n", result);
          }
        } else {
          Serial.println("Failed to add peer");
        }
      } else {
        Serial.println("Peer already exists");
      }
    }
    return;
  }

  // Check if this is a wrapped message
  if (len >= ESPNOW_PACKET_HEADER_SIZE) {
    // Filter wrapped messages - only accept from registered peers
    if (!esp_now_is_peer_exist(mac)) {
      Serial.println("ESP-NOW: Ignoring message from unknown peer");
      return;
    }

    // Reset heartbeat counter - we received a message from GCD
    consecutiveHeartbeatsMissed = 0;
    lastHeartbeatCheckTime = millis();

    espnow_message_t* msg = (espnow_message_t*)incomingData;

    char mac_str[18];
    sprintf(mac_str, "%02x:%02x:%02x:%02x:%02x:%02x",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    Serial.printf("Received %s from %s\n",
                  msg->type == ESPNOW_MSG_COMMAND ? "COMMAND" :
                  msg->type == ESPNOW_MSG_TEXT ? "TEXT" :
                  msg->type == ESPNOW_MSG_HEARTBEAT ? "HEARTBEAT" : "MESSAGE",
                  mac_str);

    // Route by message type
    switch (msg->type) {
      case ESPNOW_MSG_COMMAND: {
        // Extract command from message data
        memcpy(&dataFromGcd, msg->data, sizeof(dataFromGcd));
        Serial.print("Command: ");
        Serial.println(dataFromGcd.cmdNumber);
        cmdFromGcd = dataFromGcd.cmdNumber;

        // Process commands
        switch (dataFromGcd.cmdNumber) {
          case GCI_CMD_ADD_PEER: {
            // Extract MAC address from command payload
            uint8_t newPeerMac[6];
            memcpy(newPeerMac, dataFromGcd.macAddr, 6);

            // Check if peer already exists
            if (!esp_now_is_peer_exist(newPeerMac)) {
              // Add new peer
              esp_now_peer_info_t newPeer;
              memcpy(newPeer.peer_addr, newPeerMac, 6);
              newPeer.channel = ESPNOW_CHANNEL;
              newPeer.encrypt = false;

              if (esp_now_add_peer(&newPeer) == ESP_OK) {
                Serial.printf("*** PAIRED with GCD: %02X:%02X:%02X:%02X:%02X:%02X ***\n",
                             newPeerMac[0], newPeerMac[1], newPeerMac[2],
                             newPeerMac[3], newPeerMac[4], newPeerMac[5]);

                // Update display to show paired status (connected since we just received message)
                displayPairingLine(tft, PAIRED_CONNECTED, mac_str);
              } else {
                Serial.println("Failed to add peer");
              }
            } else {
              Serial.println("Peer already exists");
            }
            break;
          }

          case GCI_CMD_REMOVE_PEER:
            Serial.println("Remove peer command received (not implemented)");
            break;

          case GCI_CMD_REBOOT:
            Serial.println("Reboot command received (not implemented)");
            break;

          default:
            Serial.println("Unknown command");
            break;
        }
        break;
      }

      case ESPNOW_MSG_TEXT:
        Serial.print("Text message: ");
        Serial.println((char*)msg->data);
        break;

      case ESPNOW_MSG_HEARTBEAT:
        // Respond to heartbeat from known peer (closed-loop keepalive)
        if (esp_now_is_peer_exist(mac)) {
          Serial.printf("Heartbeat from %s - sending response\n", mac_str);

          // Send heartbeat response back to GCD
          espnow_message_t response;
          response.type = ESPNOW_MSG_HEARTBEAT;
          response.timestamp = millis();
          response.msg_id = next_msg_id++;
          response.data_len = 4;
          memcpy(response.data, &response.timestamp, 4);

          esp_now_send(mac, (uint8_t*)&response, ESPNOW_PACKET_SIZE(4));
        }
        break;

      default:
        Serial.print("Unknown message type: ");
        Serial.println(msg->type);
        break;
    }
  } else {
    Serial.print("Received unexpected message size: ");
    Serial.println(len);
  }
}