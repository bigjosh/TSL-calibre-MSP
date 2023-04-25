
# use this script to program new units
# sorry it is so ugly, but i wanted it to be pythonic

import subprocess
import time
import shutil
import hashlib
import tempfile
import os

# the following are for airtable stuff
import keyring
import http.client
import json

#MSPFlasher executable name
mspflasher_name = "MSP430Flasher"

#locate executable
mspflasher_exec = shutil.which( mspflasher_name )
#check that it was found
if mspflasher_exec is None:
    raise Exception("MSPFlasher executable must be in search path")


# Airtable API host
host = "api.airtable.com"
# This is the base and table ID of the TSL units table
endpoint = "/v0/app11MZ4rXXpEyFnj/tblunHqmlHFKtvaZ1"
# Reads the Airtable API access token that was stored into KeyRing durring setup
airtable_token = keyring.get_password("tsl-airtable", "token")
# Headers for the API request. 
airtable_headers = {
    "Authorization": f"Bearer {airtable_token}",
    "Content-Type": "application/json"
}

# OMG python is so ugly. 
airtable_record_template = '''
    {{
      "records": [
        {{
          "fields": {{
            "Serialnumber": "{}",
            "Firmware": "{}",
            "DeviceID": "{}"
          }}
        }}
      ]
    }}
'''

# firmware in TI hex format
firmware_file_name = "tsl-calibre-msp.txt"

# load the firmware file into memory variable `data`

with open(firmware_file_name, 'rb') as file:
    data = file.read()

# cacluate the hash of the firmware file
firmware_hash = hashlib.md5(data).hexdigest()

print( f"Firmware hash is {firmware_hash}\n")

# repeat programming cycle until user quits or error

while True: 

    serialno = input ("Serial Number (or blank to exit)?")

    # Check if the string is empty
    if not serialno:
        print("Goodbye.")
        exit(0)
            
    # Create a temp directory for the files we are creating, then create temp files for the firmware image (will auto delete everything when pass finished)
    with tempfile.TemporaryDirectory() as tempdir:
        
        # first we create a firmware image to write to FRAM. We do this by combining a timestamp with the compiled firmware 

        image_file_name = os.path.join( tempdir , 'image.txt' )    
        with open( image_file_name ,'wb') as wfd:
        
            # get the current time
            t = time.localtime()
            
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
        
        # -z [VCC] leaves the device powered up via the EZ-FET programmer VCC pin (You should see the "First Start" message on the LCD display)
        call_line += ["-z" , "[VCC]"]
        
        print("STARING COMMAND:")
        print(call_line)

        result = subprocess.run( call_line , capture_output=False)

        # Check the return code and print the output
        if result.returncode != 0:
            print("MSPFlasher failed!")
            exit(1)

        # Lets grab the UUID and device info from the device descripto dump and make them into a serial number
        
        
        # these are in TI HEX format so we have to throw away the first line and then strip out all whitespace from the second line to get a clean hex digit string

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

            print( f"Device UUID is {device_uuid}\n")

            print( "Adding record to airtable...\n")

            airtable_record_request = airtable_record_template.format(serialno,firmware_hash,device_uuid)

            # Make the API request to create a new record
            conn = http.client.HTTPSConnection(host)
            conn.request("POST", endpoint, airtable_record_request, airtable_headers)
            response = conn.getresponse()

            # Check if the request was successful
            if response.status == 200:
                print("New record created successfully!")
            else:
                print("Error creating new record")
                print(response.read().decode())
                exit(1)
            
