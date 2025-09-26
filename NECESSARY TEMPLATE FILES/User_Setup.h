#define USER_SETUP_ID 100 // Custom setup for IdeaSpark ESP32

// Define the ST7789V driver
#define ST7789_DRIVER

// Display dimensions
#define TFT_WIDTH 135
#define TFT_HEIGHT 240

// Pin definitions for the board (as listed in manufacturer specs)
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  4
#define TFT_BL   32 // Backlight control pin

// Optional: Enable the read-back functionality
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
// #define LOAD_FONT6
// #define LOAD_FONT7
// #define LOAD_FONT8
// #define LOAD_GFXFF

// Optional: Use SPI
#define SPI_FREQUENCY  27000000
#define SPI_READ_FREQUENCY  20000000

// For better colors, you may need to invert the display
#define TFT_INVERT_COLORS
