1. Download and install Code Cmposer Studio (CCS)...
   https://www.ti.com/tool/CCSTUDIO
2. Clone the TSL-calibre-MSP repo...
   https://github.com/bigjosh/TSL-calibre-MSP/
3. Open the TSL project in CCS using "Project->Import existing CCS/Eclipse Project" and picking the "CCS Project" folder in the repo. 
5. Select "Project->Build Configurations->Set Active->Release" from the CCS menu.
5. Select "Project->Build Project" from the CCS menu and make sure there are no errors. Note that this takes longer than you think it should and the only indication that it is working is in the lower right corner of the CCS window.   
6. Plug the EZ-FET programming board into a USB port. 
7. Connect the wires from the EZ-FET  to the TSP board like...
   | TSL | EZ-FET |
   | -  | - |
   | G | Ground |
   | SBWTCK | T | 
   | SBWTDIO | R |
8. Either put batteries into the TSL or connect the TSL `V` pin to the `3V3` pin on the EZ-FET.  
9. Pick "Run->Load->Pick program to load in the CCS menu.
10. Push the "Browse Project" button and pick "Release/TSL-caible-MSP.out" and click OK. 
11. Wait for the program to download. This also takes longer than you think and does not have good feedback.
12. After all the popups go away, the TSL should be running the newly downloaded firmware.

For now on, you can also just pick "Run->Load->TSL-calibre-MSP.out" to compile and download the program again.

If you used the EZ-FET 3V3 pin for power, then you can now test the power usage by clicking on the Energy Trace icon (![image](https://user-images.githubusercontent.com/5520281/200157112-fb56b574-94ee-4805-b37a-e746643ea52a.png)) and 
then clicking on the play button in the new energy trace window. Remember that (1) the trigger switch uses a lot of power until the pin is initially put in (switch opened), and (2) 
there is a lot more software optimization to be done.

