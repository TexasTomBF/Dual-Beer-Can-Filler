// TTBF Dual Can Filler - Independent Dual - Improved
// Refactored from dual_can_filler_arduino_independent_dual.ino (v1.02)
//
// Changes from v1.02:
//   - Fully non-blocking: removed remaining delay() in stop_purge functions,
//     replaced with SETTLING state tracked via millis()
//   - unsigned long for all millis() timestamps (overflow safe)
//   - Fixed duplicate digitalWrite in fillLevelReached functions
//   - Fixed set_fillevel_2() silent failure (matched set_fillevel_1 behaviour)
//   - DEBUG compile-time flag wrapping all Serial output
//   - Typed const variables replacing #define
//   - const pin assignments
//   - bool instead of boolean
//   - Named calibration constants
//   - Consolidated clearDisplayHalf() helper
//   - Fixed Fil1Level_reached naming typo -> fillLevelReached

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

//This code is fully functional and refactored for improved maintainability and non-blocking operation. Can be used with any Arduino model like the Nano or UNO.
//Currently its supporting 2 momentary push buttons and one I2C display, the display is showing purging and dispense status for both filler lines.
//The two buttons have same functions for each fill line:
//-- SHORT PRESS starts whole sequence with purging and filling corresponding beer line.
//-- At first filling both buttons needs to be SHORT PRESSED again for the corresponding line when desired fill level is reached. The desired fill level is then stored for future fills (until reset or power down).
//-- the next fill will stop at the programmed fill level, the display will indicate fill level reached with an '*' next to the fill level measurement.
//-- After filling a LONG PRESS of any of the buttons resets the fill level for the corresponding beer line, indicated with a capital 'R' in the display for corresponding beer line. This allows to correct/reset fill level when changing can size etc. The new level must then be set on next filling (like at power up).
//
//The display itself is purely cosmetic in one sense, the filler works perfectly without it if you desire to leave it out.
//Displayed fill level progress is in 'milliliters' and must be calibrated for each system before first time use. Purely cosmetic, no practical implications if not calibrated. Can easily be changed to other units if desired, or removed completely.
//
//Differential pressure sensors are used for level sensing using the same fill tube as for co2 purging. Separate fill tubes for beer. 12V Solenoid valves for water/beer and three way 12V solenoid valves for co2/level sensing. Full parts list on GitHub also.

// Debug flag - set to 1 to enable serial debug output
#define DEBUG 0

#if DEBUG
  #define DBG_PRINT(x)     Serial.print(x)
  #define DBG_PRINTLN(x)   Serial.println(x)
  #define DBG_PRINTLN_F(x) Serial.println(F(x))
#else
  #define DBG_PRINT(x)
  #define DBG_PRINTLN(x)
  #define DBG_PRINTLN_F(x)
#endif

// Pin assignments
const byte PIN_CO2_1        = 8;
const byte PIN_CO2_2        = 7;
const byte PIN_SWITCH_1     = 5;    // Button for 1st beer line
const byte PIN_SWITCH_2     = 3;    // Button for 2nd beer line
const byte PIN_BEER_1       = 9;    // Beer1
const byte PIN_BEER_2       = 10;
const int  PIN_PRESSURE_1   = A0;   // Levelsensor
const int  PIN_PRESSURE_2   = A1;   // Levelsensor

// Timing constants
const unsigned long DEBOUNCE_MS      = 20;
const unsigned long HOLD_TIME_MS     = 2000;
const unsigned long UPDATE_DELAY_MS  = 1000;
const unsigned long PURGE_PERIOD_MS  = 7000;
const unsigned long SETTLE_TIME_1_MS = 300;
const unsigned long SETTLE_TIME_2_MS = 500;

// Calibration constants
const int   SENSOR_OFFSET_1      = 520;
const int   SENSOR_OFFSET_2      = 555;
const int   SENSOR_ADJUST_1      = 117;
const float SENSOR_ADJUST_2      = 80.2f;
const float ML_PER_UNIT_1        = 1.60f;
const float ML_PER_UNIT_2        = 1.55f;
const int   DISPLAY_MIN_LEVEL_1  = 120;
const int   DISPLAY_MIN_LEVEL_2  = 115;

// Fill level variables
int FillThreshold_1 = 900;
int FillThreshold_2 = 900;
int FillLevel_1;
int FillLevel_2;
int FillLevel_1_round;
int FillLevel_2_round;

// Valve state variables
bool valveStateCO2_1 = LOW;
bool valveStateCO2_2 = LOW;
bool valveStateBeer_1 = LOW;
bool valveStateBeer_2 = LOW;

// Button state variables
int buttonstate1 = LOW;
int buttonstate2 = LOW;
unsigned long lastupdate;
unsigned long btnDnTime1;
unsigned long btnDnTime2;
unsigned long btnUpTime1;
unsigned long btnUpTime2;
unsigned long purge_start_time1;
unsigned long purge_start_time2;
unsigned long settleStartTime1;
unsigned long settleStartTime2;
bool ignoreUp1 = false;
bool ignoreUp2 = false;
bool start_co2_1_flag = false;
bool start_co2_2_flag = false;
bool start_dispense_1 = false;
bool start_dispense_2 = false;
bool purge_complete_1 = false;
bool purge_complete_2 = false;
bool settling_1 = false;
bool settling_2 = false;
int buttonVal1 = 0;
int buttonVal2 = 0;
int buttonLast1 = 0;
int buttonLast2 = 0;

LiquidCrystal_I2C lcd(0x27, 20, 4);  // set the LCD address to 0x27 for a 20 chars and 4 line display

void setup()
{
  pinMode(PIN_CO2_1, OUTPUT);
  pinMode(PIN_CO2_2, OUTPUT);
  pinMode(PIN_BEER_1, OUTPUT);
  pinMode(PIN_BEER_2, OUTPUT);
  pinMode(PIN_SWITCH_1, INPUT);
  pinMode(PIN_SWITCH_2, INPUT);
  pinMode(PIN_PRESSURE_1, INPUT);
  pinMode(PIN_PRESSURE_2, INPUT);

  Serial.begin(9600);

  lcd.init();        // initialize the lcd
  lcd.backlight();

  lcd.setCursor(1, 0);
  lcd.print("DualCanFiller2000");
}

////////Main Loop//////
void loop()
{
  buttonVal1 = digitalRead(PIN_SWITCH_1);
  buttonVal2 = digitalRead(PIN_SWITCH_2);
  UpdateDisplay();

  // Test for button pressed and store the down time
  if (buttonVal2 == HIGH && buttonLast2 == LOW && (millis() - btnUpTime2) > DEBOUNCE_MS)
  {
    btnDnTime2 = millis();
  }

  if (buttonVal1 == HIGH && buttonLast1 == LOW && (millis() - btnUpTime1) > DEBOUNCE_MS)
  {
    btnDnTime1 = millis();
  }

  // Test for button release and store the up time, initiate short press actions
  if (buttonVal1 == LOW && buttonLast1 == HIGH && (millis() - btnDnTime1) > DEBOUNCE_MS)
  {
    if (ignoreUp1 == false) {
      if (start_co2_1_flag == true) {
        valveStateBeer_1 = LOW;
        valveStateCO2_1 = LOW;
        digitalWrite(PIN_CO2_1, LOW);
        digitalWrite(PIN_BEER_1, LOW);
        lcd.setCursor(0, 2);
        lcd.print("          ");
        lcd.setCursor(1, 2);
        lcd.print("Aborted");
        start_co2_1_flag = false;
        start_dispense_1 = false;
        settling_1 = false;
      }
      else if (start_dispense_1 == true) {
        clearDisplayHalf(0);
        set_fillevel_1();
      }
      else {
        clearDisplayHalf(0);
        eventPress1();
      }
    }
    else ignoreUp1 = false;
    btnUpTime1 = millis();
  }

  // Test for intended button press and perform actions accordingly
  if (buttonVal2 == LOW && buttonLast2 == HIGH && (millis() - btnDnTime2) > DEBOUNCE_MS)
  {
    if (ignoreUp2 == false) {
      if (start_co2_2_flag == true) {
        valveStateBeer_2 = LOW;
        valveStateCO2_2 = LOW;
        digitalWrite(PIN_CO2_2, LOW);
        digitalWrite(PIN_BEER_2, LOW);
        lcd.setCursor(10, 2);
        lcd.print("          ");
        lcd.setCursor(11, 2);
        lcd.print("Aborted");
        start_co2_2_flag = false;
        start_dispense_2 = false;
        settling_2 = false;
      }
      else if (start_dispense_2 == true) {
        clearDisplayHalf(10);
        set_fillevel_2();
      }
      else {
        clearDisplayHalf(10);
        eventPress2();
      }
    }
    else ignoreUp2 = false;
    btnUpTime2 = millis();
  }

  // Test for button held down for longer than the hold time
  if (buttonVal1 == HIGH && (millis() - btnDnTime1) > HOLD_TIME_MS)
  {
    eventHold1();
    ignoreUp1 = true;
    btnDnTime1 = millis();
  }

  if (buttonVal2 == HIGH && (millis() - btnDnTime2) > HOLD_TIME_MS)
  {
    eventHold2();
    ignoreUp2 = true;
    btnDnTime2 = millis();
  }

  // Test if purge period is complete and stop purge
  if (start_co2_1_flag == true && purge_complete_1 == false && (millis() - purge_start_time1) > PURGE_PERIOD_MS) {
    stop_purge1();
  }

  if (start_co2_2_flag == true && purge_complete_2 == false && (millis() - purge_start_time2) > PURGE_PERIOD_MS) {
    stop_purge2();
  }

  // Check if settling period is complete and start beer flow
  if (settling_1 == true && (millis() - settleStartTime1) > SETTLE_TIME_1_MS) {
    settling_1 = false;
    valveStateBeer_1 = HIGH;
    digitalWrite(PIN_BEER_1, valveStateBeer_1);
    lcd.setCursor(0, 2);
    lcd.print("          ");
    lcd.setCursor(1, 2);
    lcd.print("Filling");
    start_dispense_1 = true;
  }

  if (settling_2 == true && (millis() - settleStartTime2) > SETTLE_TIME_2_MS) {
    settling_2 = false;
    valveStateBeer_2 = HIGH;
    digitalWrite(PIN_BEER_2, valveStateBeer_2);
    lcd.setCursor(10, 2);
    lcd.print("          ");
    lcd.setCursor(11, 2);
    lcd.print("Filling");
    start_dispense_2 = true;
  }

  buttonLast1 = buttonVal1;
  buttonLast2 = buttonVal2;

  // Regardless if button pushed, always check if fill level is reached
  if ((analogRead(PIN_PRESSURE_1)) > (FillThreshold_1) && (valveStateBeer_1 == HIGH))
  {
    fillLevelReached1();
  }

  if ((analogRead(PIN_PRESSURE_2)) > (FillThreshold_2) && (valveStateBeer_2 == HIGH))
  {
    fillLevelReached2();
  }
}
/////////////////////////////////////


///////////Function area////////////////////////
void UpdateDisplay()
{
  if ((millis() - lastupdate) > UPDATE_DELAY_MS)
  {
    lastupdate = millis();
    
    DBG_PRINTLN_F("display update done");
    FillLevel_2 = analogRead(PIN_PRESSURE_2);
    FillLevel_1 = analogRead(PIN_PRESSURE_1);
    FillLevel_2 -= SENSOR_OFFSET_2;
    FillLevel_1 -= SENSOR_OFFSET_1;
    
    FillLevel_1_round = int((FillLevel_1 + SENSOR_ADJUST_1) * ML_PER_UNIT_1);
    FillLevel_2_round = int((FillLevel_2 + SENSOR_ADJUST_2) * ML_PER_UNIT_2);
    DBG_PRINTLN(FillLevel_1_round);
    DBG_PRINTLN(FillLevel_2_round);
    DBG_PRINTLN(FillLevel_1);
    DBG_PRINTLN(FillLevel_2);
    
    if (FillLevel_1_round >= DISPLAY_MIN_LEVEL_1 || FillLevel_2_round >= DISPLAY_MIN_LEVEL_2)
    {
      lcd.setCursor(0, 3);
      lcd.print("L1: ");
      lcd.setCursor(3, 3);
      lcd.print(FillLevel_1_round);
      lcd.print("ml");
      lcd.setCursor(10, 3);
      lcd.print("L2: ");
      lcd.setCursor(13, 3);
      lcd.print("   ");
      lcd.setCursor(13, 3);
      lcd.print(FillLevel_2_round);
      lcd.setCursor(16, 3);
      lcd.print("ml");
    }
  }
}

void set_fillevel_1()
{
  if ((valveStateCO2_1 == LOW) && (valveStateBeer_1 == HIGH))  // if beer is flowing and button pushed,
  {
    FillLevel_1 = analogRead(PIN_PRESSURE_1);
    FillThreshold_1 = FillLevel_1;  // update fill level threshold(calibrate)
    DBG_PRINTLN(FillThreshold_1);
    valveStateBeer_1 = LOW;  // stop beer flow
    digitalWrite(PIN_BEER_1, valveStateBeer_1);
  }

  lcd.setCursor(1, 2);
  lcd.print("LevelSet");

  start_dispense_1 = false;
}

void set_fillevel_2()
{
  if ((valveStateCO2_2 == LOW) && (valveStateBeer_2 == HIGH))  // if beer is flowing and button pushed,
  {
    FillLevel_2 = analogRead(PIN_PRESSURE_2);
    FillThreshold_2 = FillLevel_2;  // update fill level threshold(calibrate)
    DBG_PRINTLN(FillThreshold_2);

    valveStateBeer_2 = LOW;  // stop beer flow
    digitalWrite(PIN_BEER_2, valveStateBeer_2);
  }
  
  lcd.setCursor(11, 2);
  lcd.print("LevelSet");

  start_dispense_2 = false;
}

void stop_purge1()
{
  valveStateCO2_1 = LOW;
  digitalWrite(PIN_CO2_1, valveStateCO2_1);  // stop Co2 flow

  DBG_PRINTLN_F("CO2_1 purge complete");
  
  // Start settling period instead of delay
  settling_1 = true;
  settleStartTime1 = millis();
  
  purge_complete_1 = true;
  start_co2_1_flag = false;
}

void stop_purge2()
{
  valveStateCO2_2 = LOW;  // stop Co2 flow
  digitalWrite(PIN_CO2_2, valveStateCO2_2);

  DBG_PRINTLN_F("CO2_2 purge complete");
  
  // Start settling period instead of delay
  settling_2 = true;
  settleStartTime2 = millis();
  
  purge_complete_2 = true;
  start_co2_2_flag = false;
}

void eventPress1()
{
  if ((valveStateCO2_1 == LOW) && (valveStateBeer_1 == LOW))  // button has been pushed to initate filling, and no valves are open
  {
    valveStateCO2_1 = HIGH;
    // start CO2 purge
    digitalWrite(PIN_CO2_1, valveStateCO2_1);

    lcd.setCursor(0, 2);
    lcd.print("Purging..");

    purge_complete_1 = false;
    start_co2_1_flag = true;
    purge_start_time1 = millis();
  }
}

void eventPress2()
{
  DBG_PRINTLN_F("Purge should start 2");
  if ((valveStateCO2_2 == LOW) && (valveStateBeer_2 == LOW))  // button has been pushed to initate filling, and no valves are open
  {
    valveStateCO2_2 = HIGH;  // start CO2 purge

    digitalWrite(PIN_CO2_2, valveStateCO2_2);

    lcd.setCursor(10, 2);
    lcd.print("Purging..");

    purge_complete_2 = false;
    start_co2_2_flag = true;
    purge_start_time2 = millis();
  }
}

void eventHold1()
{
  DBG_PRINTLN_F("Reset threshold 1...");
  lcd.setCursor(8, 3);
  lcd.print("R");
  FillThreshold_1 = 1000;
}

void eventHold2()
{
  DBG_PRINTLN_F("Reset threshold 2...");
  lcd.setCursor(18, 3);
  lcd.print("R");
  FillThreshold_2 = 1000;
}

void fillLevelReached2()
{
  valveStateBeer_2 = LOW;
  digitalWrite(PIN_BEER_2, LOW);
  DBG_PRINTLN(FillThreshold_2);
  lcd.setCursor(19, 3);
  lcd.print("*");
  lcd.setCursor(10, 2);
  lcd.print("          ");
  lcd.setCursor(10, 2);
  lcd.print("Finished");

  start_dispense_2 = false;
}

void fillLevelReached1()
{
  valveStateBeer_1 = LOW;
  digitalWrite(PIN_BEER_1, LOW);
  lcd.setCursor(9, 3);
  lcd.print("*");
  lcd.setCursor(0, 2);
  lcd.print("          ");
  lcd.setCursor(0, 2);
  lcd.print("Finished");

  start_dispense_1 = false;
}

void clearDisplayHalf(int startCol)
{
  lcd.setCursor(startCol, 2);
  lcd.print("          ");
  lcd.setCursor(startCol, 3);
  lcd.print("          ");
  lcd.setCursor(startCol, 1);
  lcd.print("----------");
}
