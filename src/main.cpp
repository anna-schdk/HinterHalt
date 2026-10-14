#include <Arduino.h>
#include <string>

#include <WiFi.h>
#include <WebServer.h>

// =====================================================
// WLAN
// =====================================================

const char *AP_SSID = "HinterHalt";
const char *AP_PASSWORD = "12345678";

WebServer webServer(80);

String currentDistance = "--";
String currentWarning = "0";
String currentBlink = "0";

// =====================================================
// LED
// =====================================================

#define LED_PIN D1

// Normaler Blinkmodus
#define NORMAL_BLINK_TIME 500

// Aggressives Blinken bei Warnung
#define WARNING_BLINK_TIME 100


// =====================================================
// RADAR
// =====================================================

#define RADAR_RX D6
#define RADAR_TX D7

HardwareSerial RadarSerial(1);


// =====================================================
// WARNPARAMETER
// =====================================================

const int WARNING_SPEED = 30;
const int WARNING_DISTANCE = 50;


// =====================================================
// Zustände
// =====================================================

bool warningActive = false;
bool manualBlink = false;
bool ledState = false;
unsigned long lastLedChange = 0;


// =====================================================
// Forward declarations
// =====================================================

void processRadarFrame(uint8_t *buffer, int frameLength);


// =====================================================
// HTML
// =====================================================

String buildHtmlPage()
{
    return R"HTML(
<!DOCTYPE html>
<html lang="de">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>HinterHalt</title>
  <style>
    * { box-sizing: border-box; }
    body {
      margin: 0;
      font-family: Arial, sans-serif;
      background: #f4f6f8;
      min-height: 100vh;
      display: flex;
      justify-content: center;
      align-items: center;
      color: #222;
    }
    .container {
      width: min(90vw, 480px);
      background: rgba(255,255,255,0.8);
      border-radius: 20px;
      padding: 28px;
      box-shadow: 0 12px 25px rgba(0,0,0,0.10);
      text-align: center;
    }
    h1 { margin: 0 0 12px; font-size: 42px; }
    .subtitle { color: #666; margin-bottom: 20px; }
    button {
      width: 100%;
      padding: 16px;
      border: none;
      border-radius: 12px;
      font-size: 18px;
      font-weight: bold;
      cursor: pointer;
      background: #1d1d1d;
      color: white;
      margin-top: 18px;
    }
    button.active { background: #d60000; }
    .distance-box {
      margin-top: 26px;
      background: #eef2f5;
      border-radius: 16px;
      padding: 22px;
    }
    .distance {
      font-size: 52px;
      font-weight: bold;
      margin-top: 8px;
    }
    #warning {
      display: none;
      margin-top: 24px;
      padding: 18px;
      background: #ffe5e5;
      color: #a30000;
      border: 2px solid #c80000;
      border-radius: 16px;
      font-size: 20px;
      font-weight: bold;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>HinterHalt</h1>
    <div class="subtitle">Abstandswarnsystem</div>

    <div class="distance-box">
      <div>Abstand zum Fahrzeug</div>
      <div class="distance" id="distance">--</div>
      <div>Meter</div>
    </div>

    <div id="warning">⚠️ WARNUNG<br>Fahrzeug nähert sich schnell!</div>

    <button id="blinkButton">LED-Blinkmodus einschalten</button>
  </div>

  <script>
    const distanceEl = document.getElementById('distance');
    const warningEl = document.getElementById('warning');
    const blinkButton = document.getElementById('blinkButton');

    let blinkMode = false;

    async function refreshState() {
      try {
        const res = await fetch('/state');
        const data = await res.json();

        distanceEl.textContent = data.distance;
        warningEl.style.display = data.warning === '1' ? 'block' : 'none';

        blinkMode = data.blink === '1';
        blinkButton.textContent = blinkMode
          ? 'LED-Blinkmodus ausschalten'
          : 'LED-Blinkmodus einschalten';
        blinkButton.classList.toggle('active', blinkMode);
      }
      catch (error) {
        console.log('Refresh error:', error);
      }
    }

    blinkButton.addEventListener('click', async () => {
      const mode = blinkMode ? 'off' : 'on';
      await fetch('/led?mode=' + mode);
      await refreshState();
    });

    setInterval(refreshState, 500);
    refreshState();
  </script>
</body>
</html>
)HTML";
}


// =====================================================
// WLAN / Web Server
// =====================================================

void handleRoot()
{
    webServer.send(200, "text/html", buildHtmlPage());
}

void handleState()
{
    String json = "{\"distance\":\"" + currentDistance + "\",\"warning\":\"" + currentWarning + "\",\"blink\":\"" + currentBlink + "\"}";
    webServer.send(200, "application/json", json);
}

void handleLed()
{
    if (webServer.hasArg("mode"))
    {
        String mode = webServer.arg("mode");
        if (mode == "on")
        {
            manualBlink = true;
            currentBlink = "1";
        }
        else if (mode == "off")
        {
            manualBlink = false;
            currentBlink = "0";
            if (!warningActive)
            {
                ledState = false;
                digitalWrite(LED_PIN, LOW);
            }
        }
    }

    webServer.send(200, "application/json", "{\"status\":\"ok\"}");
}

void setupWiFi()
{
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);

    Serial.println();
    Serial.println("================================");
    Serial.println("WLAN Access Point aktiv");
    Serial.println("================================");
    Serial.print("SSID: ");
    Serial.println(AP_SSID);
    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());
    Serial.println("Öffne im Browser: http://192.168.4.1");

    webServer.on("/", HTTP_GET, handleRoot);
    webServer.on("/state", HTTP_GET, handleState);
    webServer.on("/led", HTTP_GET, handleLed);
    webServer.begin();
}


// =====================================================
// LED Steuerung
// =====================================================

void updateLED()
{
    unsigned long now = millis();

    if (warningActive)
    {
        if (now - lastLedChange >= WARNING_BLINK_TIME)
        {
            lastLedChange = now;
            ledState = !ledState;
            digitalWrite(LED_PIN, ledState ? HIGH : LOW);
        }
        return;
    }

    if (manualBlink)
    {
        if (now - lastLedChange >= NORMAL_BLINK_TIME)
        {
            lastLedChange = now;
            ledState = !ledState;
            digitalWrite(LED_PIN, ledState ? HIGH : LOW);
        }
        return;
    }

    ledState = false;
    digitalWrite(LED_PIN, LOW);
}


// =====================================================
// Radar Daten auswerten
// =====================================================

void readRadar()
{
    static uint8_t buffer[64];
    static int index = 0;

    while (RadarSerial.available())
    {
        uint8_t data = RadarSerial.read();

        if (index == 0)
        {
            if (data == 0xF4)
            {
                buffer[index++] = data;
            }
            continue;
        }

        if (index == 1)
        {
            if (data == 0xF3)
            {
                buffer[index++] = data;
            }
            else
            {
                index = 0;
            }
            continue;
        }

        if (index == 2)
        {
            if (data == 0xF2)
            {
                buffer[index++] = data;
            }
            else
            {
                index = 0;
            }
            continue;
        }

        if (index == 3)
        {
            if (data == 0xF1)
            {
                buffer[index++] = data;
            }
            else
            {
                index = 0;
            }
            continue;
        }

        buffer[index++] = data;

        if (index >= sizeof(buffer))
        {
            index = 0;
            continue;
        }

        if (index >= 6)
        {
            uint16_t dataLength = buffer[4] | (buffer[5] << 8);
            int frameLength = 4 + 2 + dataLength + 4;

            if (index >= frameLength)
            {
                processRadarFrame(buffer, frameLength);
                index = 0;
            }
        }
    }
}


// =====================================================
// Radar Frame verarbeiten
// =====================================================

void processRadarFrame(uint8_t *buffer, int frameLength)
{
    if (frameLength < 12)
    {
        return;
    }

    int end = frameLength - 4;

    if (
        buffer[end] != 0xF8 ||
        buffer[end + 1] != 0xF7 ||
        buffer[end + 2] != 0xF6 ||
        buffer[end + 3] != 0xF5
    )
    {
        return;
    }

    uint8_t alarm = buffer[6];
    uint8_t targetCount = buffer[7];

    Serial.println();
    Serial.println("----- RADAR -----");
    Serial.print("Ziele: ");
    Serial.println(targetCount);

    if (targetCount == 0)
    {
        warningActive = false;
        currentWarning = "0";
        currentDistance = "--";
        Serial.println("Kein Fahrzeug");
        return;
    }

    bool dangerousApproach = false;
    float nearestDistance = 999.0;

    for (int i = 0; i < targetCount; i++)
    {
        int position = 8 + (i * 5);

        if (position + 4 >= frameLength - 4)
        {
            break;
        }

        int angle = (int)buffer[position] - 128;
        int distance = buffer[position + 1];
        uint8_t direction = buffer[position + 2];
        int speed = buffer[position + 3];
        int snr = buffer[position + 4];

        Serial.print("Ziel ");
        Serial.print(i + 1);
        Serial.print(" | Winkel: ");
        Serial.print(angle);
        Serial.print("° | Abstand: ");
        Serial.print(distance);
        Serial.print(" m | Richtung: ");
        Serial.print(direction == 1 ? "ANNAEHERND" : "ENTFERNT");
        Serial.print(" | Geschwindigkeit: ");
        Serial.print(speed);
        Serial.print(" km/h | SNR: ");
        Serial.println(snr);

        if (distance < nearestDistance)
        {
            nearestDistance = distance;
        }

        if (direction == 1 && speed >= WARNING_SPEED && distance <= WARNING_DISTANCE)
        {
            dangerousApproach = true;
        }
    }

    if (dangerousApproach)
    {
        if (!warningActive)
        {
            Serial.println("!!! WARNUNG !!!");
        }
        warningActive = true;
        currentWarning = "1";
    }
    else
    {
        if (warningActive)
        {
            Serial.println("Warnung beendet");
        }
        warningActive = false;
        currentWarning = "0";
    }

    if (nearestDistance < 999)
    {
        currentDistance = String(nearestDistance, 2);
        Serial.print("WEB Abstand: ");
        Serial.print(currentDistance);
        Serial.println(" m");
    }
    else
    {
        currentDistance = "--";
    }
}


// =====================================================
// SETUP
// =====================================================

void setup()
{
    Serial.begin(115200);
    delay(2000);

    Serial.println();
    Serial.println("================================");
    Serial.println("       HINTERHALT");
    Serial.println("================================");

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    RadarSerial.begin(115200, SERIAL_8N1, RADAR_RX, RADAR_TX);
    Serial.println("Radar UART gestartet");
    Serial.println("RX = D6");
    Serial.println("TX = D7");

    setupWiFi();
}


// =====================================================
// LOOP
// =====================================================

void loop()
{
    readRadar();
    updateLED();
    webServer.handleClient();
    delay(10);
}