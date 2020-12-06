/*
 *  Sink Water Heater
 *  Georges Troulis  <georgetroulis@gmail.com>
 *  
 *  Controls the water recirculation system that recirculates
 *  cold water into hot water to minimize water waste.
 * 
 *  When turned on, immediately starts a timer and displays
 *  water temperature.
 *  
 *  After 1 minute, play a warning beep. Play another warning
 *  one minute after that. After 3 minutes elapsed since turning
 *  on, beep indefinitely.
 *
 *  Device should be TURNED OFF after the indefinite beeping
 *  to avoid permanent damage to the water solenoid.
 *  
 *  ------------------------------------------------------------
 *  Revision Log (Updated 12/05/2020):
 *  0.3: Revised FSM to make 2 warning beeps at 1-min intervals
 *        before beeping indefinitely. Removed solenoid/button
 *  0.2: Added STM32 Support, Solenoid Hardware, button
 *  0.1: First Release
 *  
 *  ------------------------------------------------------------
 *  Items to fix:
 *   [ ] Add Radio Transmitter to control motor near thermostat
 */

// OLED Display Library
#include <Adafruit_SSD1306.h>

// DS18B20 Temperature Sensor
#include <OneWire.h>
#include <DallasTemperature.h>

/******************************************************************************/
/*  Hardware Platform                                                         */
/******************************************************************************/

//#define HW_STM32
#define HW_ARDUINO

/******************************************************************************/
/*  GPIO Defs                                                                 */
/******************************************************************************/

#if defined(HW_ARDUINO)
  #define PIN_TEMP_SENS     3
  #define PIN_BUZZER        6

#elif defined(HW_STM32)
  #define PIN_TEMP_SENS     PB5
  #define PIN_BUZZER        PB14

#else
  #error "Must define one hardware platform"
#endif


/******************************************************************************/
/*  Const Defs                                                                */
/******************************************************************************/

#define DISP_ADDR        0x3C

#define DISP_ROT         2

#define T_BEEP_1         60
#define T_BEEP_2         120
#define T_BEEP_3         180

#define OVERRIDE_SENSOR  0      // Set to 1 for testing w/o sensor connected

/******************************************************************************/
/*  Global Vars                                                               */
/******************************************************************************/

// DS18B20 Temperature Sensor
OneWire tempSensBus(PIN_TEMP_SENS);
DallasTemperature tempSens(&tempSensBus);

// OLED Display
Adafruit_SSD1306 disp(128, 64, &Wire, 4);

// Temperature in Celsius, fixed point
// Initial value is used for debug only when overriding the sensor reading
float tempC = 17.0;

enum prog_state_t {
  INIT,
  BEEP_1,
  BEEP_2,
  BEEP_3,
  BEEP_INDEF,
} prog_state = INIT;

unsigned long timeHeatingStarted = 0;

/******************************************************************************/
/*  Main Functions                                                            */
/******************************************************************************/

void setup() {

  // Enable default serial UART for USB debugging/printing
  Serial.begin(115200);

  // Initialize the OLED Display
  if(!disp.begin(SSD1306_SWITCHCAPVCC, DISP_ADDR)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  pinMode(PIN_BUZZER, OUTPUT);

  resetDisplay();
  
  #if (OVERRIDE_SENSOR == 0)
    // Enable the temperature sensor
    tempSens.begin();
  #endif

  prog_state = INIT;
}

void loop() {

  // Grab the temperature values from the sensor
  #if (OVERRIDE_SENSOR == 1)
    tempC += 0.02;
  #else
    tempSens.requestTemperatures();
    tempC = tempSens.getTempCByIndex(0);
  #endif
  
  // Always show the temperature
  showTemperature();

  switch(prog_state) {

    case INIT:
      timeHeatingStarted = millis() / 1000;
      prog_state = BEEP_1;
      break;

    // Wait for the BEEP_1 time interval to pass, then beep
    // and advance to the next waiting state
    case BEEP_1:
      if ((millis()/1000 - timeHeatingStarted) > T_BEEP_1) {
        prog_state = BEEP_2;
        beepOnce();
      }

      break;

    // Wait for the BEEP_2 time interval to pass, then beep
    // and advance to the next waiting state
    case BEEP_2:
      if ((millis()/1000 - timeHeatingStarted) > T_BEEP_2) {
        prog_state = BEEP_3;
        beepOnce();
      }
      break;

    // Wait for the final beep time interval to pass, then
    // advance to the "beep indefinitely" state
    case BEEP_3:
      if ((millis()/1000 - timeHeatingStarted) > T_BEEP_3) {
        prog_state = BEEP_INDEF;
        beepOnce();
      }
      break;

    // Beep indefinitely. Device should be TURNED OFF when this state
    // is reached to avoid permanent damage to water solenoid
    case BEEP_INDEF:
        beepOnce();
      break;
  }

  delay(100);
}

/******************************************************************************/
/*  I/O Helper Functions                                                      */
/******************************************************************************/

void beepOnce() {
  digitalWrite(PIN_BUZZER, HIGH);
  delay(200);
  digitalWrite(PIN_BUZZER, LOW);
  delay(600);
}

/******************************************************************************/
/*  OLED Display Helper Functions                                             */
/******************************************************************************/

/*
 * Shows the current temperature and setpoint
 */
void showTemperature() {
  char strBuf[64] = "XX.Y C";
  char str_temp[10];

  resetDisplay();

  // Temperature sensor returns -127 on error
  // (usually means sensor disconnected)
  if (tempC == -127.0) {
    disp.setTextSize(1);

    sprintf(strBuf, "Error: Temperature");
    dispCenterX(strBuf, 1);
    dispCenterY(strBuf, 1);
    disp.print(strBuf);

    sprintf(strBuf, "Sensor Disconnected");
    dispCenterX(strBuf, 1);
    disp.setCursor(disp.getCursorX(),
                   disp.getCursorY() + 10);
    disp.print(strBuf);
  }
  else if (tempC > 50) {
    // Sometimes sensor glitches and returns ultra high temperature
    // Keep the last temperature visible, do not update
    return;
  }
  else {

    #define TEMP_FONT_SIZE  2

    // Temperature string, XX.X C format
    dtostrf(tempC, 2, 1, str_temp);
    disp.setRotation(DISP_ROT);
    sprintf(strBuf, "%s C", str_temp);

    // Display the current temperature in nice large font
    disp.setTextSize(TEMP_FONT_SIZE);

    dispCenterX(strBuf, TEMP_FONT_SIZE);
    dispCenterY(strBuf, TEMP_FONT_SIZE);
    disp.setRotation(DISP_ROT);
    disp.print(strBuf);
  }

  // Display a pretty rectangle around the number
  disp.drawRect(0, 0, disp.width(), disp.height(), WHITE);

  // Flush buffer to screen so it can be displayed
  disp.display();
}

/*
 * Resets the display and sets some default settings
 */
void resetDisplay() {
  disp.clearDisplay();
  disp.setRotation(DISP_ROT);
  disp.setCursor(0, 0);
  disp.setTextSize(1);
  disp.setTextColor(WHITE);
}

/*
 * Centers the cursor for a string given a textSize
 *
 * Using 6x8 size font
 */
void dispCenterX(char* str, int textSize) {
  disp.setCursor((disp.width() - 6*textSize*strlen(str))/2,
                 disp.getCursorY());
}

/*
 * Centers the cursor for a string given a textSize
 *
 * Using 6x8 size font
 */
void dispCenterY(char* str, int textSize) {
  disp.setCursor(disp.getCursorX(),
                (disp.height() - 8*textSize)/2);
}
