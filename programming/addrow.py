import json
import urllib.request
import urllib.error

import keyring
import uuid

# takes a list of 5 values

def send_data_to_sheet(data):

    # Get the URL of the webapp to post our data to.
    # This is created by entering a webapp into the google spreadsheet.
    # It is then stored it keyring use a command line like...
    # `keyring set "tsl-programmer" "addrow-url"`
    # We keep this in keyring becuase anyone who has this URL can post to
    # our spreadhseet so we keep it a secret.

    url = keyring.get_password("tsl-programmer", "addrow-url")
    
    # Convert data to JSON
    data = json.dumps(data).encode('utf-8')
    
    # Create the request
    req = urllib.request.Request(url, data=data, method='POST')
    req.add_header('Content-Type', 'application/json')
    
    # note we do not try to catch any expection, better to let them terminate us. 

    print("Making URL request...")

    # Send the request and get the response
    with urllib.request.urlopen(req) as response:
        result = response.read().decode('utf-8')

        resultJSON = json.loads(result)

        message = resultJSON['message']

        if (message != "Data appended successfully"):
            raise Exception("Unexpected response adding row to log spreadsheet:"+message)
        

def add_to_log(serialno,firmware_hash,device_uuid):

    #save the MAC of this machine so we can trace where it was programmed
    mac_id =  format( uuid.getnode() , "x")


    send_data_to_sheet( [serialno,firmware_hash,device_uuid,mac_id ] )       

if __name__ == "__main__" :

    # Example usage
    test_data = [
        "test1",
        "test2",
        "test3",
        "test3",
        "test4"
    ]

    send_data_to_sheet(test_data)