import subprocess

import time

import shutil


# // The format that we use to store timestamps
#
#struct __attribute__((__packed__)) rv3032_time_block_t {
#    byte sec_bcd;
#    byte min_bcd;
#    byte hour_bcd;
#    byte weekday_bcd;
#    byte date_bcd;
#    byte month_bcd;
#    byte year_bcd;
#};

# Create a file snippet in TI hex format that has the current time/date

def create_timestamp_file():
    # get the current time
    t = time.localtime()

    # write the timecode snippet to a file called "timecode.txt"
    with open("timestamp.txt", "w") as f:
        f.write("@1800\n")
        f.write(f"{t.tm_sec:02d} {t.tm_min:02d} {t.tm_hour:02d} {t.tm_wday:02d} {t.tm_mday:02d} {t.tm_mon:02d} {t.tm_year%100:02d}\n")

# first we create a timestamp file. This will get burned into this unit as the "commisioned time".
# note that this time is never used by the unit - it is only here for archival purposes so that if this
# unit ever dies then someone can read the commisioned time out and then use it to calculate when the device
# was first programmed and when it was triggered.
# If you ever need to read the commisioned time out of a unit, you can use the command...
# MSP430Flasher.exe -j fast -r [commisioned_time.txt,0x1800-0x1806] -z [VCC]
# Do note that the timestamp does not a a century field, so you will have to infer what century the unit was commisioned
# in using other factors like how dusty it is. 

create_timestamp_file()

# next we concatinate the timestamp plus the firmware into a single output file.
# sadly we must do this becuase it seems that the flasher tool only obeys a single write per invokation, and starting it is slow
# note that the firmware file must come last becuase it ends with a "q" that will end the write. :/
# This file copy code comes from https://stackoverflow.com/a/27077437/3152071
with open('output.txt','wb') as wfd:
    for f in ['timestamp.txt','tsl-calibre-msp.txt']:
        with open(f,'rb') as fd:
            shutil.copyfileobj(fd, wfd)

# Here is the meat where we...
# 1. Grab the device UUID and ID from the MSP430 chip. This will let us keep a record of the serial number and what version of the chip this unit has
# 2. Burn the current time/date into the info memory as described above
# 3. Burn the firmware

# -j fast means use the fastest clock speed so programming will go as quickly as possible (it still takes a couple seconds)
# -e ERASE_MAIN prepares the FRAM to recieve the firmware download
# -v verifies the contents of the FRAM match the firmware file
# -z [VCC] leaves the device powered up via the VCC pin (You should see the "First Start" message on the LCD display)

command = f"MSP430Flasher.exe -j fast -r [uuid.txt,0x1A04-0x1A0a] -r [device.txt,0x1a04-0x1a07] -e NO_ERASE -w output.txt -v -z [VCC]"

print("STARING COMMAND:")
print(command)

result = subprocess.run(command, capture_output=False)

# Check the return code and print the output
if result.returncode != 0:
    print("MSPFlasher failed!")
    exit()


input("Press Enter to continue...")

# s.replace(" ", "")

# read device UID
# MSP430Flasher.exe -j FAST -r [dump.txt,0x1A0A-0x1A11]

# MSP430Flasher.exe -j fast -e ERASE_MAIN -r [uuid.txt,0x1A04-0x1A0a] -r [device.txt,0x1a04-0x1a07] -w tsl-calibre-msp.txt -v -z [VCC]
# MSP430Flasher.exe -j fast -r [info.txt,0x1800-0x1900] -z [RESET,VCC]

# MSP430FR4133 F0h 81h
# MSP430FR4132 F1h 81h
# MSP430FR4131 F2h 81h

