

## 🔧 Display Functions

### **Style Helpers**
- `setLabelStyle(tft)` - Small cyan text
- `setValueStyle(tft)` - Large white text

### **Line Display Functions**
- `displayMacLine(tft, macAddress)` - Shows device MAC
- `displayPairingLine(tft, status, pairedMac)` - Shows pairing with color
- `displayTempLine(tft, tempF, sensorConnected)` - Shows temperature
- `displayFuelBattLine(tft, fuelRaw, battRaw)` - Shows fuel & battery

### **Utility Functions**
- `clearScreen(tft)` - Clear entire screen
- `redrawAllLines(...)` - Redraw all lines at once (future use)

