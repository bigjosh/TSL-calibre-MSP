"""
Utility to compute times in BCD format.
Written mostly by ChatGPT4!
"""

from datetime import datetime, timedelta

def int_to_bcd(x):
    """
    Convert an integer to its Binary Coded Decimal (BCD) equivalent.

    :param x: int, an integer number (usually representing time) to be converted.
    :return: int, the BCD equivalent of the input number.
    """
    return (x // 10) * 16 + (x % 10)

def datetime_to_bcd(dt):
    """
    Convert the individual components of a datetime object (year, month, day, hour, minute, second)
    into their BCD equivalents and package them into a dictionary.

    :param dt: datetime, the datetime object containing the date and time to be converted.
    :return: dict, a dictionary containing the BCD representations of the datetime components.
    """
    sec_bcd = int_to_bcd(dt.second)
    min_bcd = int_to_bcd(dt.minute)
    hour_bcd = int_to_bcd(dt.hour)
    weekday_bcd = int_to_bcd(dt.isoweekday())  # ISO weekday (1=Monday, 7=Sunday)
    date_bcd = int_to_bcd(dt.day)
    month_bcd = int_to_bcd(dt.month)
    year_bcd = int_to_bcd(dt.year % 100)  # Only the last two digits of the year

    return {
        'sec_bcd': sec_bcd,
        'min_bcd': min_bcd,
        'hour_bcd': hour_bcd,
        'weekday_bcd': weekday_bcd,
        'date_bcd': date_bcd,
        'month_bcd': month_bcd,
        'year_bcd': year_bcd,
    }

def datetime_to_bcds(dt):

    # Convert the datetime to BCD format components
    bcd_time = datetime_to_bcd(dt)

    bcds = (
        f"    .sec_bcd = 0x{bcd_time['sec_bcd']:02X},\n"
        f"    .min_bcd = 0x{bcd_time['min_bcd']:02X},\n"
        f"    .hour_bcd = 0x{bcd_time['hour_bcd']:02X},\n"
        f"    .weekday_bcd = 0x{bcd_time['weekday_bcd']:02X},\n"
        f"    .date_bcd = 0x{bcd_time['date_bcd']:02X},\n"
        f"    .month_bcd = 0x{bcd_time['month_bcd']:02X},\n"
        f"    .year_bcd = 0x{bcd_time['year_bcd']:02X},\n"
    )    

    return bcds

def time_passed_since( past_input_date_str, now_input_date_str, date_format):
    try:
        # Convert string to datetime object
        past_input_date = datetime.strptime( past_input_date_str, date_format)
    except ValueError as e:
        print(f"Error: {e}")
        return
    
    try:
        # Convert string to datetime object
        now_input_date = datetime.strptime( now_input_date_str, date_format)
    except ValueError as e:
        print(f"Error: {e}")
        return


    # Current date and time
    # Calculating the difference between now and the input date
    time_difference = now_input_date - past_input_date

    # Getting the total number of seconds, including days in seconds
    total_seconds = time_difference.total_seconds()

    return total_seconds

def main():
    # Input from the user

    # Expected format
    date_format = "%Y-%m-%d %H:%M:%S"

    past_date_str = input("Enter the past date and time (YYYY-MM-DD HH:MM:SS): ")

    now_date_str = input("Enter the now date and time (YYYY-MM-DD HH:MM:SS): ")

    # Calculating the time passed since the input date and time
    total_seconds = time_passed_since( past_date_str, now_date_str, date_format)

    if total_seconds is not None:
        # Displaying the result
        print(f"Time passed : {total_seconds} seconds.")


    rtc_start_time = datetime.strptime( "2000-01-01 00:00:00" , "%Y-%m-%d %H:%M:%S")

    rtc_set_time = rtc_start_time + timedelta(seconds=total_seconds)

    print( datetime_to_bcds( rtc_set_time) )

if __name__ == "__main__":
    main()