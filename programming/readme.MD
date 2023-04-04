# PROGRAMMING

## Set up

### Software
 1. Download `program.py` and `tsl-calibre-msp.txt` from a release in this repo.
 2. Install Python. (The TSL programming script is in Python)
 3. Install [KeyRing](https://pypi.org/project/keyring/). (We use KeyRing to securely store the AirTable API key)
 4. Install [MSPFlasher](https://www.ti.com/tool/MSP430-FLASHER). (This actually talks to the programming hardware)
 5. Make sure that the MSPFlasher executable is in the search path.
 
### Hardware
1. Get yourself a TI board with an EZFet programmer on it. There are lots and any will do. I used the MSP-EXP430FR4133.
2. Remove all of the jumpers on the programmer board and connect it to the computer via USB.
3. Connect the pins from the EZFet side of the programmer (the side closer to the USB) to the programming connector like this...
    | Programmer pin | TSL pin |
    | - | - | 
    | G | G |
    | 3V3 | V |
    | SBWTDIO | R | 
    | SBWTICK | T |

## Commissioning Procedure
1. Connect a new TSL board without batteries to the programming connector.
2. Run the `program.py` program and make sure it completes without error.
3. Confirm that the TSL display shows the "First Start" message.
4. Disconnect the programming connector. The display should fade out. 
5. Within 1 minute, install the batteries into the TSL.
6. Confirm that it displays the "Load Pin" message.
7. Assemble the TSL into the tube.
8. Insert the trigger pin and confirm that the display says "Arming" followed by the dancing dashes animation.
9. Confirm that the dancing dashes display lights all the LCD segments and that it steps with a steady 1Hz cadence.
10. Pack it up and ship it out! 

Note that this procedure intentionally does not prove out the flash LEDs. We could flash them when we insert the pin, 
but just that little test flash uses up many years of battery life. Should we do it? 



 
