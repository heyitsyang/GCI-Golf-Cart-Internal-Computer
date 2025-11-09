# Display Refactoring - Migration Guide

## What Changed?

We've simplified the display code by creating reusable functions instead of repeating the same code patterns throughout `main.cpp`.

## New Files Created

1. **include/display.h** - Display constants, colors, and function declarations
2. **src/display.cpp** - Display function implementations

## Benefits

- ✅ **No duplicate code** - Pairing display was in 6+ places, now in 1 function
- ✅ **Easy to change layout** - Modify Y-coordinates in one place (display.h)
- ✅ **Easy to change colors** - Named colors instead of `TFT_CYAN` scattered everywhere
- ✅ **Clear function names** - `displayPairingLine()` vs scattered tft calls
- ✅ **Easier to test** - Each function does one thing

---

## How to Use the New Functions

### Before (Old Way):
```cpp
// This code was repeated 6+ times throughout main.cpp
tft.fillRect(0, 28, 320, 16, TFT_BLACK);
tft.setCursor(1, 28);
tft.setTextFont(2);
tft.setTextColor(TFT_CYAN, TFT_BLACK);
tft.print("PR");
tft.setTextColor(isConnected ? TFT_GREEN : TFT_RED, TFT_BLACK);
tft.setTextFont(4);
tft.print(pairedMacStr);
```

### After (New Way):
```cpp
// One simple function call
PairingStatus status = isConnected ? PAIRED_CONNECTED : PAIRED_DISCONNECTED;
displayPairingLine(tft, status, pairedMacStr);
```

---

## Migration Examples

### Example 1: Updating Pairing Status

**Old code (lines 192-199, 318-325, 434-441, etc.):**
```cpp
tft.fillRect(0, 28, 320, 16, TFT_BLACK);
tft.setCursor(1, 28);
tft.setTextFont(2);
tft.setTextColor(TFT_CYAN, TFT_BLACK);
tft.print("PR");
tft.setTextColor(TFT_RED, TFT_BLACK);
tft.setTextFont(4);
tft.print(pairedMacStr);
```

**New code:**
```cpp
displayPairingLine(tft, PAIRED_DISCONNECTED, pairedMacStr);
```

---

### Example 2: Showing Temperature

**Old code (lines 366-384):**
```cpp
tft.fillRect(60, 54, 260, 18, TFT_BLACK);
tft.setCursor(1, 54);
tft.setTextFont(2);
tft.setTextColor(TFT_CYAN, TFT_BLACK);
tft.print("TEMP ");
tft.setTextColor(TFT_WHITE, TFT_BLACK);
tft.setTextFont(4);

if (tempC_0 != DEVICE_DISCONNECTED_C) {
    tft.print(tempF_0);
    tft.println("°F");
} else {
    tft.println("No sensor");
}
```

**New code:**
```cpp
bool sensorConnected = (tempC_0 != DEVICE_DISCONNECTED_C);
displayTempLine(tft, tempF_0, sensorConnected);
```

---

### Example 3: Redrawing Full Display Header

**Old code (redrawDisplayHeader function, lines 420-443):**
```cpp
void redrawDisplayHeader() {
    // Redraw MAC address line
    tft.fillRect(0, 0, 320, 28, TFT_BLACK);
    tft.setCursor(1, 2);
    tft.setTextFont(2);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.print("MAC");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(4);
    tft.println(thisDeviceMacStr);

    // Redraw paired status line if we have a peer
    if (strlen(pairedMacStr) > 0) {
        bool isConnected = (consecutiveHeartbeatsMissed < HEARTBEAT_MISS_THRESHOLD);
        tft.fillRect(0, 28, 320, 16, TFT_BLACK);
        tft.setCursor(1, 28);
        tft.setTextFont(2);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.print("PR");
        tft.setTextColor(isConnected ? TFT_GREEN : TFT_RED, TFT_BLACK);
        tft.setTextFont(4);
        tft.print(pairedMacStr);
    }
}
```

**New code:**
```cpp
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
```

---

## Quick Reference: All Display Functions

### Style Helpers
- `setLabelStyle(tft)` - Small cyan text for labels
- `setValueStyle(tft)` - Large white text for values

### Line Display Functions
- `displayMacLine(tft, macAddress)` - Show device MAC address
- `displayPairingLine(tft, status, pairedMac)` - Show pairing status
- `displayTempLine(tft, tempF, sensorConnected)` - Show temperature
- `displayFuelBattLine(tft, fuelRaw, battRaw)` - Show fuel and battery

### Full Screen
- `clearScreen(tft)` - Clear entire screen
- `redrawAllLines(...)` - Redraw all display lines at once

### Pairing Status Values
- `WAITING_FOR_PAIRING` - Yellow "WAITING FOR PAIRING..."
- `PAIRED_CONNECTED` - Green MAC address
- `PAIRED_DISCONNECTED` - Red MAC address

---

## Implementation Steps

1. ✅ Create `include/display.h` and `src/display.cpp`
2. ⏳ Update `main.cpp` to use new functions (replace old code)
3. ⏳ Update `prototypes.h` (remove old display functions, they're in display.h now)
4. ⏳ Compile and test

---

## Notes

- No changes to display behavior - only code organization
- All existing features work exactly the same
- Can migrate incrementally (old and new code can coexist temporarily)
- Easy to change layout: just update constants in `display.h`
