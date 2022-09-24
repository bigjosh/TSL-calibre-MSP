## Software considerations

We want to make make sure that all segments for a given digit are accessible with a single write so that we can quickly update that digit and also not mess up any other digits. 

Each digit has 7 segments, and on the LCD they are arranged so that those segments are addressed via 2 segment pins * 4 common pins. See how all the segments in digit #1 are connected to LCD pins 1 and 2...

![](LCD%20pin%20detail.png)  

We are using a 4-mux LCD and in this mode the MSP430 puts 2 physical pins in each writeable register. In each MSP430 LCD memory byte, the lower 4 bits are the the 4 common pins for one segment pin and the upper 4 bit are the 4 common pins on the next segment pin...

![](MSP430%20LCD%20memory%20map.png)  

Importantly note that the pairs are always an even then odd pin number since they start at `L0`. So `L62` and `L63` can be connected to the same digit on the LCD since we can update both with a single write to `LCDM31`, but if we connected LCD digit #1 to MSP pins `L61` and `L62` then we would need to write to both `LCDM31` and `LCDM30` to update the digit on the LCD display (and mess up whatever was on `L63` and `L60`).

## PCB Layout considerations

Additionally we want to be able to optimize the layout of the traces to keep them short and non-overlapping. Luckily the MSP-430FR411 gives us flexibility to map the LCD common pins to any of the `L` pins on the MSP, so we map them to `L50`-`L53` which are in the upper left corner of the MSP430, corresponding to the location of the common pins on the LCD. 

Here are the physical layouts of the chip pins and the LCD pins they way they are oriented on the PCB...

![](LCD%20trace%20layout.png)  

These trace mappings are reflected both on the PCB layout and with `#define`s in the firmware. 

