import subprocess

# Define the parameters for MSPFlasher
input_file = "tsl-calibre-msp.txt" # The file name to write to chip

# Build the command line string for MSPFlasher
# command = f"C:/ti/MSPFlasher_1.3.20/MSP430Flasher.exe -w {input_file} -v -z [VCC]"
# command = f"C:/ti/MSPFlasher_1.3.20/MSP430Flasher.exe -r [info.txt,0x1800-0x1900]  -z [VCC]"
command = f"C:/ti/MSPFlasher_1.3.20/MSP430Flasher.exe -r [fram.txt,0xc400-0xc500]  -z [VCC]"

print("STARING COMMAND:")
print(command)

# Call MSPFlasher using subprocess.run()
#result = subprocess.run(command, capture_output=True)
result = subprocess.run(command, capture_output=False)

print("STDOUT:")
print( result.stdout )
print("STDERR:")
print( result.stderr )
print("CODE:")
print( result.returncode )


# Check the return code and print the output
if result.returncode == 0:
    print("MSPFlasher succeeded!")
    print(result.stdout.decode())
else:
    print("MSPFlasher failed!")
    print(result.stderr.decode())

input("Press Enter to continue...")    

# read device UID
# MSP430Flasher.exe -j FAST -r [dump.txt,0x1A0A-0x1A11]

# MSP430Flasher.exe -j fast -r [uuid.txt,0x1A04-0x1A0a] -r [device.txt,0x1a04-0x1a07] -w tsl-calibre-msp.txt -z [VCC]
# MSP430Flasher.exe -j fast -r [info.txt,0x1800-0x1900] -z [RESET,VCC]

# MSP430FR4133 F0h 81h
# MSP430FR4132 F1h 81h
# MSP430FR4131 F2h 81h

