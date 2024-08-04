# PROGRAMMING

## One-time Set up

### Software
 1. Download `program.py`, 'addrow.py', and `tsl-calibre-msp.txt` from a release in this repo.
 2. Install Python. (The TSL programming script is in Python)
 3. Install [KeyRing](https://pypi.org/project/keyring/). (We use KeyRing to securely store the AirTable API key)
 4. Install [MSPFlasher](https://www.ti.com/tool/MSP430-FLASHER). (This actually talks to the programming hardware)
 5. Make sure that the MSPFlasher executable is in the search path.
 6. Create a google spreadhseet with 5 columns and give them names like "Timestamp",	"Device Serial Number",	"Device UUID",	"Firmware Hash",	"Programmer Mac Address" amd freeze the top row.
 7. Add a webapp to the sheet with "Extentions->Appscript" and paste the code from `addrow-appscript.gs` into the code editor.
 8. Click "Deploy->New Deploy" and select "Run as" you and "Allow anyone".
 9. Click "Deploy" and copy the URL it gives you. 
 7. Add the aboveURL  to KeyRing by entering this command...
     `keyring set "tsl-programmer" "addrow-url"`
      ...and then pasting in the URL you got in the step above when prompted for the "password". You have to press enter after 
     you paste it. Note that it does not print the token to the screen when you paste it.
 8. Enter the command `keyring get "tsl-programmer" "addrow-url"` and confirm that it prints the same URL that got from the deploy step. If not, try the above step again. 
 
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
1. Run the `program.py` program.
1. Connect a new TSL board without batteries to the programming connector.
2. Enter the unit's serial number. 
2. _Confirm_ that the firmware download completes without error.
3. _Confirm_ that the TSL starts, the two LEDs flash, and the display shows the 'First Start' message.
4. Disconnect the programming connector. The display should show all 8's and fade out while the unit tests its power consumption. Be sure to hold the board by the edges - just touching the LCD pins can increase the power usage enough to cause the test to fail. 
5. Within 1 minute, install the batteries into the TSL.
6. _Confirm_ that the display shows the 'Load Pin' message.
7. Assemble the TSL into the tube.
8. Insert the pull pin.
9. The display should show 'Arming' followed by the dancing dashes animation.
9. _Confirm_ that the dancing dashes display lights all the LCD segments and that it updates steadily once per second. 
10. Pack it up and ship it out!

Reject the unit if it fails any of the _confirm_ steps above.

## Troubleshooting

Try unplugging the EZFET board from the USB and plugging it back in. Sometimes it gets messed up if the computer goes to sleep.
