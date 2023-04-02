import subprocess

# Define the parameters for MSPFlasher
device = "MSP430G2553" # The device type
interface = "TIUSB" # The interface type
input_file = "blink.txt" # The input file name
erase = "ERASE_ALL" # The erase method

# Build the command line string for MSPFlasher
command = f"MSP430Flasher.exe -n {device} -w {input_file} -v -g -z [VCC] -i {interface} -e {erase}"

# Call MSPFlasher using subprocess.run()
result = subprocess.run(command, capture_output=True)

# Check the return code and print the output
if result.returncode == 0:
    print("MSPFlasher succeeded!")
    print(result.stdout.decode())
else:
    print("MSPFlasher failed!")
    print(result.stderr.decode())

# read device UID
# MSP430Flasher.exe -j FAST -r [dump.txt,0x1A0A-0x1A11]

# MSP430Flasher.exe -j fast -r [uuid.txt,0x1A04-0x1A0a] -r [device.txt,0x1a04-0x1a07] -w tsl-calibre-msp.txt -z [VCC]

# MSP430FR4133 F0h 81h
# MSP430FR4132 F1h 81h
# MSP430FR4131 F2h 81h


Lot Wafer ID
1A0Ah Per unit
1A0Bh Per unit
1A0Ch Per unit
1A0Dh Per unit
Die X position
1A0Eh Per unit
1A0Fh Per unit
Die Y position
1A10h Per unit
1A11h Per unit