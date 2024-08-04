// This code listens for http requests and then adds rows to our spreadsheet when it gets one. 
// This is more controlled than just sharng the sheet to a service account becuase then the caller could also 
// delete or change rows. This way, the caller can only ever add.
// More info at https://wp.josh.com/2014/06/05/tempurature-logging-to-a-google-spreadsheet-with-an-arduino-yun/

function doPost(e) {
  try {
    // Get the active spreadsheet adn use the first sheet in it. 
    // "getActiveSpreadsheet(), getActiveDocument(), getActivePresentation(), and getActiveForm() allow bound scripts to refer to their parent file without referring to the file's ID."
    var sheet = SpreadsheetApp.getActiveSpreadsheet().getSheets()[0];
    
    // Parse the JSON data from the POST request
    var values = JSON.parse(e.postData.contents);
    
    if (!Array.isArray(values) || values.length != 4) {
          // Return an error response if something goes wrong
      return ContentService.createTextOutput(JSON.stringify({
        'status': 'error',
        'message': 'wrong number of params'+e.postData.contents+"<"
      })).setMimeType(ContentService.MimeType.JSON);
    }

    // Create a new Date object for the current time
    var now = new Date();
    
    // Format the date as GMT string
    var timestamp = Utilities.formatDate(now, 'GMT', "yyyy-MM-dd'T'HH:mm:ss'Z'");
    
    // Add the timestamp to the beginning of the values array
    values.unshift(timestamp);    
    
    // Append the values to the sheet
    sheet.appendRow(values);
    
    // Return a success response
    return ContentService.createTextOutput(JSON.stringify({
      'status': 'success',
      'message': 'Data appended successfully'
    })).setMimeType(ContentService.MimeType.JSON);
    
  } catch (error) {
    // Return an error response if something goes wrong
    return ContentService.createTextOutput(JSON.stringify({
      'status': 'error',
      'message': error.toString()
    })).setMimeType(ContentService.MimeType.JSON);
  }
}
