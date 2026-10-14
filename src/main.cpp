#include <Arduino.h>

void setup()
{
    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println("================================");
    Serial.println("       HinterHalt Website");
    Serial.println("================================");
    Serial.println("Öffne index.html im Browser");
    Serial.println("BLE über Web Bluetooth");
}

void loop()
{
    delay(1000);
}