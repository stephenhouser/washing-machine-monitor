#include <FastLED.h>
#include <M5Stack.h>

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "SPIFFS.h"
#include "AudioFileSourceSPIFFS.h"
#include "AudioFileSourceID3.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

#include "Free_Fonts.h" // Include the header file attached to this sketch

WiFiClient espClient;
PubSubClient client(espClient);

AudioGeneratorMP3 *mp3;
AudioFileSourceSPIFFS *file;
AudioFileSourceID3 *id3;
AudioOutputI2S *out;

// Configure the name and password of the connected wifi and your MQTT Serve host.
const char* ssid = "***REMOVED***";
const char* password = "***REMOVED***";
const char* mqtt_server = "mqtt.***REMOVED***";

// unsigned long lastMsg = 0;
// #define MSG_BUFFER_SIZE	(50)
// char msg[MSG_BUFFER_SIZE];
// int value = 0;

void setupWifi();
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void reConnect();

#define NUM_LEDS 10
CRGB leds[NUM_LEDS];

void setup(void) {
    M5.begin();
    M5.Power.begin();

    SPIFFS.begin();

    setupWifi();
    client.setServer(mqtt_server, 1883);
    client.setCallback(mqtt_callback);

    FastLED.addLeds<NEOPIXEL, 15>(leds, NUM_LEDS);

    out = new AudioOutputI2S(0, 1); // Output to builtInDAC
    out->SetOutputModeMono(true);
}

int8_t getBatteryLevel() {
    Wire.beginTransmission(0x75);
    Wire.write(0x78);
    if (Wire.endTransmission(false) == 0 && Wire.requestFrom(0x75, 1)) {
        switch (Wire.read() & 0xF0) {
        case 0xE0: return 25;
        case 0xC0: return 50;
        case 0x80: return 75;
        case 0x00: return 100;
        default: return 0;
        }
    }
    return -1;
}


void stop_sound() {
    mp3->stop();

    delete mp3;
    mp3 = NULL;
    delete id3;
    id3 = NULL;
    delete file;
    file = NULL;
}

void play_sound(const char *sound) {    
    if (mp3) {
        stop_sound();
    }

    if (sound && strlen(sound)) {
        // M5.Lcd.drawString(sound, 160, 0, GFXFF);
        file = new AudioFileSourceSPIFFS(sound);
        id3 = new AudioFileSourceID3(file);
        mp3 = new AudioGeneratorMP3();
        mp3->begin(id3, out);
    }
}

// void rainbow(loops=120, ms_delay=1, sat=1.0, bri=0.2) {
void rainbow(int loops = 120, int ms_delay = 1, float sat = 1.0,
             float bri = 0.2) {
    for (int pos = 0; pos < loops; pos++) {
        for (int i = 0; i < NUM_LEDS; i++) {
            float dHue = 360.0 / NUM_LEDS * (pos + i);
            float hue = dHue * 360;
            leds[i] = CHSV(hue, sat, bri);
        }

        FastLED.show();
        if (ms_delay) {
            delay(ms_delay);
        }
    }
}

CRGB decode_color_crgb(String color) {
    if (color == "black") return CRGB::Black;
    if (color == "white") return CRGB::White;
    if (color == "red") return CRGB::Red;
    if (color == "green") return CRGB::Green;
    if (color == "blue") return CRGB::Blue;
    if (color == "yellow") return CRGB::Yellow;
    return CRGB::Black;
}

void led_chase(String color) {
    CRGB led_color = decode_color_crgb(color);
    FastLED.setBrightness(128);
    int loops = 20;
    for (int l = 0; l < loops; l++) {
        for (int i = 0; i < NUM_LEDS; i++) {
            leds[i] = led_color;
            FastLED.delay(10);
            if (l != loops - 1) {
                leds[i] = CRGB::Black;
            }
        }
    }
}

int decode_color_tft(String color) {
    if (color == "black") return TFT_BLACK;
    if (color == "white") return TFT_WHITE;
    if (color == "red") return TFT_RED;
    if (color == "green") return TFT_GREEN;
    if (color == "blue") return TFT_BLUE;
    if (color == "yellow") return TFT_YELLOW;
    return TFT_BLACK;
}

String last_title = "";

void mqtt_callback(char* raw_topic, byte* payload, unsigned int length) {
    StaticJsonDocument<256> doc;
    deserializeJson(doc, payload, length);

    String topic = String(raw_topic);
    // {"title":"WASHING","subtitle":"120 W","color":"green","menu":["check",null,"off"]}
    if (topic.equals("m5go/update")) {
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.setTextPadding(10);

        M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Lcd.setFreeFont(FMB24);

        if (doc.containsKey("title")) {
            const char *title = doc["title"];
            if (!last_title.equals(title)) {
                M5.Lcd.clearDisplay();
                M5.Lcd.drawString(title, 160, 60, GFXFF);
                last_title = title;
            }
        }

        if (doc.containsKey("subtitle")) {
            M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
            M5.Lcd.setFreeFont(FMB24);

            M5.Lcd.fillRect(0, 120-20, 320, 40, TFT_BLACK);
            const char *subtitle = doc["subtitle"];
            M5.Lcd.drawString(subtitle, 160, 120, GFXFF);
        } else {
            M5.Lcd.fillRect(0, 120-20, 320, 40, TFT_BLACK);
        }

        if (doc.containsKey("sound")) {
            const char *sound = doc["sound"];
            play_sound(sound);
        }

        if (doc.containsKey("color")) {
            const char *color = doc["color"];
            int tft_color = decode_color_tft(String(color));
            M5.Lcd.fillRect(0, 208, 320, 28, tft_color);
        }

        if (doc.containsKey("menu")) {
            M5.Lcd.setTextColor(TFT_BLACK, TFT_WHITE);
            M5.Lcd.setFreeFont(FMB12);

            JsonArray menu = doc["menu"];
            if (menu[0]) {
                const char *item = menu[0];
                M5.Lcd.drawString(item, 65, 219, GFXFF);
            }
            if (menu[1]) {
                const char *item = menu[1];
                M5.Lcd.drawString(item, 160, 219, GFXFF);
            }
            if (menu[2]) {
                const char *item = menu[2];
                M5.Lcd.drawString(item, 255, 219, GFXFF);
            }
        }

        if (doc.containsKey("color")) {
            const char *color = doc["color"];
            led_chase(color);
        }
    }
}

#define PULSE_TICKS 8192*2
int pulse_delay = PULSE_TICKS;

void loop() {
    if (!client.connected()) {
      reConnect();
    }
    client.loop();

    if (mp3 && mp3->isRunning()) {
        if (!mp3->loop()) {
            stop_sound();
        }
    }

    M5.update();

    if (M5.BtnA.wasPressed()) {
        client.publish("m5go/button", "A");
    }

    if (M5.BtnB.wasPressed()) {
        client.publish("m5go/button", "B");
    }

    if (M5.BtnC.wasPressed()) {
        client.publish("m5go/button", "C");
    }
}

void setupWifi() {
    delay(10);
    M5.Lcd.printf("Connecting to %s",ssid);
    WiFi.mode(WIFI_STA);  //Set the mode to WiFi station mode. 
    WiFi.begin(ssid, password); //Start Wifi connection.

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        M5.Lcd.print(".");
    }

    M5.Lcd.printf("\nSuccess\n");
}

void reConnect() {
    while (!client.connected()) {
        M5.Lcd.print("Attempting MQTT connection...");
        // Create a random client ID.
        String clientId = "M5Go-";
        clientId += String(random(0xffff), HEX);
        // Attempt to connect.  尝试重新连接
        if (client.connect(clientId.c_str())) {
            M5.Lcd.printf("\nSuccess\n");
            // Once connected, publish an announcement to the topic. 
            // client.publish("M5G", "hello world");
            // ... and resubscribe. 
            client.subscribe("m5go/#");
            M5.Lcd.clearDisplay();
        } else {
            M5.Lcd.print("failed, rc=");
            M5.Lcd.print(client.state());
            M5.Lcd.println("try again in 5 seconds");
            delay(5000);
        }
    }
}
// There follows a crude way of flagging that this example sketch needs fonts
// which have not been enbabled in the User_Setup.h file inside the TFT_HX8357
// library.
//
// These lines produce errors during compile time if settings in User_Setup are
// not correct
//
// The error will be "does not name a type" but ignore this and read the text
// between '' it will indicate which font or feature needs to be enabled
//
// Either delete all the following lines if you do not want warnings, or change
// the lines to suit your sketch modifications.

#ifndef LOAD_GLCD
// ERROR_Please_enable_LOAD_GLCD_in_User_Setup
#endif

#ifndef LOAD_GFXFF
ERROR_Please_enable_LOAD_GFXFF_in_User_Setup !
#endif
