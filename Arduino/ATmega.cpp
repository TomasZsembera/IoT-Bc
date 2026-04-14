#include <SoftwareSerial.h>
#include <PZEM004Tv30.h>

const int ssrPin = 8;
const int mosfetPin = 9;
const int acsPin = A1;

SoftwareSerial pzemSWSerial(10, 11);
PZEM004Tv30 pzem(pzemSWSerial);

unsigned long lastSend = 0;
float dcE_kWh = 0.0;

// Pamäť pre logické poistky
int aktualnyJas = 0;
int stavFan = 0;

void setup()
{
    Serial.begin(9600);
    pinMode(ssrPin, OUTPUT);
    pinMode(mosfetPin, OUTPUT);
    lastSend = millis();
}

void loop()
{
    // Čítanie príkazov z ESP8266
    if (Serial.available() > 0)
    {
        String cmd = Serial.readStringUntil('\n');

        if (cmd.startsWith("F:"))
        {
            stavFan = cmd.substring(2).toInt();
            digitalWrite(ssrPin, stavFan);
        }
        else if (cmd.startsWith("L:"))
        {
            aktualnyJas = cmd.substring(2).toInt();
            int pwmValue = map(aktualnyJas, 0, 100, 0, 255);
            analogWrite(mosfetPin, pwmValue);
        }
    }

    // Odosielanie dát do ESP každé 3 sekundy
    unsigned long currentMillis = millis();
    if (currentMillis - lastSend >= 3000)
    {
        float dtHours = (currentMillis - lastSend) / 3600000.0;
        lastSend = currentMillis;

        // --- AC Data (Ventilátor z PZEM) ---
        float acV = pzem.voltage();
        float acA = pzem.current();
        float acW = pzem.power();
        float acE = pzem.energy();

        // LOGICKÁ POISTKA PRE VENTILÁTOR:
        if (isnan(acV))
            acV = 0;
        if (isnan(acA) || stavFan == 0)
            acA = 0;
        if (isnan(acW) || stavFan == 0)
            acW = 0;
        if (isnan(acE))
            acE = 0;

        // --- DC Data (LED z ACS712) ---
        long rawSum = 0;
        for (int i = 0; i < 40; i++)
        {
            rawSum += analogRead(acsPin);
            delay(1);
        }
        float rawAvg = rawSum / 40.0;

        float dcA = ((rawAvg / 1023.0) * 5.0 - 2.5) / 0.185;

        if (dcA < 0.25)
            dcA = 0;

        // LOGICKÁ POISTKA PRE LED:
        if (aktualnyJas == 0)
            dcA = 0;

        float dcW = dcA * 24.0;
        dcE_kWh += (dcW * dtHours) / 1000.0;

        // Odoslanie do ESP
        Serial.print("DATA:");
        Serial.print(acV);
        Serial.print(",");
        Serial.print(acA);
        Serial.print(",");
        Serial.print(acW);
        Serial.print(",");
        Serial.print(acE);
        Serial.print("|");
        Serial.print(dcA);
        Serial.print(",");
        Serial.print(dcW);
        Serial.print(",");
        Serial.println(dcE_kWh, 6);
    }
}