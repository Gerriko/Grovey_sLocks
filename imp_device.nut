/* Developed by Gerrikoio ------------------------------------------------*/

// UART bus, Trigger and Arduino pins
UART <- hardware.uart57;

const OUTDRBTNTIMEOUT = 90;

GSlocks <- {
    PwrOn =         false,
    BtnOutDoor =    0,
    BtnInDoor =     0,
    RelayActive =   0,
    Buzzer =        0,
    DoorAngle =     0
};

BtnTimer <- null;                       // Handles button timeout to ensure btn outdoor returns to zero

function UARThandler() {
    local b = UART.read();
    local bb = 0;
    imp.sleep(0.01);
    while (b != -1) {
        if (b != 10) bb += b;
        b = UART.read();
    }
    switch (bb) {
        case 0:
            GSlocks.PwrOn = false;
            agent.send("GSRebooted", 1);
            break;
        case 1:
            if (GSlocks.PwrOn) {
                GSlocks.DoorAngle = bb;
                if (GSlocks.Buzzer) GSlocks.Buzzer = 0;
            }
            break;
        case 2:
        case 3:
        case 4:
            if (GSlocks.PwrOn) {
                GSlocks.DoorAngle = bb;
            }
            break;
        case 32:
            if (GSlocks.PwrOn) {
                GSlocks.RelayActive = 1;
            }
            break;
        case 33:
            if (GSlocks.PwrOn) {
                GSlocks.BtnOutDoor = 0;
                GSlocks.BtnInDoor = 0;
                GSlocks.RelayActive = 0;
                if (BtnTimer) {
                    imp.cancelwakeup(BtnTimer);
                    BtnTimer = null;
                }
            }
            break;
        case 34:
            if (GSlocks.PwrOn) {
                GSlocks.Buzzer = 1;
            }
            break;
        case 48:
            if (!GSlocks.PwrOn) {
                GSlocks.PwrOn = true;
            }
            break;
        case 49:
            if (GSlocks.PwrOn) {
                GSlocks.BtnOutDoor = 1;
                if (BtnTimer) {
                    imp.cancelwakeup(BtnTimer);
                    BtnTimer = null;
                }
                BtnTimer = imp.wakeup(OUTDRBTNTIMEOUT, function() {
                    server.log("GS OutDr Btn timer Expire");
                    GSlocks.BtnOutDoor = 0;
                    BtnTimer = null;
                });
            }
            break;
        case 50:
            if (GSlocks.PwrOn) {
                GSlocks.BtnInDoor = 1;
            }
            break;
        case 51:
            if (GSlocks.PwrOn) {
                server.log("GS OutDr Btn Timeout");
                GSlocks.BtnOutDoor = 0;
                if (BtnTimer) {
                    imp.cancelwakeup(BtnTimer);
                    BtnTimer = null;
                }
            }
            break;
        default:
            server.log("UART: " + bb);
    }
    if (bb) {
        if (GSlocks.PwrOn) agent.send("DoorMonitor", GSlocks);
        else GSlocks.PwrOn = true;
    }
}

function agentDoorUnlockHandler(var) {
    // agent request door unlock
    server.log("Sending Unlock Door Request to GS")
    UART.write(0x41);
}

function agentNightLightReqHandler(var) {
    // agent request for night light (if applicable)
    server.log("Sending Nightlight Request to GS")
    UART.write(0x42);
}

function agentBookingNameHandler(var) {
    // agent data for GS
    server.log("Sending Booking Name "+var+" to GS");
    // check string length
    if (var.len() > 16) var = var.slice(0, 16);
    local BookName = var+"\n";
    UART.write(0x43);
    UART.write(BookName);
}

// Hardware Configuration
// ---------------------------------------------------------------------- 
UART.configure(9600, 8, PARITY_NONE, 1, NO_CTSRTS, UARThandler);

// Agent Function call handlers
// ---------------------------------------------------------------------- 
imp.setpowersave(true);

agent.on("NightLightReq", agentNightLightReqHandler);
agent.on("BookingName", agentBookingNameHandler);
agent.on("unlockDoor", agentDoorUnlockHandler);
