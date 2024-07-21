# Print out the contents of the persisant data block in FRAM. Used for debugging. 

import tempfile
import shutil
import os
import sys
import subprocess
import time

from dataclasses import dataclass, fields
from typing import Any

from typing import List

#MSPFlasher executable name
mspflasher_name = "MSP430Flasher"

#locate executable
mspflasher_exec = shutil.which( mspflasher_name )
#check that it was found
if mspflasher_exec is None:
    raise Exception("MSPFlasher executable must be in search path")


@dataclass
class RV3032TimeBlock:
    sec_bcd: int
    min_bcd: int
    hour_bcd: int
    weekday_bcd: int
    date_bcd: int
    month_bcd: int
    year_bcd: int

@dataclass
class PersistentData:
    programmed_time: RV3032TimeBlock
    launched_time: RV3032TimeBlock
    initalized_flag: int
    commisisoned_flag: int
    launched_flag: int
    tsl_powerup_count: int
    mins: int
    days: int
    update_flag: int
    backup_mins: int
    backup_days: int

def parse_persistent_data(byte_array: List[int]) -> PersistentData:
    def parse_rv3032_time_block(offset: int) -> RV3032TimeBlock:
        return RV3032TimeBlock(
            sec_bcd=byte_array[offset],
            min_bcd=byte_array[offset + 1],
            hour_bcd=byte_array[offset + 2],
            weekday_bcd=byte_array[offset + 3],
            date_bcd=byte_array[offset + 4],
            month_bcd=byte_array[offset + 5],
            year_bcd=byte_array[offset + 6]
        )

    def parse_uint(offset: int) -> int:
        return int.from_bytes(byte_array[offset:offset + 2], byteorder='little', signed=False)

    def parse_ulong(offset: int) -> int:
        return int.from_bytes(byte_array[offset:offset + 4], byteorder='little', signed=False)

    return PersistentData(
        programmed_time=parse_rv3032_time_block(0),
        launched_time=parse_rv3032_time_block(7),
        initalized_flag=parse_uint(14),
        commisisoned_flag=parse_uint(16),
        launched_flag=parse_uint(18),
        tsl_powerup_count=parse_uint(20),
        mins=parse_uint(22),
        days=parse_ulong(24),
        update_flag=parse_uint(28),
        backup_mins=parse_uint(30),
        backup_days=parse_ulong(32)
    )

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


def print_dataclass(obj: Any, indent: int = 0):
    
    if not hasattr(obj, '__dataclass_fields__'):
        print(f"{' ' * indent}{obj}")
        return

    for field in fields(obj):

        value = getattr(obj, field.name)

        if isinstance(value, (int, float, str, bool, type(None))):
            print(f"{' ' * (indent + 2)}{field.name:<18}={value:>8} [{hex(value):>10}]")
        else:
            print(f"{' ' * indent}*{field.name}:")
            print_dataclass(value, indent + 2)

    # add a blank line after each class type
    print( " " )


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
        call_line += [ "-r" , f"[{user_file_name},0x1800-0x18ff]" ]
               
        # -z [VCC] leaves the device powered up via the EZ-FET programmer VCC pin (You should see the "First Start" message on the LCD display)
        # call_line += ["-z" , "[VCC]"]
        
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
            print(  parse_persistent_data(data) )

            print_dataclass(  parse_persistent_data(data) )





dump()