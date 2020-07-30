# Dual-Beer-Can-Filler
Dual Beer can filler with differential pressure sensor fill level sensing and CO2 purging. Also support an I2C (20,4) display (not mandatory)

//  TTBF Dual Can Filler v1.0

This code is 100% functional but could be tidied and streamlined a lot more (work in progress). Could also be optimized with more fuctions etc etc. Can be used with any Arduino model like the Nano or UNO.

Currently its supporting 2 push buttons and one I2C display, the display is showing purging and dispense status for both filler lines.
The two buttons has same fuctions for each fill line:
-- SHORT PRESS starts whole sequence with purging and filling corresponing beer line. 
-- At first filling both buttons needs to SHORT PRESSED again for the corresponding line when desired fill level is reached. The desired fill level is then stored for future fills (until reset or power down).
-- the next fill will stop at the programmed fill level, the display will indicate wit an '*' next to the fill level measurement.
-- After filling a LONG PRESS of any of the buttons resets the fill level for the corresponing beer line, indicated with a captial 'R' in the display. This allows to correct fill level when changing can size etc.

The display itself is also cosmetical in one sense, the filler wors perfectly without it if you desire to leave it out.
Displayed fill level progress is in 'mililiters' and must be calibrated for each system before first time use. Purely cosmetical, no practical implications if not calibrated. Can easily be changed to other units if desired, or removed completely.

Please feel free to use and change as you like, and please upload your own code if you do so. That way others can benefit from your tweaks or improvements.
