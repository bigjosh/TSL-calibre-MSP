# PROGRAMMING

## One-time Set up

### Software
 1. Download `program.py`, 'addrow.py', and `tsl-calibre-msp.txt` from a release in this repo.
 2. Install Python. (The TSL programming script is in Python)
 3. Install PySerial with `pip install pyserial`. This lets us acces the relay and the current monitor over serial connections. 
 4. Install [MSPFlasher](https://www.ti.com/tool/MSP430-FLASHER). (This actually talks to the EZ-FET programming hardware)
 5. Make sure that the MSPFlasher executable is in the search path.
 
 #### Config

 ##### Ports

 ##### Logging spreadsheet

 We optionally keep a record of every TSL programming event in a google sheet using a google app script. 
 
 If you do not want to use this feature, set the environment variable `tsl_logscript` to `none`.
 
 If you do want to use logging...

6. Create a google spreadhseet with 5 columns and give them names like "Timestamp",	"Device Serial Number",	"Device UUID",	"Firmware Hash", "Programmer Mac Address" and freeze the top row.
 7. Add a webapp to the sheet with "Extentions->Appscript" and paste the code from `addrow-appscript.gs` (in this folder) into the code editor.
 8. Click "Deploy->New Deploy" and select "Run as" you and "Allow anyone".
 9. Click "Deploy" and copy the URL it gives you. 
 9. Set the envrironment variable `tsl_logscript` to this URL. 
  
### Hardware

You will at least need a TI board with an EZFet programmer on it. There are lots and any will do. I used the MSP-EXP430FR4133.

If you optionally want to test each board's power usage after programming, you will also need...

1. A USB relay board with at leasdt two relays. I used [this one from Amazon](https://amzn.to/40phLwM). If you use a different one with a different protocol then you might need to update the code. 
2. A [CurrentRanger](https://lowpowerlab.com/shop/product/152). 
2. A USB isolator like [this one from Amazon](https://amzn.to/42tPEza). 
3. Maybe a USB hub so you can connect all 3 USB things to the computer. 

The idea is that the EZFET programmer powers the TSL and downloads the firmware into it. We then use the relays to disconnect the two programming pins from the TSL so that we can get accurate power measurements (some current otherwise leaks though those pins). We then use the CurrentRanger to read the ammount of current that the TSL is using durring a test period. We need the CurrentRnager to be connected though the USB isolator because it has [a floating ground](https://lowpowerlab.com/guide/currentranger/safety-and-proper-usage/).

Schemeatically, the setup looks like this...


The 

2. Remove all of the jumpers on the programmer board and connect it to the computer via USB.
3. Connect the pins from the EZFet side of the programmer (the side closer to the USB) to the programming connector like this...
    | Programmer pin | TSL pin |
    | - | - | 
    | G | G |
    | 3V3 | V |
    | SBWTDIO | R | 
    | SBWTICK | T |
4. Get a [CurrentRanger](https://lowpowerlab.com/shop/product/152). We use this to test the current usage of each TSL.
5. Get a [USB relay](https://amzn.to/40phLwM). We use this to disconnect the TSL from the programmer while we test the current usage.


## Commissioning Procedure
1. Run the `program.py` program.
1. Connect a new TSL board without batteries to the programming connector.
2. Enter the unit's serial number. 
2. _Confirm_ that the firmware download completes without error.
3. _Confirm_ that the TSL starts, the two LEDs flash, and the display shows the 'First Start' message.
4. Disconnect the programming connector. The display should show all 8's and fade out while the unit tests its power consumption. Be sure to hold the board by the edges - just touching the LCD pins can increase the power usage enough to cause the power test to fail. 
5. Within 1 minute, install the batteries into the TSL.
6. _Confirm_ that the display shows the 'Load Pin' message.
7. Assemble the TSL into the tube.
8. Insert the pull pin.
9. The display should show 'Arming' followed by the `Ready To Launch Mode` dancing dashes animation.
9. _Confirm_ that the dancing dashes display lights all the LCD segments and that it updates steadily once per second. 
10. Pack it up and ship it out!

Reject the unit if it fails any of the _confirm_ steps above.

## Troubleshooting

Try unplugging the EZFET board from the USB and plugging it back in. Sometimes it gets messed up if the computer goes to sleep.
