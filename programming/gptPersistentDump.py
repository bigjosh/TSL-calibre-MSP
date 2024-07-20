import struct

def parse_data(byte_array):
    # Define the format for the time block and the persistent data
    time_block_format = '6B'
    persistent_data_format = time_block_format * 2 + 'HHHHHLHLHHHL'
    
    # Unpack the byte array according to the format
    unpacked_data = struct.unpack(persistent_data_format, byte_array)
    
    # Extract the time blocks and the other fields
    programmed_time = unpacked_data[0:7]
    launched_time = unpacked_data[7:14]
    initalized_flag = unpacked_data[14]
    commisisoned_flag = unpacked_data[15]
    launched_flag = unpacked_data[16]
    tsl_powerup_count = unpacked_data[17]
    mins = unpacked_data[18]
    days = unpacked_data[19]
    update_flag = unpacked_data[20]
    backup_mins = unpacked_data[21]
    backup_days = unpacked_data[22]
    
    # Create the data structure
    data = {
        'programmed_time': programmed_time,
        'launched_time': launched_time,
        'initalized_flag': initalized_flag,
        'commisisoned_flag': commisisoned_flag,
        'launched_flag': launched_flag,
        'tsl_powerup_count': tsl_powerup_count,
        'mins': mins,
        'days': days,
        'update_flag': update_flag,
        'backup_mins': backup_mins,
        'backup_days': backup_days,
    }
    
    # Print the field names and values
    print(f"{'Field Name':<20}{'Hex Value':<20}{'Decimal Value':<20}")
    print("="*60)
    for field_name, value in data.items():
        if isinstance(value, tuple):
            value_str = ' '.join(f"{v:02X}" for v in value)
            value_dec = ' '.join(f"{v}" for v in value)
        else:
            value_str = f"{value:02X}"
            value_dec = f"{value}"
        print(f"{field_name:<20}{value_str:<20}{value_dec:<20}")
    
    return data

# Example usage
byte_array = bytes([
    0x12, 0x34, 0x56, 0x78, 0x90, 0xAB, 0xCD,  # programmed_time
    0x12, 0x34, 0x56, 0x78, 0x90, 0xAB, 0xCD,  # launched_time
    0x01, 0x02, 0x03, 0x04,  # initalized_flag, commisisoned_flag, launched_flag, tsl_powerup_count
    0x05, 0x06, 0x07, 0x08,  # mins, days
    0x09, 0x0A,  # update_flag, backup_mins
    0x0B, 0x0C, 0x0D, 0x0E  # backup_days
])

data = parse_data(byte_array)
