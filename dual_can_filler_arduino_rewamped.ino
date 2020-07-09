#include <Wire.h>
#include <LiquidCrystal_I2C.h>

//  TTBF Dual Can Filler v0.2

//This code is 100% functional but could be tidied and streamlined a lot more (work in progress). Could also be optimized with more fuctions etc etc. CAn be used with any Arduino model like the Nano or UNO.
//Currently its supporting 2 push buttons and one I2C display, the display is showing purgning and dispense status for both filler lines.
//The two buttons fuctions as follows:
//Button 1: short press starts whole sequence with purging and filling for both lines. At first filling both buttons needs to be pushed briefly again for the corresponding line when desired fill level is reached. 
//This fill level is stored until reset or power down.
//After filling a long press of any of the buttons resets the fill level for the corresponing beer line, indicated with a captial 'R' in the display.

//Displayed fill level progress is in 'mililiters' and must be calibrated for each system before first time use. Purely cosmetical, no practical implications if not calibrated.

byte ValveCO2_1 = 8;
byte ValveCO2_2 = 7;
byte Switch_1 = 5;      //Button for 1st beer line
byte Switch_2 = 3;    //Button for 2nd beer line
byte ValveBeer_1 = 9;   //Beer1
byte ValveBeer_2 = 10;
int FillLevelPressure_1 = A0;     //Levelsensor
int FillLevelPressure_2 = A1;     //Levelsensor


int FillThreshold_1 = 900;
int FillThreshold_2 = 900;
float FillLevel_1;
float FillLevel_2;
int FillLevel_1_round;
int FillLevel_2_round;


// variable to hold the valve state
boolean valveStateCO2_1 = LOW;
boolean valveStateCO2_2 = LOW;
boolean valveStateBeer_1 = LOW;
boolean valveStateBeer_2 = LOW;



// used for debounce and button modes(short press, long press etc)
int buttonstate1 = LOW;
int buttonstate2 = LOW;
long lastupdate;
long btnDnTime1;
long btnDnTime2;
long btnUpTime1;
long btnUpTime2;
boolean ignoreUp1 = false;
boolean ignoreUp2 = false;
int buttonVal1 = 0;
int buttonVal2 = 0;
int buttonLast1 = 0;
int buttonLast2 = 0;

#define debounce 20
#define holdTime 2000
#define updateDelay 1000

LiquidCrystal_I2C lcd(0x27, 20, 4);                             // set the LCD address to 0x27 for a 20 chars and 4 line display

void setup()
{
  pinMode(ValveCO2_1, OUTPUT);
  pinMode(ValveCO2_2, OUTPUT);
  pinMode(ValveBeer_1, OUTPUT);
  pinMode(ValveBeer_2, OUTPUT);
  pinMode(Switch_1, INPUT);
  pinMode(Switch_2, INPUT);
  pinMode (FillLevelPressure_1, INPUT);
  pinMode (FillLevelPressure_2, INPUT);

  Serial.begin(9600);

  lcd.init();                                                   // initialize the lcd
  lcd.init();
  lcd.backlight();
  // Print a message to the LCD.


  lcd.setCursor(1, 0);
  lcd.print("BeerEjaculator2000");

}


void loop()
{
  buttonVal1 = digitalRead(Switch_1);
  buttonVal2 = digitalRead(Switch_2);
  UpdateDisplay ();


  //Test for button pressed and store the down time
  if ( buttonVal2 == HIGH && buttonLast2 == LOW && (millis() - btnUpTime2) > long(debounce))
  {
    Serial.println("pressed2...");
    btnDnTime2 = millis();
  }

  if ( buttonVal1 == HIGH && buttonLast1 == LOW && (millis() - btnUpTime1) > long(debounce))
  {
    Serial.println("pressed1...");
    btnDnTime1 = millis();
  }

  //Test for button release and store the up time, initiate short press actions
  if ( buttonVal2 == LOW && buttonLast2 == HIGH && (millis() - btnDnTime2) > long(debounce))
  {
    if (ignoreUp2 == false)eventPress2();

    else ignoreUp2 = false;
    btnUpTime2 = millis();
  }

  if ( buttonVal1 == LOW && buttonLast1 == HIGH && (millis() - btnDnTime1) > long(debounce))
  {
    Serial.println("initiate.");
    Serial.println(ignoreUp1);
    if (ignoreUp1 == false) {
      eventPress1();
    }
    else ignoreUp1 = false;
    btnUpTime1 = millis();
  }

  //Test for button held down for longer than the hold time

  if (buttonVal1 == HIGH && (millis() - btnDnTime1) > long(holdTime))
  {
    Serial.println("phoooold1");
    eventHold1();
    ignoreUp1 = true;
    btnDnTime1 = millis();
  }

  if (buttonVal2 == HIGH && (millis() - btnDnTime2) > long(holdTime))
  {
    eventHold2();
    ignoreUp2 = true;
    btnDnTime2 = millis();
  }


  buttonLast1 = buttonVal1;
  buttonLast2 = buttonVal2;

  if ((analogRead (FillLevelPressure_1)) > (FillThreshold_1) &&  (valveStateBeer_1 == HIGH) )  //regardless if button pushed, always check if filllevel is reached
  {
    Fil1Level_reached1 ();
  }

  if ((analogRead (FillLevelPressure_2)) > (FillThreshold_2) &&  (valveStateBeer_2 == HIGH) )  //regardless if button pushed, always check if filllevel is reached
  {
    Fil1Level_reached2 ();
  }
}

void eventPress1()
{
  lcd.setCursor(0, 2);
  lcd.print("                    ");
  lcd.setCursor(0, 3);
  lcd.print("                    ");


 // Serial.println(FillLevel_2);
  lcd.setCursor(0, 1);
  lcd.print("--------------------");

  if ((valveStateCO2_1 == LOW) && (valveStateBeer_1 == LOW))             //button has been pushed to initate filling, and no valves are open
  {
    valveStateCO2_1 = HIGH;
    valveStateCO2_2 = HIGH;                                               //start CO2 purge
    digitalWrite(ValveCO2_1, valveStateCO2_1);
    delay(500);
    digitalWrite(ValveCO2_2, valveStateCO2_2);

    //Serial.println("Purging 1...");
    lcd.setCursor(0, 2);
    lcd.print("Purging..");
    //Serial.println("Purging 2...");
    lcd.setCursor(10, 2);
    lcd.print("Purging..");
    //start CO2 purge




    delay(7000);                                                         //purge for 8 seconds, want to replace this with millis()
    valveStateCO2_1 = LOW;
    valveStateCO2_2 = LOW; //stop Co2 flow
    digitalWrite(ValveCO2_1, valveStateCO2_1);                                                        //stop Co2 flow
    digitalWrite(ValveCO2_2, valveStateCO2_2);
    Serial.println("CO2_1 purge complete");

    delay(300);                                                          //give CO2 solenoid time to close
    valveStateBeer_1 = HIGH;                                             //start beer flow
    valveStateBeer_2 = HIGH;

    digitalWrite(ValveBeer_1, valveStateBeer_1);
    delay(500);
    digitalWrite(ValveBeer_2, valveStateBeer_2);

   // Serial.println("Beer valves open......");
    lcd.setCursor(0, 2);
    lcd.print("                    ");
    lcd.setCursor(5, 2);
    lcd.print("Dispensing");

  }
  else                                                                   //button has been pushed while either beer or gas is flowing
  {
    if ((valveStateCO2_1 == LOW) && (valveStateBeer_1 == HIGH))          //if beer is flowing and button pushed,
    {
      FillThreshold_1 = analogRead (FillLevelPressure_1);                    //update fill level threshold(calibrate)


      valveStateBeer_1 = LOW;                                              //stop beer flow


      digitalWrite(ValveBeer_1, LOW);


    }
    valveStateBeer_1 = LOW;                                              //stop beer flow


    //Serial.println("Beer valves closed");
    lcd.setCursor(0, 2);
    lcd.print("          ");
    lcd.setCursor(0, 2);
    lcd.print("Finished");

    valveStateCO2_1 = LOW;                                               //stop gas flow (if button pushed while purging for some reason)


    digitalWrite(ValveBeer_1, LOW);
    digitalWrite(ValveCO2_1, LOW);
  }


}

void eventPress2()
{

 
  if ((valveStateCO2_2 == LOW) && (valveStateBeer_2 == LOW))             //button has been pushed to initate filling, and no valves are open
  {
    
  }
  else                                                                   //button has been pushed while either beer or gas is flowing
  {
    if ((valveStateCO2_2 == LOW) && (valveStateBeer_2 == HIGH))          //if beer is flowing and button pushed,
    {
      FillThreshold_2 = analogRead (FillLevelPressure_2);                    //update fill level threshold(calibrate)
      
      valveStateBeer_2 = LOW;                                              //stop beer flow

      digitalWrite(ValveBeer_2, valveStateBeer_2);

    }
    valveStateBeer_2 = LOW;                                              //stop beer flow
    valveStateCO2_2 = LOW;
    digitalWrite(ValveBeer_2, LOW);
    digitalWrite(ValveCO2_2, LOW);                       //stop gas flow (if button pushed while purging for some reason)

   // Serial.println("Beer valves closed2");
    lcd.setCursor(10, 2);
    lcd.print("          ");
    lcd.setCursor(10, 2);
    lcd.print("Finished");


  }


}

void eventHold2 ()
{
 // Serial.println("Reset threshold 2...");
  lcd.setCursor(19, 3);
  lcd.print("R");
  FillThreshold_2 = 1000;
}

void eventHold1 ()
{
 // Serial.println("Reset threshold 1...");
  lcd.setCursor(8, 3);
  lcd.print("R");
  FillThreshold_1 = 1000;
}

void UpdateDisplay ()
{
  if ( (millis() - lastupdate) > long (updateDelay))
  {
    //    if (buttonVal1 == HIGH)
    //    {
    lastupdate = millis();
    FillLevel_1 = analogRead (FillLevelPressure_1);
    FillLevel_2 = analogRead (FillLevelPressure_2);
    FillLevel_1_round = int ((FillLevel_1 - 443) * 2.56);
    FillLevel_2_round = int ((FillLevel_2 - 539) * 2.56);
    if (FillLevel_1_round >= 0 && FillLevel_2_round >= 0)
    {

    //  Serial.println(FillLevel_1_round);
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
    //    }
  }
}

void Fil1Level_reached2 ()
{
//  Serial.print("Measured fill level 2: ");
//  Serial.println(analogRead (FillLevelPressure_2));
//  Serial.print("Calibrated fill threshold 2: ");
//  Serial.println(FillThreshold_2);
  valveStateBeer_2 = LOW;                           //when reached stop beer flow
  Serial.println("Beer level 2 reached");
  lcd.setCursor(19, 3);
  lcd.print("*");
  lcd.setCursor(10, 2);
  lcd.print("          ");
  lcd.setCursor(10, 2);
  lcd.print("Finished");
  digitalWrite(ValveBeer_2, LOW);

}

void Fil1Level_reached1 ()
{
  Serial.print("Measured fill level 1: ");
  Serial.println(analogRead (FillLevelPressure_1));
  Serial.print("Calibrated fill threshold 1: ");
  Serial.println(FillThreshold_1);
  valveStateBeer_1 = LOW;                                                                                                //when reached stop beer flow
  Serial.println("Beer level 1 reached");
  lcd.setCursor(9, 3);
  lcd.print("*");
  lcd.setCursor(0, 2);
  lcd.print("          ");
  lcd.setCursor(0, 2);
  lcd.print("Finished");
  digitalWrite(ValveBeer_1, LOW);


}
