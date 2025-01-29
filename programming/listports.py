#!/usr/bin/env python3

"""
Lets the user pick which serial ports to use for the relay and CurrentRanger
"""

import serial
import serial.tools.list_ports
import subprocess
import re

# Field width constants
INDEX_WIDTH = 4
PORT_WIDTH = 12
VID_PID_WIDTH = 10
SN_WIDTH = 20
DESC_WIDTH = 30

def truncate_with_ellipsis(text: str, width: int) -> str:
    """Truncate text to width-3 and add ... if it exceeds width."""
    if len(text) <= width:
        return text
    return text[:width-3] + "..."

def strip_ansi_sequences(text: str) -> str:
    """Remove ANSI escape sequences from text."""
    ansi_escape = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')
    return ansi_escape.sub('', text)

def print_field_with_max_width(text: str, width: int, fill_char: str = " ") -> str:
    """Print a field with consistent width, truncating if necessary.
    Args:
        text: The text to print
        width: Maximum width of the field
        fill_char: Character to use for padding (default space)
    Returns:
        Formatted string with consistent width
    """
    # Clean any potential ANSI sequences from the text
    clean_text = strip_ansi_sequences(str(text))
    truncated = truncate_with_ellipsis(clean_text, width)
    return f"{truncated:{fill_char}<{width}}"

def main():

    """List all available serial ports with their details."""
    ports = serial.tools.list_ports.comports()
    
    if not ports:
        print("No serial ports found")
        return
        
    print("Available ports:")
    # Print header
    header = (
        print_field_with_max_width("#", INDEX_WIDTH) +
        print_field_with_max_width("PORT", PORT_WIDTH) +
        print_field_with_max_width("VID:PID", VID_PID_WIDTH) +
        print_field_with_max_width("SN", SN_WIDTH) +
        print_field_with_max_width("DESCRIPTION", DESC_WIDTH)
    )
    print(header)
    
    # Print separator
    separator = (
        print_field_with_max_width("", INDEX_WIDTH, "=") +
        print_field_with_max_width("", PORT_WIDTH, "=") +
        print_field_with_max_width("", VID_PID_WIDTH, "=") +
        print_field_with_max_width("", SN_WIDTH, "=") +
        print_field_with_max_width("", DESC_WIDTH, "=")
    )
    print(separator)
        
    for i, port in enumerate(ports, 1):
        vid = f"{port.vid:04X}" if port.vid is not None else "None"
        pid = f"{port.pid:04X}" if port.pid is not None else "None"
        vid_pid = f"{vid}:{pid}"
        sn = port.serial_number or "None"
        
        line = (
            print_field_with_max_width(i, INDEX_WIDTH) +
            print_field_with_max_width(port.device, PORT_WIDTH) +
            print_field_with_max_width(vid_pid, VID_PID_WIDTH) +
            print_field_with_max_width(sn, SN_WIDTH) +
            print_field_with_max_width(port.description, DESC_WIDTH)
        )
        print(line)

if __name__ == '__main__':
    main()