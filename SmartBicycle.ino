/***************************************************
  This is a library for 1.8 SPI TFT 128*160 display.
  link more :
  http://lucidtronix.com/tutorials/13
  https://www.youtube.com/watch?v=O5YYsm_BqJQ
  http://blog.simtronyx.de/en/a-1-8-inch-tft-color-display-hy-1-8-spi-and-an-arduino/
  http://www.tweaking4all.com/hardware/arduino/sainsmart-arduino-color-display/

  Device :
    - Arduino
    - 1.8 SPI TFT 128*160 display
    - SS1305 Unipolar Hall Effect Sensor


  Pin connect :
    Arduino   <----------> HY-1.8 SPI   or  SD Card
    D4        <---------------------------> Pin 14 (CS)
    D9        <----------> Pin 07 (A0)
    D10(SS)   <----------> Pin 10 (CS)
    D11(MOSI) <----------> Pin 08 (SDA)---> Pin 12 (MOSI)
    D12(MISO) <---------------------------> Pin 13 (MISO)
    D13(SCK)  <----------> Pin 09 (SCK)---> Pin 11 (SCK)
    D8        <----------> Pin 06 (RESET)
    5V(VCC)   <----------> Pin 02 (VCC)
    GND       <----------> Pin 01 (GND)
    5V(VCC)   <----------> Pin 15 (LED+)
    GND       <----------> Pin 16 (LED-)
 ****************************************************/

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library
#include <SPI.h>
#include <SD.h>

#define TFT_CS  10
#define TFT_RST  8
#define TFT_DC   9
#define SD_CS    4

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

/********************* Initial speedometer ***************************/
String message = "Luf";
int mode = 1;
int num_modes = 2;
int cur_scroll = 0;
int cur_msg_length = 0;
int cursor_index = 0;
int char_index = 0;
int btn = 3;
int hall_pin = 16;
byte backlight_pwm = 200;
int hall_state = 1;
int revolutions = 0;
float miles_in_inches = 63360;
float kmh = 0.0, pev_kmh = 0.0;
float mph = 0.0, pev_mph = 0.0;
float distance = 0.0, pev_distance = 0.0;
float k_ipm_2_mph;
int tire_circumference;
unsigned long last_fall = 0;
unsigned long last_interval = 0;
int cur_x = 8;
int cur_y = 1;
float tire_diameter = 26; // 700X23C = 26 Inch x 1.25 Inch  
/*********************************************************************/
/************************* Clear Screen ******************************/
unsigned long previousMillis = 0;
const long interval = 1000;

void setup(void) {
  Serial.begin(9600);

  tft.initR(INITR_BLACKTAB);

  k_ipm_2_mph = 3600000 / miles_in_inches;
  tire_circumference = tire_diameter * 3.14159;
  pinMode(hall_pin, INPUT);

  Serial.print("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("failed!");
    return;
  }
  Serial.println("OK!");
  tft.setRotation(3);
//  bmpDraw("man.bmp", 0, 0);
//  delay(1000);
  tft.fillScreen(ST7735_BLACK);
}

void loop() {
  int hall_val = digitalRead(hall_pin);
  if (hall_state != hall_val && hall_val == LOW) {
    revolutions++;
    last_interval = millis() - last_fall;
    last_fall = millis();
  }
  hall_state = hall_val;
  updateSpeedAndDistance();

  Serial.print("Hall:" );
  Serial.print(hall_state);
  Serial.print("  ");
  Serial.print("Mph:");
  Serial.print(mph);
  Serial.print("  ");
  Serial.print("Miles:");
  Serial.println(distance);

  tft.setTextSize(2);
  tft.setCursor(0, 0);
  tft.setTextColor(ST7735_YELLOW);
  tft.print("  Road Bike");

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    
    kmh = mph/0.62137;
    tft.setTextColor(ST7735_BLACK);    
    tft.setCursor(0, 20);
    tft.print("Kmh = ");
    tft.println(pev_kmh);
    tft.print("Mph = ");
    tft.println(pev_mph);
    tft.print("Miles = ");
    tft.print(pev_distance);

    tft.setTextColor(ST7735_WHITE);
    tft.setCursor(0, 20);
    tft.print("Kmh = ");
    tft.println(kmh);
    tft.print("Mph = ");
    tft.println(mph);
    tft.print("Miles = ");
    tft.print(distance);
    
    pev_kmh = kmh;
    pev_mph = mph;
    pev_distance = distance;
  }


}

/*************** Update Speed And Distance **************************/
void updateSpeedAndDistance() {
  distance = tire_circumference * revolutions;
  distance /= miles_in_inches;
  float ipm = tire_circumference / (float)last_interval;
  mph = ipm * k_ipm_2_mph;
}
/*********************************************************************/
/************************ bmpDraw ************************************/
#define BUFFPIXEL 20
void bmpDraw(char *filename, uint8_t x, uint8_t y) {

  File     bmpFile;
  int      bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint8_t  sdbuffer[3 * BUFFPIXEL]; // pixel buffer (R+G+B per pixel)
  uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      w, h, row, col;
  uint8_t  r, g, b;
  uint32_t pos = 0, startTime = millis();

  if ((x >= tft.width()) || (y >= tft.height())) return;

  Serial.println();
  Serial.print("Loading image '");
  Serial.print(filename);
  Serial.println('\'');

  // Open requested file on SD card
  if ((bmpFile = SD.open(filename)) == NULL) {
    Serial.print("File not found");
    return;
  }

  // Parse BMP header
  if (read16(bmpFile) == 0x4D42) { // BMP signature
    Serial.print("File size: "); Serial.println(read32(bmpFile));
    (void)read32(bmpFile); // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image data
    Serial.print("Image Offset: "); Serial.println(bmpImageoffset, DEC);
    // Read DIB header
    Serial.print("Header size: "); Serial.println(read32(bmpFile));
    bmpWidth  = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if (read16(bmpFile) == 1) { // # planes -- must be '1'
      bmpDepth = read16(bmpFile); // bits per pixel
      Serial.print("Bit Depth: "); Serial.println(bmpDepth);
      if ((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed

        goodBmp = true; // Supported BMP format -- proceed!
        Serial.print("Image size: ");
        Serial.print(bmpWidth);
        Serial.print('x');
        Serial.println(bmpHeight);

        // BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (bmpWidth * 3 + 3) & ~3;

        // If bmpHeight is negative, image is in top-down order.
        // This is not canon but has been observed in the wild.
        if (bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip      = false;
        }

        // Crop area to be loaded
        w = bmpWidth;
        h = bmpHeight;
        if ((x + w - 1) >= tft.width())  w = tft.width()  - x;
        if ((y + h - 1) >= tft.height()) h = tft.height() - y;

        // Set TFT address window to clipped image bounds
        tft.setAddrWindow(x, y, x + w - 1, y + h - 1);

        for (row = 0; row < h; row++) { // For each scanline...

          // Seek to start of scan line.  It might seem labor-
          // intensive to be doing this on every line, but this
          // method covers a lot of gritty details like cropping
          // and scanline padding.  Also, the seek only takes
          // place if the file position actually needs to change
          // (avoids a lot of cluster math in SD library).
          if (flip) // Bitmap is stored bottom-to-top order (normal BMP)
            pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          else     // Bitmap is stored top-to-bottom
            pos = bmpImageoffset + row * rowSize;
          if (bmpFile.position() != pos) { // Need seek?
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer); // Force buffer reload
          }

          for (col = 0; col < w; col++) { // For each pixel...
            // Time to read more pixel data?
            if (buffidx >= sizeof(sdbuffer)) { // Indeed
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx = 0; // Set index to beginning
            }

            // Convert pixel from BMP to TFT format, push to display
            b = sdbuffer[buffidx++];
            g = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];
            tft.pushColor(tft.Color565(r, g, b));
          } // end pixel
        } // end scanline
        Serial.print("Loaded in ");
        Serial.print(millis() - startTime);
        Serial.println(" ms");
      } // end goodBmp
    }
  }

  bmpFile.close();
  if (!goodBmp) Serial.println("BMP format not recognized.");
}

// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t read16(File f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(File f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}
/*********************************************************************/
