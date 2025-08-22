#!/usr/bin/env python3
"""
normalize.py - Normalize MSP430 memory dump files with current time calculations

This program reads an MSP430 memory dump file (0x1800-0x18ff range) and updates
the persistent days and minutes values based on the current date/time.

Usage: python normalize.py <input_file> <output_file>
"""

import sys
import datetime
import time
from dataclasses import dataclass
from typing import List

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
    porsoltCount: int
    tsl_powerup_count: int
    mins: int
    days: int
    update_flag: int
    backup_mins: int
    backup_days: int

def bcd_to_int(bcd_value: int) -> int:
    """Convert BCD (Binary Coded Decimal) to integer"""
    return ((bcd_value >> 4) * 10) + (bcd_value & 0x0F)

def int_to_bcd(value: int) -> int:
    """Convert integer to BCD (Binary Coded Decimal)"""
    return ((value // 10) << 4) | (value % 10)

def timeblock_to_datetime(timeblock: RV3032TimeBlock) -> datetime.datetime:
    """Convert RV3032TimeBlock to datetime object"""
    return datetime.datetime(
        year=2000+bcd_to_int(timeblock.year_bcd),
        month=bcd_to_int(timeblock.month_bcd),
        day=bcd_to_int(timeblock.date_bcd),
        hour=bcd_to_int(timeblock.hour_bcd),
        minute=bcd_to_int(timeblock.min_bcd),
        second=bcd_to_int(timeblock.sec_bcd)
    )

def decode_titxt(titxt_data: str) -> bytes:
    """Decode TI TXT format data to bytes"""
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

def parse_persistent_data(byte_array: List[int]) -> PersistentData:
    """Parse byte array into PersistentData structure"""
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
        porsoltCount=parse_uint(20),
        tsl_powerup_count=parse_uint(22),
        mins=parse_uint(24),
        days=parse_ulong(26),
        update_flag=parse_uint(30),
        backup_mins=parse_uint(32),
        backup_days=parse_ulong(34)
    )

def serialize_persistent_data(data: PersistentData) -> bytes:
    """Convert PersistentData back to byte array"""
    result = bytearray(38)  # Total size based on the structure
    
    # Serialize programmed_time (offset 0-6)
    result[0] = data.programmed_time.sec_bcd
    result[1] = data.programmed_time.min_bcd
    result[2] = data.programmed_time.hour_bcd
    result[3] = data.programmed_time.weekday_bcd
    result[4] = data.programmed_time.date_bcd
    result[5] = data.programmed_time.month_bcd
    result[6] = data.programmed_time.year_bcd
    
    # Serialize launched_time (offset 7-13)
    result[7] = data.launched_time.sec_bcd
    result[8] = data.launched_time.min_bcd
    result[9] = data.launched_time.hour_bcd
    result[10] = data.launched_time.weekday_bcd
    result[11] = data.launched_time.date_bcd
    result[12] = data.launched_time.month_bcd
    result[13] = data.launched_time.year_bcd
    
    # Serialize other fields
    result[14:16] = data.initalized_flag.to_bytes(2, byteorder='little')
    result[16:18] = data.commisisoned_flag.to_bytes(2, byteorder='little')
    result[18:20] = data.launched_flag.to_bytes(2, byteorder='little')
    result[20:22] = data.porsoltCount.to_bytes(2, byteorder='little')
    result[22:24] = data.tsl_powerup_count.to_bytes(2, byteorder='little')
    result[24:26] = data.mins.to_bytes(2, byteorder='little')
    result[26:30] = data.days.to_bytes(4, byteorder='little')
    result[30:32] = data.update_flag.to_bytes(2, byteorder='little')
    result[32:34] = data.backup_mins.to_bytes(2, byteorder='little')
    result[34:38] = data.backup_days.to_bytes(4, byteorder='little')
    
    return bytes(result)

def bytes_to_titxt(data: bytes, base_address: int = 0x1800) -> str:
    """Convert bytes to TI TXT format"""
    lines = []
    lines.append(f"@{base_address:04X}")
    
    # Write data in chunks of 16 bytes per line
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        hex_bytes = ' '.join(f"{b:02X}" for b in chunk)
        lines.append(hex_bytes)
    
    lines.append("q")  # End marker
    return '\n'.join(lines)

def calculate_time_since_launch(launched_time: datetime.datetime) -> tuple[int, int]:
    """Calculate days and minutes since launch time to current time"""
    # Use same time function as the programming code - gets UTC time
    now_as_time_type = time.gmtime()    
    # Convert to datetime for easier diff and printing
    now_time = datetime.datetime(*now_as_time_type[:6])  # year, month, day, hour, minute, second
    
    # Calculate next 5-minute boundary
    future_time = now_time + datetime.timedelta(minutes=5)
    future_time = future_time.replace(second=0, microsecond=0)
    future_time = future_time - datetime.timedelta(minutes=future_time.minute % 5)
    
    # Calculate the time difference using the future time
    diff = future_time - launched_time
    
    # Break down the difference into days and minutes
    total_seconds = int(diff.total_seconds())
    days = total_seconds // (24 * 3600)
    remaining_seconds = total_seconds % (24 * 3600)
    minutes = remaining_seconds // 60
    
    return days, minutes

def main():
    """Main function"""
    if len(sys.argv) != 3:
        print("Usage: python normalize.py <input_file> <output_file>")
        print("  input_file:  Path to MSP430 memory dump file (0x1800-0x18ff)")
        print("  output_file: Path to output file with normalized time values")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    
    try:
        # Read input file
        print(f"Reading input file: {input_file}")
        with open(input_file, 'r') as f:
            titxt_data = f.read()
        
        # Decode TI TXT format to bytes
        data_bytes = decode_titxt(titxt_data)
        
        # Parse persistent data structure
        parsed_data = parse_persistent_data(list(data_bytes))
        
        # Get launched time
        launched_time = timeblock_to_datetime(parsed_data.launched_time)
        print(f"Launched time: {launched_time.strftime('%Y-%m-%d %H:%M:%S')}")
        
        # Check if update flag is set
        update_flag_set = parsed_data.update_flag != 0
        print(f"Update flag: {parsed_data.update_flag} ({'SET' if update_flag_set else 'CLEAR'})")
        
        # If update flag is set, use backup values as the current values
        if update_flag_set:
            print("Update flag is set - using backup values as current values")
            current_days = parsed_data.backup_days
            current_minutes = parsed_data.backup_mins
            print(f"  Using backup days: {current_days}")
            print(f"  Using backup minutes: {current_minutes}")
        else:
            current_days = parsed_data.days
            current_minutes = parsed_data.mins
            print(f"  Using primary days: {current_days}")
            print(f"  Using primary minutes: {current_minutes}")
        
        # Calculate current days and minutes since launch
        new_days, new_minutes = calculate_time_since_launch(launched_time)
        
        print(f"Calculated time since launch:")
        print(f"  Days: {new_days}")
        print(f"  Minutes: {new_minutes}")
        print(f"  Original days: {current_days}")
        print(f"  Original minutes: {current_minutes}")
        
        # Update the data with new values
        parsed_data.mins = new_minutes
        parsed_data.days = new_days
        parsed_data.backup_mins = new_minutes
        parsed_data.backup_days = new_days
        
        # Serialize back to bytes
        updated_bytes = serialize_persistent_data(parsed_data)
        
        # Convert to TI TXT format
        output_titxt = bytes_to_titxt(updated_bytes)
        
        # Write output file
        print(f"Writing output file: {output_file}")
        with open(output_file, 'w') as f:
            f.write(output_titxt)
        
        print("Normalization complete!")
        
    except FileNotFoundError:
        print(f"Error: Input file '{input_file}' not found")
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
