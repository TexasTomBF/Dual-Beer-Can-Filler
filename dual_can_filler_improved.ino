// TTBF Dual Can Filler - Improved Version
// Refactored from dual_can_filler_arduino_rewamped.ino (v0.2)
// Changes: non-blocking state machine, struct-based line handling,
//          fixed Button 2 logic, type-safe constants, debug flag,
//          overflow-safe millis() usage, LCD helper, naming fixes.

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ============================================================================
// DEBUG FLAG
// ============================================================================
#define DEBUG 1
#if DEBUG
  #define DBG_PRINT(x)   Serial.print(x)
  #define DBG_PRINTLN(x) Serial.println(x)
#else
  #define DBG_PRINT(x)
  #define DBG_PRINTLN(x)
#endif

// ============================================================================
// CONSTANTS - Pin Assignments
// ============================================================================
const byte PIN_CO2_1  = 8;
const byte PIN_CO2_2  = 7;
const byte PIN_SWITCH_1 = 5;      // Button for 1st beer line
const byte PIN_SWITCH_2 = 3;      // Button for 2nd beer line
const byte PIN_BEER_1 = 9;        // Beer valve 1
const byte PIN_BEER_2 = 10;       // Beer valve 2
const int  PIN_PRESSURE_1 = A0;   // Level sensor 1
const int  PIN_PRESSURE_2 = A1;   // Level sensor 2

// ============================================================================
// CONSTANTS - Timing
// ============================================================================
const unsigned long DEBOUNCE_MS     = 20;
const unsigned long HOLD_TIME_MS    = 2000;
const unsigned long UPDATE_DELAY_MS = 1000;
const unsigned long PURGE_TIME_MS   = 7000;
const unsigned long VALVE_STAGGER_MS = 500;
const unsigned long SETTLE_TIME_MS  = 300;

// ============================================================================
// CONSTANTS - Calibration
// ============================================================================
const int   SENSOR_OFFSET_1    = 443;
const int   SENSOR_OFFSET_2    = 539;
const float ML_PER_ANALOG_UNIT = 2.56f;
const int   DEFAULT_THRESHOLD  = 900;

// ============================================================================
// STATE MACHINE STATES
// ============================================================================
enum State : uint8_t {
  STATE_IDLE = 0,
  STATE_PURGING,
  STATE_PURGE_SETTLING,
  STATE_DISPENSING,
  STATE_FINISHED
};

// ============================================================================
// BEER LINE STRUCTURE
// ============================================================================
struct BeerLine {
  const byte pinCO2;
  const byte pinBeer;
  const byte pinSwitch;
  const int  pinPressure;
  int        fillThreshold;
  bool       valveStateCO2;
  bool       valveStateBeer;
  unsigned long btnDnTime;
  unsigned long btnUpTime;
  bool       ignoreUp;
  int        buttonVal;
  int        buttonLast;
  // State machine fields:
  uint8_t    state;          // Current state
  unsigned long stateStart;  // millis() when current state began
  int        sensorOffset;   // Calibration offset
  int        lcdCol;         // LCD column for this line's status
  int        lcdFillCol;     // LCD column for this line's fill level
  unsigned long co2Stagger;  // Stagger delay for CO2 valve opening
  unsigned long beerStagger; // Stagger delay for beer valve opening
  bool       co2Opened;      // Track if CO2 valve has been opened (for staggering)
};

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================
BeerLine line1 = {
  PIN_CO2_1, PIN_BEER_1, PIN_SWITCH_1, PIN_PRESSURE_1,
  DEFAULT_THRESHOLD, false, false, 0, 0, false, 0, 0,
  STATE_IDLE, 0, SENSOR_OFFSET_1, 0, 3, 0, 0, false
};

BeerLine line2 = {
  PIN_CO2_2, PIN_BEER_2, PIN_SWITCH_2, PIN_PRESSURE_2,
  DEFAULT_THRESHOLD, false, false, 0, 0, false, 0, 0,
  STATE_IDLE, 0, SENSOR_OFFSET_2, 10, 13, VALVE_STAGGER_MS, VALVE_STAGGER_MS, false
};

unsigned long lastUpdate = 0;
LiquidCrystal_I2C lcd(0x27, 20, 4);

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================
void clearLcdRegion(int col, int row, int width);
void updateDisplay();
void updateStateMachine(BeerLine& line);
void handleButtonPress(BeerLine& line);
void handleButtonHold(BeerLine& line);
void fillLevelReached(BeerLine& line, int lcdCol);
void startFillSequence();
void stopLine(BeerLine& line, int lcdCol);

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  pinMode(PIN_CO2_1, OUTPUT);
  pinMode(PIN_CO2_2, OUTPUT);
  pinMode(PIN_BEER_1, OUTPUT);
  pinMode(PIN_BEER_2, OUTPUT);
  pinMode(PIN_SWITCH_1, INPUT);
  pinMode(PIN_SWITCH_2, INPUT);
  pinMode(PIN_PRESSURE_1, INPUT);
  pinMode(PIN_PRESSURE_2, INPUT);

  Serial.begin(9600);

  lcd.init();
  lcd.backlight();

  lcd.setCursor(1, 0);
  lcd.print("BeerEjaculator2000");
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
  // Read button states
  line1.buttonVal = digitalRead(PIN_SWITCH_1);
  line2.buttonVal = digitalRead(PIN_SWITCH_2);
  
  updateDisplay();

  // Process button events for line 1
  // Test for button pressed and store the down time
  if (line1.buttonVal == HIGH && line1.buttonLast == LOW && 
      (millis() - line1.btnUpTime) > DEBOUNCE_MS) {
    DBG_PRINTLN("pressed1...");
    line1.btnDnTime = millis();
  }

  // Test for button release and initiate short press actions
  if (line1.buttonVal == LOW && line1.buttonLast == HIGH && 
      (millis() - line1.btnDnTime) > DEBOUNCE_MS) {
    DBG_PRINTLN("initiate.");
    DBG_PRINTLN(line1.ignoreUp);
    if (line1.ignoreUp == false) {
      handleButtonPress(line1);
    } else {
      line1.ignoreUp = false;
    }
    line1.btnUpTime = millis();
  }

  // Test for button held down for longer than the hold time
  if (line1.buttonVal == HIGH && (millis() - line1.btnDnTime) > HOLD_TIME_MS) {
    DBG_PRINTLN("phoooold1");
    handleButtonHold(line1);
    line1.ignoreUp = true;
    line1.btnDnTime = millis();
  }

  // Process button events for line 2
  if (line2.buttonVal == HIGH && line2.buttonLast == LOW && 
      (millis() - line2.btnUpTime) > DEBOUNCE_MS) {
    DBG_PRINTLN("pressed2...");
    line2.btnDnTime = millis();
  }

  if (line2.buttonVal == LOW && line2.buttonLast == HIGH && 
      (millis() - line2.btnDnTime) > DEBOUNCE_MS) {
    if (line2.ignoreUp == false) {
      handleButtonPress(line2);
    } else {
      line2.ignoreUp = false;
    }
    line2.btnUpTime = millis();
  }

  if (line2.buttonVal == HIGH && (millis() - line2.btnDnTime) > HOLD_TIME_MS) {
    handleButtonHold(line2);
    line2.ignoreUp = true;
    line2.btnDnTime = millis();
  }

  line1.buttonLast = line1.buttonVal;
  line2.buttonLast = line2.buttonVal;

  // Update state machines
  updateStateMachine(line1);
  updateStateMachine(line2);

  // Check fill levels (regardless of button state)
  if ((analogRead(PIN_PRESSURE_1)) > line1.fillThreshold && 
      line1.valveStateBeer == HIGH) {
    fillLevelReached(line1, 0);
  }

  if ((analogRead(PIN_PRESSURE_2)) > line2.fillThreshold && 
      line2.valveStateBeer == HIGH) {
    fillLevelReached(line2, 10);
  }
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

void clearLcdRegion(int col, int row, int width) {
  lcd.setCursor(col, row);
  for (int i = 0; i < width; i++) {
    lcd.print(" ");
  }
}

void updateDisplay() {
  if ((millis() - lastUpdate) > UPDATE_DELAY_MS) {
    lastUpdate = millis();
    
    int fillLevel1Raw = analogRead(PIN_PRESSURE_1);
    int fillLevel2Raw = analogRead(PIN_PRESSURE_2);
    int fillLevel1Ml = int((fillLevel1Raw - SENSOR_OFFSET_1) * ML_PER_ANALOG_UNIT);
    int fillLevel2Ml = int((fillLevel2Raw - SENSOR_OFFSET_2) * ML_PER_ANALOG_UNIT);
    
    if (fillLevel1Ml >= 0 && fillLevel2Ml >= 0) {
      lcd.setCursor(0, 3);
      lcd.print("L1: ");
      lcd.setCursor(3, 3);
      lcd.print(fillLevel1Ml);
      lcd.print("ml");
      
      lcd.setCursor(10, 3);
      lcd.print("L2: ");
      clearLcdRegion(13, 3, 3);
      lcd.setCursor(13, 3);
      lcd.print(fillLevel2Ml);
      lcd.setCursor(16, 3);
      lcd.print("ml");
    }
  }
}

void updateStateMachine(BeerLine& line) {
  unsigned long elapsed = millis() - line.stateStart;
  
  switch (line.state) {
    case STATE_IDLE:
      // Nothing to do in IDLE
      break;
      
    case STATE_PURGING:
      // Handle CO2 valve staggered opening
      if (!line.co2Opened && elapsed >= line.co2Stagger) {
        digitalWrite(line.pinCO2, HIGH);
        line.co2Opened = true;
      }
      
      // Check if purge time is complete
      if (elapsed >= PURGE_TIME_MS) {
        // Purge complete, stop CO2
        line.valveStateCO2 = false;
        digitalWrite(line.pinCO2, LOW);
        DBG_PRINTLN("CO2 purge complete");
        
        // Transition to settling state
        line.state = STATE_PURGE_SETTLING;
        line.stateStart = millis();
      }
      break;
      
    case STATE_PURGE_SETTLING:
      if (elapsed >= SETTLE_TIME_MS + line.beerStagger) {
        // Settling complete, start beer flow
        line.valveStateBeer = true;
        digitalWrite(line.pinBeer, HIGH);
        
        // Transition to dispensing state
        line.state = STATE_DISPENSING;
        line.stateStart = millis();
        
        // Update display - if both lines are now dispensing, show centered message
        if (line1.state == STATE_DISPENSING && line2.state == STATE_DISPENSING) {
          clearLcdRegion(0, 2, 20);
          lcd.setCursor(5, 2);
          lcd.print("Dispensing");
        }
      }
      break;
      
    case STATE_DISPENSING:
      // Wait for fill level to be reached or button press
      break;
      
    case STATE_FINISHED:
      // Nothing to do in FINISHED
      break;
  }
}

void handleButtonPress(BeerLine& line) {
  clearLcdRegion(0, 2, 20);
  clearLcdRegion(0, 3, 20);
  
  lcd.setCursor(0, 1);
  lcd.print("--------------------");
  
  // Check if this is button 1 or button 2
  bool isLine1 = (line.pinSwitch == PIN_SWITCH_1);
  
  if (line.state == STATE_IDLE) {
    // Button pressed to initiate filling sequence
    // Start the fill sequence for BOTH lines
    startFillSequence();
  } else if (line.state == STATE_DISPENSING) {
    // Button pressed during dispensing - set fill threshold
    line.fillThreshold = analogRead(line.pinPressure);
    stopLine(line, line.lcdCol);
  } else {
    // Button pressed during purging or other state - stop everything
    stopLine(line, line.lcdCol);
  }
}

void handleButtonHold(BeerLine& line) {
  // Reset fill threshold
  DBG_PRINTLN("Reset threshold...");
  lcd.setCursor((line.pinSwitch == PIN_SWITCH_1) ? 8 : 19, 3);
  lcd.print("R");
  line.fillThreshold = 1000;
}

void startFillSequence() {
  // Clear display
  clearLcdRegion(0, 2, 20);
  
  // Start CO2 purge for both lines
  line1.valveStateCO2 = true;
  line2.valveStateCO2 = true;
  
  // Reset CO2 opened flags
  line1.co2Opened = false;
  line2.co2Opened = false;
  
  // Update display
  lcd.setCursor(0, 2);
  lcd.print("Purging..");
  lcd.setCursor(10, 2);
  lcd.print("Purging..");
  
  // Set both lines to PURGING state at the same time
  unsigned long now = millis();
  line1.state = STATE_PURGING;
  line1.stateStart = now;
  line2.state = STATE_PURGING;
  line2.stateStart = now;
  
  // CO2 and beer valve staggers are handled by co2Stagger and beerStagger fields
  // in the state machine
}

void stopLine(BeerLine& line, int lcdCol) {
  // Stop all valves for this line
  line.valveStateBeer = false;
  line.valveStateCO2 = false;
  digitalWrite(line.pinBeer, LOW);
  digitalWrite(line.pinCO2, LOW);
  
  // Update display
  clearLcdRegion(lcdCol, 2, 10);
  lcd.setCursor(lcdCol, 2);
  lcd.print("Finished");
  
  // Set state to FINISHED
  line.state = STATE_FINISHED;
  line.stateStart = millis();
}

void fillLevelReached(BeerLine& line, int lcdCol) {
  DBG_PRINT("Measured fill level: ");
  DBG_PRINTLN(analogRead(line.pinPressure));
  DBG_PRINT("Calibrated fill threshold: ");
  DBG_PRINTLN(line.fillThreshold);
  
  // Stop beer flow
  line.valveStateBeer = false;
  digitalWrite(line.pinBeer, LOW);
  
  DBG_PRINTLN("Beer level reached");
  
  // Update display
  lcd.setCursor((lcdCol == 0) ? 9 : 19, 3);
  lcd.print("*");
  
  clearLcdRegion(lcdCol, 2, 10);
  lcd.setCursor(lcdCol, 2);
  lcd.print("Finished");
  
  // Set state to FINISHED
  line.state = STATE_FINISHED;
  line.stateStart = millis();
}
