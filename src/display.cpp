/*
 * DISPLAY.CPP - Implementation of simple display functions
 *
 * Each function here handles ONE specific part of the display.
 * This makes it easy to update the display without duplicating code.
 */

#include "display.h"

// ============================================================================
// TEXT STYLE HELPERS
// ============================================================================

void setLabelStyle(TFT_eSPI &tft) {
    tft.setTextFont(2);                    // Small font
    tft.setTextColor(COLOR_LABEL, TFT_BLACK);  // Cyan text
}

void setValueStyle(TFT_eSPI &tft) {
    tft.setTextFont(4);                    // Medium font
    tft.setTextColor(COLOR_VALUE, TFT_BLACK);  // White text
}

// ============================================================================
// LINE-BY-LINE DISPLAY FUNCTIONS
// ============================================================================

void displayMacLine(TFT_eSPI &tft, const char* macAddress) {
    // Clear the line
    tft.fillRect(0, MAC_LINE_Y, SCREEN_WIDTH, 26, TFT_BLACK);

    // Draw label
    tft.setCursor(1, MAC_LINE_Y);
    setLabelStyle(tft);
    tft.print("MAC");

    // Draw value
    setValueStyle(tft);
    tft.println(macAddress);
}

void displayPairingLine(TFT_eSPI &tft, PairingStatus status, const char* pairedMac) {
    // Clear the line
    tft.fillRect(0, PAIRED_LINE_Y, SCREEN_WIDTH, 16, TFT_BLACK);
    tft.setCursor(1, PAIRED_LINE_Y);

    if (status == WAITING_FOR_PAIRING) {
        // Show yellow "waiting" message
        tft.setTextFont(2);
        tft.setTextColor(COLOR_WAITING, TFT_BLACK);
        tft.print("WAITING FOR PAIRING...");
    } else {
        // Show "PR" label
        setLabelStyle(tft);
        tft.print("PR");

        // Show MAC address in green (connected) or red (disconnected)
        tft.setTextFont(4);
        uint16_t color = (status == PAIRED_CONNECTED) ? COLOR_CONNECTED : COLOR_DISCONNECTED;
        tft.setTextColor(color, TFT_BLACK);
        tft.print(pairedMac);
    }
}

void displayTempLine(TFT_eSPI &tft, float tempF, bool sensorConnected) {
    // Clear the value area (keep label area)
    tft.fillRect(60, TEMP_LINE_Y, 260, 18, TFT_BLACK);

    // Draw label
    tft.setCursor(1, TEMP_LINE_Y);
    setLabelStyle(tft);
    tft.print("TEMP ");

    // Draw value
    setValueStyle(tft);
    if (sensorConnected) {
        tft.print(tempF);
        tft.println(" F");  // Note: degree symbol removed for simplicity
    } else {
        tft.println("No sensor");
    }
}

void displayFuelBattLine(TFT_eSPI &tft, float fuelVolts, float battVolts) {
    // Clear the value area
    tft.fillRect(60, FUEL_LINE_Y, 260, 18, TFT_BLACK);

    // Draw FUEL
    tft.setCursor(1, FUEL_LINE_Y);
    setLabelStyle(tft);
    tft.print("FUEL ");
    setValueStyle(tft);
    tft.print(fuelVolts, 3);  // 3 decimal places

    // Draw BATT
    setLabelStyle(tft);
    tft.print("     BATT ");
    setValueStyle(tft);
    tft.print(battVolts, 3);  // 3 decimal places
}

// ============================================================================
// FULL SCREEN OPERATIONS
// ============================================================================

void clearScreen(TFT_eSPI &tft) {
    tft.fillScreen(TFT_BLACK);
}

void redrawAllLines(TFT_eSPI &tft, const char* thisMac, PairingStatus status,
                    const char* pairedMac, float tempF, bool sensorConnected,
                    float fuelVolts, float battVolts) {
    // Redraw each line of the display
    displayMacLine(tft, thisMac);
    displayPairingLine(tft, status, pairedMac);
    displayTempLine(tft, tempF, sensorConnected);
    displayFuelBattLine(tft, fuelVolts, battVolts);
}
