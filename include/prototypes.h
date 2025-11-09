
/*
 * PROTOTYPES
 */

// Display functions are now in display.h
void redrawDisplayHeader();

// Sleep functions
void BeforeSleeping();
void enterDeepSleep();

// MAC address utilities
void createMacAddressStr(char* MacStr);
void parseMacAddressStr(const char* macStr, uint8_t* macBytes);

// ESP-NOW callbacks
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len);
