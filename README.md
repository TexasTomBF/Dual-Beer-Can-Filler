# Dual-Beer-Can-Filler
Dual Beer can filler with differential pressure sensor fill level sensing and CO2 purging. Also support an I2C (20,4) display (not mandatory)

//  TTBF Dual Can Filler v1.02

//This code is 100% functional but could be tidied and streamlined a lot more (work in progress). Could also be optimized with more fuctions etc etc. Can be used with any Arduino model like the Nano or UNO.
//Currently its supporting 2 momentary push buttons and one I2C display, the display is showing purging and dispense status for both filler lines.
//The two buttons has same fuctions for each fill line:
//-- SHORT PRESS starts whole sequence with purging and filling corresponding beer line.
//-- At first filling both buttons needs to be SHORT PRESSED again for the corresponding line when desired fill level is reached. The desired fill level is then stored for future fills (until reset or power down).
//-- the next fill will stop at the programmed fill level, the display will indicate fill level reached with an '*' next to the fill level measurement.
//-- After filling a LONG PRESS of any of the buttons resets the fill level for the corresponing beer line, indicated with a captial 'R' in the display for corresponding beer line. This allows to correct/reset fill level when changing can size etc. The new level must then be set on next filling (like at power up).
//
//The display itself is purely cosmetical in one sense, the filler works perfectly without it if you desire to leave it out.
//Displayed fill level progress is in 'mililiters' and must be calibrated for each system before first time use. Purely cosmetical, no practical implications if not calibrated. Can easily be changed to other units if desired, or removed completely.
//
//Differential pressure sensors are used for level sensing using the same fill tube as for co2 purging. Separate fill tubes for beer. 12V Solenoid valves for water/beer and three way 12V solenoid valves for co2/level sensing. Full parts list on GitHub also.


Please feel free to use and change as you like, and please upload your own code if you do so. That way others can benefit from your tweaks or improvements.
