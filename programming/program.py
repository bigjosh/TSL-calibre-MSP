# use this script to program new units
# sorry it is so ugly, but i wanted it to be pythonic

import subprocess
import time
import shutil
import hashlib
import tempfile
import os
import sys
import uuid

# we use this to make it possible to portably read a single keypress
import getch

# for logging to the google sheet
import addrow

#MSPFlasher executable name
mspflasher_name = "MSP430Flasher"

#env variables
relay_port_name  = os.environ.get( "tsl_relayport" )
logscript_url    = os.environ.get( "tsl_logscript" )   

if ( relay_port_name is None or logscript_url is None ):
    print( "Please set the `tsl_relayport` and `tsl_logscript` environment variables. They can be set to `none` if not needed." )
    exit(1)

logscript_enabled = False
if (logscript_url != "none"):
    print( f"Logging to google app script at {logscript_url}" )
    logscript_enabled = True

relay_enabled = False 

print( f"Relay port: {relay_port_name}" )
print( f"Logscript url: {logscript_url}" )  

if (relay_port_name != "none"):

    import serial

    # start up serial_relay (used to disconnect programming pins until we are ready to program)
    print(f"Opening Relay port on {relay_port_name}...")
    serial_relay = serial.Serial( port=relay_port_name,  timeout=1 )
    
    # this silly protocol is defined at http://www.chinalctech.com/cpzx/Programmer/Relay_Module/115.html
    def set_relay( relay_index , relay_state ):
        serial_relay.write( bytearray(  [ 0xa0 , relay_index , relay_state , (0xa0+relay_index+relay_state) ] ) )
        serial_relay.flush()

    # Close relay
    def relay_close():
        set_relay( 1 , 1 )
        ## give the relay a little bit of time to close
        time.sleep(0.2)    # 200ms delay

    # Open relay
    def relay_open():
        set_relay( 1 , 0 )
        ## give the relay a little bit of time to close
        time.sleep(0.2)    # 200ms delay

    # default relay is open, only close it if we are actually programming
    # this keeps the pins disconnected from power when we are inserting and removing the TSL from the fixture
    relay_open()        

    print( f"Relay is enabled with Relay port {relay_port_name}" )
    # remeber that we are configured to measure power
    relay_enabled = True

#locate MSP430Flasher executable
mspflasher_exec = shutil.which( mspflasher_name )
#check that it was found
if mspflasher_exec is None:
    raise Exception("MSPFlasher executable must be in search path")  

print( f"Using MSPFlasher executable at {mspflasher_exec}" )

machine_uuid_string = str(uuid.UUID(int=uuid.getnode()))
print( f"Machine UUID: {machine_uuid_string}" )


def print_bytes_as_table(data):
    for i in range(0, len(data), 16):
        line = data[i:i+16]
        hex_line = ' '.join(f'{b:02X}' for b in line)
        print(f'{i:04X}: {hex_line}')

""" Read in a firmware image file in TI txt format and return a byte array """
def decode_titxt(titxt_data):

    #lines = titxt_data.decode('utf-8')    

    lines = titxt_data.splitlines()

    # Remove any leading or trailing whitespace from each line
    lines = [line.strip() for line in lines]

    # Remove empty lines and comments
    lines = [line for line in lines if line and not line.startswith('@') and not line.startswith('q')]

    # Concatenate the data fields into a single string
    data_string = ''.join(lines)

    # Convert the hexadecimal string to a byte array
    data_bytes = bytes.fromhex(data_string)

    return data_bytes   


    # Lets grab the UUID and device info from the device descripto dump and make them into a serial number
    # these are in TI HEX format so we have to throw away the first line and then strip out all whitespace from the second line to get a clean hex digit string

def get_ddid_from_file( dd_file_name ):
    with open(dd_file_name,"r") as f:
        # throw away the address line
        f.readline()
                
        # grab first and second line and strip all whitespace
        line1 = f.readline()
        line2 = f.readline()
        
        # now line1 is bytes 0x1a00-0x1a0f of the device desriptor table as ascii hex digits
        # now line2 is bytes 0x1a10-0x1a11 of the device desriptor table as ascii hex digits
        
        # make all the read bytes into a single linear array
        dd_hex_bytes = (line1+line2).split()

        # note in all the extractions below that in Python string slices, the end index is one past the index               
                                        
        # device info is 0x1a04-0x1a07 (4 bytes)
        device_info =  dd_hex_bytes[ 0x04 : 0x08 ]

        # Lot waffer ID 0x1a0a-0x1a0d
        lot_waffer = dd_hex_bytes[ 0x0a : 0x0e ]
        
        # Die X pos 0x1a0e-0x1a0f               
        die_x_pos =  dd_hex_bytes[ 0x0e : 0x10 ]
        
        # Die Y pos 0x1a10-0x1a11
        die_y_pos = dd_hex_bytes[ 0x10 : 0x12 ]
        
        # I know this is ugly, but seems to be the phythonic way to join these all into a string with no seporator.
        # Do you know a better way? 
        device_uuid = "".join(device_info+lot_waffer+die_x_pos+die_y_pos)

        return device_uuid

# enter interactive TSL programming loop
def program_loop():

    # firmware in TI hex format
    firmware_file_name = "tsl-calibre-msp.txt"

    # load the firmware file into memory variable `data`

    print(f"Loading {firmware_file_name} into memory...")   
    with open(firmware_file_name, 'rb') as file:
        data = file.read()

    # cacluate the hash of the firmware file
    firmware_hash = hashlib.md5(data).hexdigest()

    print( f"Firmware hash is {firmware_hash}\n")

    # repeat programming cycle until user quits or error
    while True: 
        
        #clear any buffered keypresses
        while ( getch.key_available() ):
            getch.getch()


        # wait for user to start programming cycle
        print("\rPress [spacebar] to start programming cycle, "+ ("'C'=Relay closed, 'O'=Relay open, " if relay_enabled else "") + " any other key to exit...")

        while ( not getch.key_available() ):
            pass

        key = getch.getch()

        if (relay_enabled):
            if key.lower()=='o':
                relay_open()
                continue
            if key.lower()=='c':
                relay_close()
                continue

        if key != ' ':
            print(f"Exited by user, key = {key}")
            exit(0)

        print("Programming cycle started.")

        if relay_enabled:
            print("Closing relay to send power to the TSL.")
            relay_close(); 
                
        # Create a temp directory for the files we are creating, then create temp files for the firmware image (will auto delete everything when pass finished)
        with tempfile.TemporaryDirectory() as tempdir:
            
            # first we create a firmware image to write to FRAM. We do this by combining a timestamp with the compiled firmware 

            print("Creating firmware image...")
            image_file_name = os.path.join( tempdir , 'image.txt' )    
            with open( image_file_name ,'wb') as wfd:
            
                # get the current GMT time
                t = time.gmtime()                

                # prepend a time stamp (in TI HEX format)
                # 1800 is the begining of "information memory" FRAM.
                wfd.write("@1800\n".encode())
                wfd.write(f"{t.tm_sec:02d} {t.tm_min:02d} {t.tm_hour:02d} {t.tm_wday:02d} {t.tm_mday:02d} {t.tm_mon:02d} {t.tm_year%100:02d}\n".encode())
                    
                # ...and append firmware file into the image file
                # note that the firmware comes last becuase the TI tools add a "q" to the end of this file.
                with open('tsl-calibre-msp.txt','rb') as rfd:
                    shutil.copyfileobj(rfd, wfd)

                # If you ever need to read the commisioned time out of a unit, you can use the command...
                # MSP430Flasher.exe -j fast -r [commisioned_time.txt,0x1800-0x1806] -z [VCC]
                # Do note that the timestamp does not have a century field, so you will have to infer what century the unit was commisioned
                # in using other factors like how dusty it is. 

            print("Firmware image ready.")
            # Here is the meat where we...
            # 1. Grab the device UUID and ID from the MSP430 chip. This will let us keep a record of the serial number and what version of the chip this unit has.
            # 2. Erase the FRAM. 
            # 3. Burn the combined image into the unit.
        
            # start with executable
            call_line = [mspflasher_exec]

            # -j fast means use the fastest clock speed so programming will go as quickly as possible (it still takes a couple seconds)
            call_line +=[ "-j" , "fast" ]

            # dump the whole device descriptor table
            # note that it would be nice to just dump the two parts we need (device_id & uuid), but there is an undocumented limitation
            # in MSP430Flasher where if you try to do consecutive read operations, it just siliently ignores the second one. So instead
            # we dump the whole table and will parse out the parts we care about later. 
            # device desciptor table is in the MSP430FR4133 datasheet section 9.1
            
            # Dump the device descirtor data from the MSP430 to a file named `dd.txt` in the temp directory. 
            dd_file_name = os.path.join( tempdir , 'dd.txt')
            call_line += [ "-r" , f"[{dd_file_name},0x1a00-0x1a12]" ]
            
            # -e ERASE_MAIN prepares the FRAM to recieve the firmware download
            call_line += ["-e","ERASE_MAIN"]
            
            #program in the firmware image we created earlier
            call_line += ["-w" , image_file_name ]
            
            # -v verifies the contents of the FRAM match the firmware image file
            call_line += ["-v"]
            

            # TODO TEMP
            # -z [VCC] leaves the device powered up via the EZ-FET programmer VCC pin (You should see the "First Start" message on the LCD display)
            call_line += ["-z" , "[VCC]"]
            
            print("Powering up TSL, reading device ID and UUID, erasing FRAM, and writing firmware image...")
            print("STARING COMMAND:")
            print(call_line)

            result = subprocess.run( call_line , capture_output=False)

            # Check the return code and print the output
            if result.returncode != 0:
                print("MSPFlasher failed!")
                exit(1)
        
            # Lets grab the UUID and device info from the device descriptor dump we just grabbed above and make them into a serial number
            device_uuid = get_ddid_from_file( dd_file_name )
                        
            print( f"Device UUID is {device_uuid}")

            # remove power from fixture (and device) so it can be safely removed
            # Allow a monent for LEDs to flash and the 1uF capacitor to charge
            print("Waiting for flash to complete...")
            time.sleep(2)

            if (relay_enabled):
                # Power down the device and fixture
                print("Powering down device and fixture...")
                relay_open()

            if ( logscript_enabled ):
                print( "Adding record to log...")
                addrow.send_data_to_sheet(logscript_url,[device_uuid,firmware_hash,machine_uuid_string])
                print("Success!")  

            
# If we started from command line and there is a argument
if __name__ == "__main__":

    if (sys.argv.__len__() == 1):
        program_loop()

    # This is a special command to let us programatically close the relay
    if (sys.argv.__len__() == 2) and (sys.argv[1] == "cr"):

        if relay_enabled:
            relay_close()
            print("Relay closed.")   
        else:
            print("Relay is not enabled. Make sure the `tsl_relayport` environment variable is set.")   
        exit(0)
    