#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include <time.h>

#define EEPROM_SIZE 192

#define API_KEY "---"
#define DATABASE_URL "---"

FirebaseConfig config;
FirebaseAuth auth;
FirebaseData fbdo;
FirebaseData fbdoStream;
FirebaseData firebaseHistoryData;

ESP8266WebServer server(80);
DNSServer dnsServer;

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);

bool apMode = true;
bool firebaseReady = false;

String desiredSSID = "";
String desiredPASS = "";

unsigned long wifiConnectStart = 0;
const unsigned long WIFI_TIMEOUT = 15000;

const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

unsigned long logTimer = 0;
unsigned long lastCommandCheck = 0;

// EEPROM adresy
const int DESIRED_SSID_ADDR = 0;
const int DESIRED_PASS_ADDR = 32;

// POMOCNÉ FUNKCIE
void writeEEPROMString(int start, const String &value, int maxLen)
{
    for (int i = 0; i < maxLen; i++)
        EEPROM.write(start + i, i < value.length() ? value[i] : 0);
}
String readEEPROMString(int start, int maxLen)
{
    char buffer[65];
    if (maxLen > 64)
        maxLen = 64;
    for (int i = 0; i < maxLen; i++)
        buffer[i] = EEPROM.read(start + i);
    buffer[maxLen] = '\0';
    String result = String(buffer);
    result.trim();
    return result;
}

void loadWiFi()
{
    desiredSSID = readEEPROMString(DESIRED_SSID_ADDR, 32);
    desiredPASS = readEEPROMString(DESIRED_PASS_ADDR, 32);
}

void initTime()
{
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);
    time_t now = time(nullptr);
    int retries = 0;
    while (now < 100000 && retries < 20)
    {
        delay(300);
        now = time(nullptr);
        retries++;
    }
}

// Vue.js  timestamp v milis
unsigned long long getUnixTimestampMs()
{
    time_t now = time(nullptr);
    if (now < 100000)
        return 0;
    return (unsigned long long)now * 1000ULL;
}

// --- WEB PORTAL PRE WIFI ---
void handleConfigPage()
{
    String page =
        "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<title>ESP WiFi Setup</title></head>"
        "<body style='font-family:Arial;background:#111;color:white;text-align:center;padding-top:40px;'>"
        "<h2>IoT Spotreba - Nastavenie</h2>"
        "<p>Zadaj udaje k domacej WiFi</p>"
        "<form action='/save' method='POST'>"
        "<input name='ssid' placeholder='WiFi Meno (SSID)' style='padding:10px;width:220px;'><br><br>"
        "<input name='pass' type='password' placeholder='Heslo' style='padding:10px;width:220px;'><br><br>"
        "<input type='submit' value='Pripojit' style='padding:10px 20px;'>"
        "</form></body></html>";
    server.send(200, "text/html", page);
}

void handleSave()
{
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    ssid.trim();
    pass.trim();

    writeEEPROMString(DESIRED_SSID_ADDR, ssid, 32);
    writeEEPROMString(DESIRED_PASS_ADDR, pass, 32);
    EEPROM.commit();

    server.send(200, "text/html", "<html><body style='font-family:Arial;background:#111;color:white;text-align:center;padding-top:40px;'><h2>Ulozene. Doska sa pripaja...</h2></body></html>");
    delay(1000);

    ESP.restart();
}

void startAP()
{
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP("ESP_Config", "---");
    dnsServer.start(DNS_PORT, "*", apIP);
    apMode = true;
    firebaseReady = false;

    server.on("/", handleConfigPage);
    server.on("/save", handleSave);
    server.onNotFound(handleConfigPage);
    server.begin();
}

// --- HLAVNÁ LOGIKA ---
void setup()
{
    Serial.begin(9600);

    EEPROM.begin(EEPROM_SIZE);
    loadWiFi();

    if (desiredSSID != "")
    {
        WiFi.mode(WIFI_STA);
        WiFi.begin(desiredSSID.c_str(), desiredPASS.c_str());
        wifiConnectStart = millis();
        apMode = false;
    }
    else
    {
        startAP();
    }
}

void loop()
{
    if (apMode)
    {
        dnsServer.processNextRequest();
        server.handleClient();
    }
    else
    {
        if (WiFi.status() != WL_CONNECTED)
        {
            if (millis() - wifiConnectStart > WIFI_TIMEOUT)
            {
                // Ak sa nepripojí do 15 sekúnd, vytvorí opäť ESP_Config
                startAP();
            }
        }
        else if (!firebaseReady)
        {
            initTime();
            config.api_key = API_KEY;
            config.database_url = DATABASE_URL;
            config.signer.test_mode = true;

            Firebase.begin(&config, &auth);
            Firebase.reconnectWiFi(true);
            firebaseReady = true;
        }
        else
        {
            // 1. ČÍTANIE Z ARDUINA
            if (Serial.available() > 0)
            {
                String line = Serial.readStringUntil('\n');
                if (line.startsWith("DATA:"))
                {
                    line.remove(0, 5);
                    int pipeIdx = line.indexOf('|');
                    String acPart = line.substring(0, pipeIdx);
                    String dcPart = line.substring(pipeIdx + 1);

                    int c1 = acPart.indexOf(',');
                    int c2 = acPart.indexOf(',', c1 + 1);
                    int c3 = acPart.indexOf(',', c2 + 1);
                    float acW = acPart.substring(c2 + 1, c3).toFloat();
                    float acE = acPart.substring(c3 + 1).toFloat();

                    int c4 = dcPart.indexOf(',');
                    int c5 = dcPart.indexOf(',', c4 + 1);
                    float dcW = dcPart.substring(c4 + 1, c5).toFloat();
                    float dcE = dcPart.substring(c5 + 1).toFloat();

                    unsigned long long currentMs = getUnixTimestampMs();

                    Firebase.setFloat(fbdo, "/devices/device1/powerW", dcW);
                    Firebase.setDouble(fbdo, "/devices/device1/updatedAt", currentMs);

                    Firebase.setFloat(fbdo, "/devices/device2/powerW", acW);
                    Firebase.setDouble(fbdo, "/devices/device2/updatedAt", currentMs);

                    // Zápis Logov (Každých 5 minút)
                    if (millis() - logTimer > 300000 || logTimer == 0)
                    {
                        logTimer = millis();

                        FirebaseJson json1;
                        json1.set("kWh", dcE);
                        json1.set("timestamp", currentMs);
                        Firebase.pushJSON(firebaseHistoryData, "/consumptionLogs/device1", json1);

                        FirebaseJson json2;
                        json2.set("kWh", acE);
                        json2.set("timestamp", currentMs);
                        Firebase.pushJSON(firebaseHistoryData, "/consumptionLogs/device2", json2);
                    }
                }
            }

            // 2. ČÍTANIE PRÍKAZOV Z FIREBASE (Každé 2 sekundy)
            if (millis() - lastCommandCheck > 2000)
            {
                lastCommandCheck = millis();

                if (Firebase.getBool(fbdoStream, "/devices/device2/isOn"))
                {
                    bool isOn = fbdoStream.boolData();
                    Serial.println(isOn ? "F:1" : "F:0");
                }

                if (Firebase.getInt(fbdoStream, "/devices/device1/brightness"))
                {
                    int webJas = fbdoStream.intData();
                    if (Firebase.getBool(fbdoStream, "/devices/device1/isOn"))
                    {
                        if (!fbdoStream.boolData())
                            webJas = 0;
                    }
                    Serial.println("L:" + String(webJas));
                }
            }
        }
    }
}