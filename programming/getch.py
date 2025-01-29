import sys
import select
import time

"""returns next character from standard input, or None if no input is available."""
"""It is sad that there is no standard, portable way to do non-blocking key input (or even blocking!) in Python."""

# Try to import Windows-specific modules
try:
    import msvcrt
    _is_windows = True
except ImportError:
    # Unix-specific modules
    import tty
    import termios
    _is_windows = False
    _fd = sys.stdin.fileno()
    _old_settings = termios.tcgetattr(_fd)

def key_available():
    if _is_windows:
        return msvcrt.kbhit()
    else:
        tty.setraw(_fd)
        # Use select to check if input is ready
        _keyready = select.select([sys.stdin], [], [], 0)[0]
        termios.tcsetattr(_fd, termios.TCSADRAIN, _old_settings)
        return _keyready

def getch():
    """
    Waits for key to be pressed. Returns the key.
    """
    if _is_windows:
        ch = msvcrt.getch()
        return ch.decode("utf-8")            
    else:
        try:
            tty.setraw(_fd)
            # Use select to check if input is ready
            if select.select([sys.stdin], [], [], 0)[0]:
                ch = sys.stdin.read(1)
                return ch
        finally:
            termios.tcsetattr(_fd, termios.TCSADRAIN, _old_settings)
        return None

def main():
    print("Press any key to test non-blocking input, or 'q' to quit.")
    print("Running loop (press keys anytime)...")

    while True:
        if (key_available()):
            ch = getch()
            print(f"Key pressed: {ch}")
            if ch.lower() == 'q':
                print("Quitting...")
                break
        # Do other tasks here while waiting for input
        print("Working... (no key yet)")
        time.sleep(0.5)

if __name__ == "__main__":
    main()
