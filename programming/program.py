
# use this script to program new units
# sorry it is so ugly, but i wanted it to be pythonic

import subprocess
import time
import shutil
import hashlib
import tempfile
import os
import sys
import time

# the following are for airtable stuff
import keyring
import http.client
import json
import uuid

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
## This is to add a new record
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

# this is to query all matching DeviceID records
airtable_query_template = '''
    {{
      "records": [
        {{
          "fields": {{
            "DeviceID": "{}"
          }}
        }}
      ]
    }}
'''


# returns a nested named toubple of the data parsed out of the pass array of bytes
def parseUserData( data ):
    import struct
    from collections import namedtuple

    """
        // The of time registers we read from the RTC in a single block using one i2c transaction.

        struct __attribute__((__packed__)) rv3032_time_block_t {
            byte sec_bcd;
            byte min_bcd;
            byte hour_bcd;
            byte weekday_bcd;
            byte date_bcd;
            byte month_bcd;
            byte year_bcd;
        };
    """


    # Define the persistent_data_t structure format
    persistent_data_format = "<7B7B2HH2H2H"

    # Define the named tuple based on the persistent_data_t structure
    PersistentData = namedtuple('PersistentData', [
        'commisioned_time',
        'launched_time',
        'once_flag',
        'launch_flag',
        'acid_century_counter'
    ])


    # Function to convert BCD to integer
    def bcd_to_int(bcd):
        return (bcd >> 4) * 10 + (bcd & 0x0F)

    # Unpack the binary data using the persistent_data_t format
    # sadly seems python struct.struct does not support nested structs so this mess here. 
    (
        commisioned_time_sec_bcd, commisioned_time_min_bcd, commisioned_time_hour_bcd, commisioned_time_weekday_bcd, commisioned_time_date_bcd, commisioned_time_month_bcd, commisioned_time_year_bcd,
        launched_time_sec_bcd, launched_time_min_bcd, launched_time_hour_bcd, launched_time_weekday_bcd, launched_time_date_bcd, launched_time_month_bcd, launched_time_year_bcd,

        once_flag, launch_flag,
        acid_in_progress_flag, acid_protected_postmidcentury_flag, acid_protected_century_count, acid_backup_postmidcentury_flag, acid_backup_century_count

    ) = struct.unpack(persistent_data_format, data)

    print("once_flag: [{}] ".format(hex(once_flag)))

    print("commisioned_time_sec_bcd: [{}] {}".format(hex(commisioned_time_sec_bcd), bcd_to_int( commisioned_time_sec_bcd)))
    print("commisioned_time_min_bcd: [{}] {}".format(hex(commisioned_time_min_bcd), bcd_to_int( commisioned_time_min_bcd)))
    print("commisioned_time_hour_bcd: [{}] {}".format(hex(commisioned_time_hour_bcd), bcd_to_int( commisioned_time_hour_bcd)))
    print("commisioned_time_weekday_bcd: [{}] {}".format(hex(commisioned_time_weekday_bcd), bcd_to_int( commisioned_time_weekday_bcd)))
    print("commisioned_time_date_bcd: [{}] {}".format(hex(commisioned_time_date_bcd), bcd_to_int( commisioned_time_date_bcd)))
    print("commisioned_time_month_bcd: [{}] {}".format(hex(commisioned_time_month_bcd), bcd_to_int( commisioned_time_month_bcd)))
    print("commisioned_time_year_bcd: [{}] {}".format(hex(commisioned_time_year_bcd), bcd_to_int( commisioned_time_year_bcd)))

    print("launch_flag: [{}] ".format(hex(launch_flag)))

    # Print BCD values in raw hexadecimal format followed by the BCD values
    print("launched_time_sec_bcd: [{}] {}".format(hex(launched_time_sec_bcd), bcd_to_int( launched_time_sec_bcd)))
    print("launched_time_min_bcd: [{}] {}".format(hex(launched_time_min_bcd), bcd_to_int( launched_time_min_bcd)))
    print("launched_time_hour_bcd: [{}] {}".format(hex(launched_time_hour_bcd), bcd_to_int( launched_time_hour_bcd)))
    print("launched_time_weekday_bcd: [{}] {}".format(hex(launched_time_weekday_bcd), bcd_to_int( launched_time_weekday_bcd)))
    print("launched_time_date_bcd: [{}] {}".format(hex(launched_time_date_bcd), bcd_to_int( launched_time_date_bcd)))
    print("launched_time_month_bcd: [{}] {}".format(hex(launched_time_month_bcd), bcd_to_int( launched_time_month_bcd)))
    print("launched_time_year_bcd: [{}] {}".format(hex(launched_time_year_bcd), bcd_to_int( launched_time_year_bcd)))

    # and the century count data that will become very important starting in the 2100's
    print(f"acid_in_progress_flag: {acid_in_progress_flag}")
    print(f"acid_protected_postmidcentury_flag: {acid_protected_postmidcentury_flag}")
    print(f"acid_protected_century_count: {acid_protected_century_count}")
    print(f"acid_backup_postmidcentury_flag: {acid_backup_postmidcentury_flag}")
    print(f"acid_backup_century_count: {acid_backup_century_count}")

    # next get the device info

    

def print_bytes_as_table(data):
    for i in range(0, len(data), 16):
        line = data[i:i+16]
        hex_line = ' '.join(f'{b:02X}' for b in line)
        print(f'{i:04X}: {hex_line}')

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

def get_ddid( tempdir ):
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



# dump contents of TSL
def dump():

    print("dumping TSL")        
    # Create a temp directory for the files we are creating
    with tempfile.TemporaryDirectory() as tempdir:
        
        # Here is the meat where we...
        # 1. Grab the user data segment, decode, print.
        # 2. Decode and print the device info.
        # 3. Try look up device info in airtable. 
    
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
        user_file_name = os.path.join( tempdir , 'user.txt')
        call_line += [ "-r" , f"[{user_file_name},0x1800-0x181C]" ]
               
        # -z [VCC] leaves the device powered up via the EZ-FET programmer VCC pin (You should see the "First Start" message on the LCD display)
        call_line += ["-z" , "[VCC]"]
        
        print("STARING COMMAND:")
        print(call_line)

        result = subprocess.run( call_line , capture_output=False)

        # Check the return code and print the output
        if result.returncode != 0:
            print("MSPFlasher failed!")
            exit(1)


        # next read the devie info block

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

        # -z [VCC] leaves the device powered up via the EZ-FET programmer VCC pin (You should see the "First Start" message on the LCD display)
        call_line += ["-z" , "[VCC]"]

        print("STARING COMMAND:")
        print(call_line)

        result = subprocess.run( call_line , capture_output=False)

        # Check the return code and print the output
        if result.returncode != 0:
            print("MSPFlasher failed!")
            exit(1)


        # Open the user data file for reading
        with open( user_file_name ,'rt') as file:

            titxt_data = file.read();

            print("Raw user data:")
            print(titxt_data)

            # Read the binary data from the file
            data = decode_titxt(   titxt_data )

            print("decoded user data:")            
            # only parse the first 28 bytes
            parseUserData(data[:28])

        # Open the device data file for reading
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

                # list any records in airtable
                print( "Querying airtable with params...\n")

                # Wholey crap this ws hard to figure out. The airtable API query builder gioves wrong answer. 

                params="filterByFormula=DeviceID%3D%22"+device_uuid+"%22"

                url= endpoint+"?"+params

                # Make the API request to look for records
                conn = http.client.HTTPSConnection(host)

                # Send the GET request to Airtable
                conn.request('GET', url , headers=airtable_headers )

                # Get the response from the Airtable API
                response = conn.getresponse()
                data = response.read().decode('utf-8')

                # Process the response
                if response.status == 200:

                    data = json.loads(data)

                    if 'records' in data:

                        records = data['records']
                        
                        print(f"found {len(records)} matching record in airtable:")

                        for record in records:
                            record_id = record['id']
                            fields = record['fields']
                            print(f"Record ID: {record_id}")
                            print(f"Fields: {fields}")
                    else:
                        print('No records found.')
                else:
                    print(f"Request failed with status code {response.status}:")
                    print(response.read().decode('utf-8'))
                    exit(1)

                # Close the connection
                conn.close()

# enter interactive TSL programming loop
def program_loop():

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
            
# If we started from command line and there is a argument
if __name__ == "__main__" and  len(sys.argv) == 2:
    # One argument provided
        if sys.argv[1].lower()=="dump":
                dump()
        else:
            print(f"Only supported option is `dump`")   
            exit(1)

else :

    program_loop()

