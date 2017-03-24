// Agent code
const EPOCH_TIME_2016 =     1451606400;             // To reduce space use this when calculating time
const HASHKEY =             "--- CREATE YOUR SPECIAL HASH KEY HERE ---";

const USRBTNRESPONSETIME =  90;                     // 90 seconds

local devCheck = 0;
local devConnected = false;

local DTbl = {
    BNB = {
        bref = "--- a booking reference goes here ---",
        eml = "--- a booking email address goes here ---",
        nme = "--- a booking name goes here ---",
        DTvalidfrom = 36428400,                     // UTC time for when booking starts (note that I subtract EPOCH_TIME_2016 here)
        DTvalidto = 40351600,                       // UTC time for when booking ends
        DTsignin = 0,
        token = "",
        tokConfirm = 0,                             // User system must confirm they have the token for it to stick
        DTactive = 0                                // DateTime of Active User
    },
    Door = {
        RebootDT = 0,
        LastCommsDT = 0,
        OutBtn = 0,
        OutBtnToken = "",
        OutBtnTimer = null,                         // used as a failsafe to ensure OutBtn returns to zero state
        InBtn = 0,
        Relay = 0,
        RelayTimer = null,                         // used as a failsafe to ensure Relay returns to zero state
        Buzzer = 0,
        Angle = 0
    }
}

/* STATIC WEB PAGE -----------------------------------------------------*/

function LandingPage()
{
    local html =@"<!doctype html> --- INSERT SOME HTML CODE HERE IF YOU WANT TO HAVE AN ADMIN PAGE (NOT USED IN HACKSTER EXAMPLE)</html>";
    return html;
}


/* HTTP HANDLER -----------------------------------------------------*/

function httpHandler(request,response) {
    try 
    {
        local urlPath = split(http.agenturl(), "/");
        local pathField = "path=/" + urlPath[2];
        local method = request.method.toupper();
        local reqpath = split(request.path, "/");

        response.header("Access-Control-Allow-Origin", " ---- IMPORTANT ------ LOCK THIS DOWN BY INSERTING WEB PAGE DOMAIN NAME HERE INSTEAD OF USING *");
        response.header("Access-Control-Request-Headers", "X-Requested-With, Content-Type");
        response.header("Access-Control-Allow-Methods", "GET, POST");
        if (reqpath.len()) {
            server.log("http path: "+ reqpath[0]);
            if (method == "GET") {
                if (reqpath[0] == "valbooking") {
                    if (reqpath.len() > 1) {
                        if (reqpath[1] == DTbl.BNB.token) {
                            if (!DTbl.BNB.tokConfirm) DTbl.BNB.tokConfirm = 1;            // Confirm token to lock down
                            DTbl.BNB.DTactive = time() - EPOCH_TIME_2016;                       // Set up time user active as pressed button on phone
                            // return valid token message
                            local htmlTable = {USRv=1, USRd=0, USRr=0, USRm="valid token"};            // 1 = valid
                            local jvars = http.jsonencode(htmlTable);
                            response.send(200, jvars);
                        }
                        else {
                            //return invalid token message
                            local htmlTable = {USRv=2, USRd=0, USRr=0, USRm="invalid token"};            // 2 = invalid token
                            local jvars = http.jsonencode(htmlTable);
                            response.send(200, jvars);
                        }
                        device.send("NightLightReq", 1);
                        return;
                    }
                }
                else if (reqpath[0] == "chkbutton") {
                    if (reqpath.len() > 1) {
                        if (reqpath[1] == DTbl.BNB.token) {
                            DTbl.BNB.DTactive = time() - EPOCH_TIME_2016;
                            if (!DTbl.BNB.tokConfirm) DTbl.BNB.tokConfirm = 1;            // Confirm token to lock down
                            // return valid token message
                            local htmlTable = {USRv=1, USRd=DTbl.Door.OutBtn, USRr=DTbl.Door.Relay, USRm=DTbl.Door.OutBtnToken};            // 1 = valid
                            local jvars = http.jsonencode(htmlTable);
                            response.send(200, jvars);
                        }
                        else {
                            //return invalid token message
                            local htmlTable = {USRv=2, USRd=0, USRr=0, USRm="invalid token"};            // 2 = invalid token
                            local jvars = http.jsonencode(htmlTable);
                            response.send(200, jvars);
                        }
                        return;
                    }
                }
                else if (reqpath[0] == "chkthedoor") {
                    local htmlTable = null;
                    if (DTbl.Door.Relay && DTbl.Door.Angle <=1)
                        htmlTable = {USRv=1, USRd=DTbl.Door.Angle, USRr=1, USRm="Please open the door"};
                    else if (DTbl.Door.Relay && DTbl.Door.Angle > 1)
                        htmlTable = {USRv=1, USRd=DTbl.Door.Angle, USRr=2, USRm="Please enter and remember to close door"};

                    if (DTbl.Door.Buzzer && DTbl.Door.Angle > 1)
                        htmlTable = {USRv=1, USRd=DTbl.Door.Angle, USRr=3, USRm="Please close door as soon as you can"};

                    if (!DTbl.Door.Relay && !DTbl.Door.Buzzer && DTbl.Door.Angle <= 1)
                        htmlTable = {USRv=1, USRd=DTbl.Door.Angle, USRr=4, USRm="Thank you for closing the door"};

                    local jvars = http.jsonencode(htmlTable);
                    response.send(200, jvars);
                    return;
                }

                response.send(200, "OK");
            }
            else if (method == "POST") {
                if (reqpath[0] == "confirmbooking") {
                    local brefdata = request.body;              // This is one long text string. Includes booking ref and encoded email address
                    // See if there is a match with booking reference
                    if (DTbl.BNB.bref == brefdata.slice(0,DTbl.BNB.bref.len()).toupper()) {
                        //decode email address
                        brefdata = brefdata.slice(DTbl.BNB.bref.len());
                        local emldec = "";
                        for (local i = 0; i < brefdata.len(); i += 2) {
                            emldec += hexStringToInt(brefdata.slice(i,i+2)).tochar();
                        }
                        if (DTbl.BNB.eml == emldec.tolower()) {
                            // match found -- check dates and times of request against booking ref
                            local regTime = time() - EPOCH_TIME_2016;
                            if (regTime >= DTbl.BNB.DTvalidfrom && regTime <= DTbl.BNB.DTvalidto) {
                                // Check that no token is set and confirmed
                                if (DTbl.BNB.tokConfirm) {
                                    // A token has already been confirmed by a user. So cannot reissue
                                    response.send(200, "OK");
                                }
                                else {
                                    // Register the registration time
                                    DTbl.BNB.DTsignin = regTime;
                                    // Create Token
                                    local newSignature = DTbl.BNB.eml+DTbl.BNB.DTvalidfrom+DTbl.BNB.DTsignin;
                                    local newToken = http.base64encode(http.hash.hmacsha256(newSignature, HASHKEY+DTbl.BNB.bref+DTbl.BNB.nme));
                                    //strip out bad characters
                                    local subTokens = split(newToken, "=?/&");
                                    newToken = subTokens[0];
                                    if (subTokens.len() > 1) {
                                        for (local i = 1; i < subTokens.len(); i++) {
                                            newToken += "x"+ subTokens[i];
                                        }
                                    }
                                    DTbl.BNB.token = newToken;
                                    // now respond back with token
                                    local htmlTable = {USRt=DTbl.BNB.token, USRm=""};
                                    local jvars = http.jsonencode(htmlTable);
                                    response.send(200, jvars);
                                }
                            } else {
                                // send message to say outside time
                                local htmlTable = {USRt = 0, USRm="Sorry Booking is out of Date."};
                                local jvars = http.jsonencode(htmlTable);
                                response.send(200, jvars);
                            }
                        }
                        else {
                            // send message to say bad data provided
                            local htmlTable = {USRt = 0, USRm="Sorry incorrect email"};
                            local jvars = http.jsonencode(htmlTable);
                            response.send(200, jvars);
                        }
                    }
                    else {
                        // send message to say bad data provided
                        local htmlTable = {USRt = 0, USRm="Sorry incorrect booking ref"};
                        local jvars = http.jsonencode(htmlTable);
                        response.send(200, jvars);
                    }
                    device.send("NightLightReq", 1);
                    return;
                }
                else if (reqpath[0] == "unlockrequest") {
                    local brefdata = request.body;              // This is one long text string. Includes booking ref and encoded email address
                    if (DTbl.Door.OutBtnToken == brefdata) {
                        // Send unlock request back to device
                        device.send("unlockDoor", 1);
                        DTbl.Door.Relay = 1;                    // 1 = instruction sent. 2 = instruction confirmed
                        //clear the once off unlock tokem
                        DTbl.Door.OutBtnToken = "";
                        //clear the user active time
                        DTbl.BNB.DTactive = 0;
                        local htmlTable = {USRv=1, USRd=DTbl.Door.OutBtn, USRr=DTbl.Door.Relay, USRm=DTbl.Door.OutBtnToken};            // 1 = valid
                        local jvars = http.jsonencode(htmlTable);
                        response.send(200, jvars);
                        
                    }
                    else response.send(200, "OK");
                }
            }
        }
        else {
            response.send(200, LandingPage());
        }
    }
    catch(error)
    {
        response.send(500, "Internal Server Error: " + error)
    }    
}


/* CALLBACK HANDLERS ------------------------------------------------*/

function doorBtnPressHandler(data) {
    if (!data.btn) {
        if (!data.bStat) {
            // Relay is now off
            DTbl.Door.OutBtn = data.bStat;
            DTbl.Door.InBtn = data.bStat;
            DTbl.Door.Relay = data.bStat;
        }
    }
    else if (data.btn == 1) {
        if (DTbl.BNB.DTactive) {
            local TimeDiff = time() - EPOCH_TIME_2016 - DTbl.BNB.DTactive;
            device.send("BookingName",DTbl.BNB.nme);
        }
        DTbl.Door.OutBtn = data.bStat;
        if (DTbl.Door.OutBtn) {
            // Set up a new token
            local newSignature = DTbl.BNB.bref+(time() - EPOCH_TIME_2016).tostring();
            local btnhash = BlobToHexString(http.hash.md5(newSignature));
            DTbl.Door.OutBtnToken = btnhash;
        }
        else {
            
        }
    }
    else if (data.btn == 2) {
        if (data.bStat) {
            DTbl.Door.InBtn = 1;
            DTbl.Door.Relay = 2;
        }
    }
}

function doorMonitorHandler(data) {
    // Update Server Table
    if (data.PwrOn) DTbl.Door.LastCommsDT = time() - EPOCH_TIME_2016;
    
    // Outdoor button status
    if (!DTbl.Door.OutBtn && data.BtnOutDoor) {
        DTbl.Door.OutBtn = data.BtnOutDoor;
        if (DTbl.BNB.DTactive) {
            local TimeDiff = time() - EPOCH_TIME_2016 - DTbl.BNB.DTactive;
            device.send("BookingName",DTbl.BNB.nme);
        }
        if (!DTbl.Door.OutBtnToken) {
            // Set up a new token
            local newSignature = DTbl.BNB.bref+(time() - EPOCH_TIME_2016).tostring();
            local btnhash = BlobToHexString(http.hash.md5(newSignature));
            DTbl.Door.OutBtnToken = btnhash;
        }
        // Chk the door button status and extend timer (to prevent timer expiring before user responds)
        if (DTbl.Door.OutBtnTimer) {
            imp.cancelwakeup(DTbl.Door.OutBtnTimer);
            DTbl.Door.OutBtnTimer = null;
        }
        DTbl.Door.OutBtnTimer = imp.wakeup(USRBTNRESPONSETIME, function() {
            DTbl.Door.OutBtn = 0;
            DTbl.Door.OutBtnToken = "";
            DTbl.Door.OutBtnTimer = null;
        });

    }
    else if (DTbl.Door.OutBtn && !data.BtnOutDoor) {
        DTbl.Door.OutBtn = data.BtnOutDoor;
        DTbl.Door.OutBtnToken = "";
        if (DTbl.Door.OutBtnTimer) {
            imp.cancelwakeup(DTbl.Door.OutBtnTimer);
            DTbl.Door.OutBtnTimer = null;
        }
    }
    
    // Indoor button status
    if (!DTbl.Door.InBtn && data.BtnInDoor) {
        DTbl.Door.InBtn = data.BtnInDoor;
    }
    else if (DTbl.Door.InBtn && !data.BtnInDoor) {
        DTbl.Door.InBtn = data.BtnInDoor;
    }
    
    // Relay Activation status
    if (!DTbl.Door.Relay && data.RelayActive) {
        DTbl.Door.Relay = data.RelayActive;
    }
    else if (DTbl.Door.Relay && !data.RelayActive) {
        DTbl.Door.Relay = data.RelayActive;
        if (DTbl.Door.RelayTimer) {
            imp.cancelwakeup(DTbl.Door.RelayTimer);
            DTbl.Door.RelayTimer = null;
        }
    }

    // Buzzer Activation status
    if (!DTbl.Door.Buzzer && data.Buzzer) {
        DTbl.Door.Buzzer = data.Buzzer;
    }
    else if (DTbl.Door.Buzzer && !data.Buzzer) {
        DTbl.Door.Buzzer = data.Buzzer;
    }

    // Door Angle status
    if (DTbl.Door.Angle != data.DoorAngle) {
        DTbl.Door.Angle = data.DoorAngle;
    }
}

function GSRebootedHandler(data) {
    DTbl.Door.RebootDT = time() - EPOCH_TIME_2016;
    server.log("GS Rebooted");
}

function InfoReqHandler(data) {
    local TimeDiff = time() - EPOCH_TIME_2016 - DTbl.BNB.DTactive;
}

/* UTILITY FUNCTIONS ------------------------------------------------*/

function hexStringToInt(hexString) {
    // Does the string start with '0x'? If so, remove it
    if (hexString.slice(0, 2) == "0x") hexString = hexString.slice(2);

    // Get the integer value of the remaining string
    local intValue = 0;

    foreach (character in hexString) {
        local nibble = character - '0';
        if (nibble > 9) nibble = ((nibble & 0x1F) - 7);
        intValue = (intValue << 4) + nibble;
    }

    return intValue;
}

function BlobToHexString(data) {
  local str = "";
  foreach (b in data) str += format("%02X", b);
  return str;
}


/* REGISTER HTTP HANDLER ------------------------------------------------*/

http.onrequest(httpHandler);

device.on("DoorMonitor", doorMonitorHandler);
device.on("BtnPress", doorBtnPressHandler);
device.on("GSRebooted", GSRebootedHandler);
