
# use this script to program new units
# sorry it is so ugly, but i wanted it to be pythonic

import subprocess

import time

import shutil

import hashlib

# the following are for airtable stuff
import keyring
import http.client
import json

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
            
    # Create the combined image to burn into the unit that includes both timestamp and firmware

    with open('output.txt','wb') as wfd:
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

    # -j fast means use the fastest clock speed so programming will go as quickly as possible (it still takes a couple seconds)
    # -e ERASE_MAIN prepares the FRAM to recieve the firmware download
    # -v verifies the contents of the FRAM match the firmware file
    # -z [VCC] leaves the device powered up via the VCC pin (You should see the "First Start" message on the LCD display)

    command = f"MSP430Flasher.exe -j fast -r [uuid.txt,0x1A04-0x1A0a] -r [device.txt,0x1a04-0x1a07] -e ERASE_MAIN -w output.txt -v -z [VCC]"

    print("STARING COMMAND:")
    print(command)

    result = subprocess.run(command, capture_output=False)

    # Check the return code and print the output
    if result.returncode != 0:
        print("MSPFlasher failed!")
        exit(1)


    # Lets grab the UUID and device info and make into a serial number
    # these are in TI HEX format so we have to throw away the first line and then strip out all whitespace from the second line to get a clean hex digit string

    with open("uuid.txt","r") as f:
        # throw away the address line
        f.readline()
        # grab the UUID without trailing newline
        uuid_raw = f.readline().rstrip()

    with open("device.txt","r") as f:
        # throw away the address line
        f.readline()
        # grab the device info without trailing newline
        device_raw = f.readline().rstrip()

    # concat the two and and remove embeded spaces
    deviceid = ( device_raw + uuid_raw ).replace(" ","")

    print( f"Device ID is {deviceid}\n")

    print( "Adding record to airtable...\n")

    airtable_record_request = airtable_record_template.format(serialno,firmware_hash,deviceid)

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

    

