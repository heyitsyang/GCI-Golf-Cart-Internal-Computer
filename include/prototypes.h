
/*
 * PROTOTYPES
 */

void redrawDisplayHeader();
void BeforeSleeping();
void enterDeepSleep();
void createMacAddressStr(char* MacStr);
void parseMacAddressStr(const char* macStr, uint8_t* macBytes);
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len);
