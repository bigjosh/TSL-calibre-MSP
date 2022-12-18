## Software considerations

We want to make make sure that all segments for a given digit are accessible with a single write so that we can quickly update that digit and also not mess up any other digits. This is especially true for the most frequently updated digits like the seconds but not as important as, say, day digits since these are updated so infrequently. 

Each digit has 7 segments, and on the LCD they are arranged so that those segments are addressed via 2 segment pins * 4 common pins. 

We are using a 4-mux LCD and in this mode the MSP430 puts 2 physical pins in each writeable LCD memory register. In each MSP430 LCD memory byte, the lower 4 bits are the the 4 common pins for one segment pin and the upper 4 bit are the 4 common pins on the next segment pin. Each segment pin has a logical name starting with "L0" and counting up to "L37".

Importantly note that the pairs are always an even then odd pin number since they start at `L0/L1`. So `L22` and `L23` can be connected to the same digit on the LCD since we can update both with a single write to `LCDM11`, but if we connected LCD digit #1 to MSP pins `L21` and `L22` then we would need to write to both `LCDM11` and `LCDM10` to update the digit on the LCD display (and mess up whatever was on `L23` and `L20`).

Note that the MSP430 is a 16 bit processor, so it is also possible to update two digits with a single word write if the registers connected to them are next to each other in memory and do not cross a word boundary. This is probably only worth the additional effort to save a single cycle in the seconds digits. 

## PCB Layout considerations

Additionally we want to be able to optimize the layout of the traces to keep them short and non-overlapping. Luckily the MSP-430FR411 gives us flexibility to map the LCD common pins to any of the `L` pins on the MSP, so we can map them to `L08`-`L11` which are in the upper left corner of the MSP430, physically corresponding to the location of the common pins on the LCD. 
