import json
import urllib.request
import urllib.error

import keyring
import uuid

# takes the google appscript URL and a list of values

def send_data_to_sheet(url,data):
    
    # Convert data to JSON string first
    json_str = json.dumps(data)
    # Then encode for HTTP request
    json_data = json_str.encode('utf-8')
    
    # Create the request
    req = urllib.request.Request(url, data=json_data, method='POST')
    req.add_header('Content-Type', 'application/json')
    
    
    success_flag=False

    print("Making URL request...")

    try:

        # Send the request and get the response
        with urllib.request.urlopen(req) as response:
            result = response.read().decode('utf-8')

            resultJSON = json.loads(result)

            message = resultJSON['message']

            if (message == "Data appended successfully"):
                success_flag=True   


    finally:

        if not success_flag:

            # could be a connectivity problem or the appscript choked

            print("Error adding row to log spreadsheet!")

            print("Appending failed row JSON to file log_pending.json so you can try again later...");

            # open file for appending
            with open("log_pending.json", "a") as f:
                f.write(json_str + "\n")  # Add newline for better formatting
        

if __name__ == "__main__" :

    # Example usage
    test_data = [
        "test1",
        "test2",
        "test3",
        "test3",
        "test4"
    ]

    # get URL from command line arguments
    url = sys.argv[1]

    send_data_to_sheet(url,test_data)