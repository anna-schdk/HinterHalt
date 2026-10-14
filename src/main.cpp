#include <Arduino.h>

#define RADAR_RX D6
#define RADAR_TX D7

HardwareSerial RadarSerial(1);

String currentRawData = "Warten auf Radar-Daten...";
String currentDistance = "--";
String currentWarning = "0";

void printHexFrame(const uint8_t *buffer, int frameLength)
{
    String rawFrame = "";
    for (int i = 0; i < frameLength; i++)
    {
        if (i > 0)
        {
            rawFrame += " ";
        }

        char hex[4];
        snprintf(hex, sizeof(hex), "%02X", buffer[i]);
        rawFrame += hex;
    }

    currentRawData = rawFrame;

    Serial.println();
    Serial.println("=== RADAR FRAME ===");
    Serial.println(rawFrame);
}

void processRadarFrame(uint8_t *buffer, int frameLength)
{
    if (frameLength < 12)
    {
        return;
    }

    printHexFrame(buffer, frameLength);

    int end = frameLength - 4;
    if (
        buffer[end] != 0xF8 ||
        buffer[end + 1] != 0xF7 ||
        buffer[end + 2] != 0xF6 ||
        buffer[end + 3] != 0xF5
    )
    {
        Serial.println("Footer nicht korrekt.");
        return;
    }

    uint8_t alarm = buffer[6];
    uint8_t targetCount = buffer[7];

    Serial.print("Alarm: ");
    Serial.println(alarm);
    Serial.print("Ziele: ");
    Serial.println(targetCount);

    if (targetCount == 0)
    {
        currentWarning = "0";
        currentDistance = "--";
        Serial.println("Kein Fahrzeug erkannt.");
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

        if (direction == 1 && speed >= 30 && distance <= 50)
        {
            dangerousApproach = true;
        }
    }

    if (dangerousApproach)
    {
        currentWarning = "1";
        Serial.println("!!! WARNUNG !!!");
    }
    else
    {
        currentWarning = "0";
        Serial.println("Keine kritische Annäherung.");
    }

    if (nearestDistance < 999.0)
    {
        currentDistance = String(nearestDistance, 2);
        Serial.print("Naechster Abstand: ");
        Serial.print(currentDistance);
        Serial.println(" m");
    }
    else
    {
        currentDistance = "--";
    }
}

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

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("================================");
    Serial.println("     RADAR SERIAL DEBUG");
    Serial.println("================================");

    RadarSerial.begin(115200, SERIAL_8N1, RADAR_RX, RADAR_TX);
    Serial.println("Radar UART gestartet");
    Serial.println("RX = D6");
    Serial.println("TX = D7");
    Serial.println("Warte auf Radar-Daten...");
}

void loop()
{
    readRadar();
    delay(10);
}
