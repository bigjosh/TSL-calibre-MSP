#!/usr/bin/env python3
"""
read_tsl_fram.py – Dump and interpret the “information FRAM” of a TSL.

▪ Requires MSP430Flasher (in PATH) and access to the EZ-FET.
▪ Tested with Python 3.8+ on macOS, Linux and Windows.
"""

import subprocess, tempfile, os, time, datetime, struct, sys
from pathlib import Path

# ---------------------------------------------------------------------------
# Helpers ­– TI-TXT decode & BCD ↔ int
# ---------------------------------------------------------------------------

def decode_titxt(txt: str) -> bytes:
    """Convert the data fields of a TI-TXT file into raw bytes."""
    data_bytes = bytearray()
    for line in txt.splitlines():
        line = line.strip()
        if not line or line.startswith('@') or line.startswith('q'):
            continue
        # each 'word' is one byte in hex
        for token in line.split():
            data_bytes.append(int(token, 16))
    return bytes(data_bytes)

def bcd_to_int(b: int) -> int:
    return ((b >> 4) * 10) + (b & 0x0F)

def unpack_u16_le(buf, off):   # little-endian 16-bit
    return struct.unpack_from('<H', buf, off)[0]

def unpack_u32_le(buf, off):   # little-endian 32-bit
    return struct.unpack_from('<I', buf, off)[0]

# ---------------------------------------------------------------------------
# Low-level FRAM read
# ---------------------------------------------------------------------------

def read_fram(start: int, end: int, mspflasher='MSP430Flasher') -> bytes:
    """
    Read FRAM from start..end (inclusive) and return it as bytes.
    Uses a temporary TI-TXT file produced by MSP430Flasher.
    """
    with tempfile.TemporaryDirectory() as tmp:
        dump = Path(tmp) / 'fram.txt'
        cmd = [
            mspflasher, '-j', 'fast',
            '-r', f'[{dump},{hex(start)}-{hex(end)}]',
            '-z', '[VCC]',
        ]
        print('→ Running:', ' '.join(cmd))
        subprocess.run(cmd, check=True)
        data = decode_titxt(dump.read_text())
        if len(data) < (end - start + 1):
            raise RuntimeError('FRAM read shorter than requested range')
        return data

# ---------------------------------------------------------------------------
# Data-structure offsets  (all relative to 0x1800)
# ---------------------------------------------------------------------------

OFF_COMMISSION  = 0              # 7 bytes   rv3032_time_block_t
OFF_PERSIST     = 7              # struct persistent_data_t begins here

# field offsets inside persistent_data
OFF_MINS            = OFF_PERSIST + 0                       # uint16
OFF_DAYS            = OFF_PERSIST + 2                       # uint32
OFF_BACKUP_MINS     = OFF_PERSIST + 6                       # uint16
OFF_BACKUP_DAYS     = OFF_PERSIST + 8                       # uint32
OFF_UPDATE_FLAG     = OFF_PERSIST + 12                      # uint8
OFF_LAUNCHED_FLAG   = OFF_PERSIST + 13                      # uint8
OFF_COMMISSIONED    = OFF_PERSIST + 14                      # uint8
OFF_INITIALIZED     = OFF_PERSIST + 15                      # uint8
OFF_LAUNCHED_TIME   = OFF_PERSIST + 16                      # 7 bytes
OFF_PROGRAM_TIME    = OFF_PERSIST + 23                      # 7 bytes
OFF_POWERUPS        = OFF_PERSIST + 30                      # uint16
OFF_PORSOLT         = OFF_PERSIST + 32                      # uint16

STRUCT_SIZE = 34   # we’ll read a bit beyond for safety

# ---------------------------------------------------------------------------
# High-level decode
# ---------------------------------------------------------------------------

def parse_time_block(buf, off):
    sec, minute, hour, wday, mday, mon, year = [bcd_to_int(x) for x in buf[off:off+7]]
    year += 2000
    return datetime.datetime(year, mon, mday, hour, minute, sec)

def main():
    # Allow MSP430Flasher override
    flasher = os.getenv('MSP430FLASHER', 'MSP430Flasher')

    fram = read_fram(0x1800, 0x184F, flasher)

    print('\n=== RAW FRAM HEX DUMP (0x1800-0x184F) ===')
    for i in range(0, len(fram), 16):
        chunk = fram[i:i+16]
        print(f'0x{0x1800+i:04X}:', ' '.join(f'{b:02X}' for b in chunk))

    print('\n=== DECODED DATA ===')

    # Commissioning timestamp
    commission_dt = parse_time_block(fram, OFF_COMMISSION)
    print('Commissioned (factory program time):', commission_dt.isoformat())

    # Persistent vars
    mins            = unpack_u16_le(fram, OFF_MINS)
    days            = unpack_u32_le(fram, OFF_DAYS)
    backup_mins     = unpack_u16_le(fram, OFF_BACKUP_MINS)
    backup_days     = unpack_u32_le(fram, OFF_BACKUP_DAYS)
    update_flag     = fram[OFF_UPDATE_FLAG]
    launched_flag   = fram[OFF_LAUNCHED_FLAG]
    commissioned_f  = fram[OFF_COMMISSIONED]
    initialized_f   = fram[OFF_INITIALIZED]
    launched_time   = parse_time_block(fram, OFF_LAUNCHED_TIME)
    programmed_time = parse_time_block(fram, OFF_PROGRAM_TIME)
    powerups        = unpack_u16_le(fram, OFF_POWERUPS)
    porsolt         = unpack_u16_le(fram, OFF_PORSOLT)

    print(f'\nPersistent fields @0x1807:')
    print(f'  mins                = {mins}')
    print(f'  days                = {days}')
    print(f'  backup_mins         = {backup_mins}')
    print(f'  backup_days         = {backup_days}')
    print(f'  update_flag         = 0x{update_flag:02X}')
    print(f'  launched_flag       = 0x{launched_flag:02X}')
    print(f'  commissioned_flag   = 0x{commissioned_f:02X}')
    print(f'  initialized_flag    = 0x{initialized_f:02X}')
    print(f'  launched_time       = {launched_time.isoformat()}')
    print(f'  programmed_time     = {programmed_time.isoformat()}')
    print(f'  tsl_powerup_count   = {powerups}')
    print(f'  porsoltCount        = {porsolt}')

    # -------------------------------------------------------------------
    # Extra calculations when already triggered
    # -------------------------------------------------------------------
    if launched_flag == 0x01:
        now = datetime.datetime.now(datetime.timezone.utc).astimezone()
        elapsed = now - launched_time

        total_minutes = int(elapsed.total_seconds() // 60)
        delta_days, delta_minutes = divmod(total_minutes, 24*60)

        expected_days  = days  + delta_days
        expected_mins  = mins  + delta_minutes
        if expected_mins >= 24*60:
            expected_days += 1
            expected_mins -= 24*60

        print('\n*** This TSL HAS BEEN TRIGGERED ***')
        print('Launch (trigger pulled) time :', launched_time.isoformat())
        print('Elapsed since launch         :', elapsed)
        print('Projected persistent counters if still running now:')
        print(f'  → mins would be : {expected_mins}')
        print(f'  → days would be : {expected_days}')
    else:
        print('\n*** This TSL has NOT been triggered yet ***')

if __name__ == '__main__':
    try:
        main()
    except subprocess.CalledProcessError as e:
        sys.stderr.write(f'\nERROR: MSP430Flasher failed (return {e.returncode}).\n')
        sys.exit(1)
