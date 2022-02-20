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

#include "led_animation.h"

#include "secrets.h"

// Configure the name and password of the connected wifi and your MQTT Serve host.
WiFiClient espClient;
PubSubClient client(espClient);

String last_title = "";

AudioGeneratorMP3 *mp3;
AudioFileSourceSPIFFS *file;
AudioFileSourceID3 *id3;
AudioOutputI2S *out;

void setupWifi();
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void reConnect();

void setup(void) {
    M5.begin();
    M5.Power.begin();
    M5.Power.setWakeupButton(BUTTON_A_PIN);
    M5.Lcd.setFreeFont(FMB12);

    SPIFFS.begin();

    setupWifi();
    client.setServer(MQTT_SERVER, 1883);
    client.setCallback(mqtt_callback);

    led_animation_setup();

    out = new AudioOutputI2S(0, 1); // Output to builtInDAC
    out->SetOutputModeMono(true);
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

int decode_color_tft(String color) {
    if (color == "black") return TFT_BLACK;
    if (color == "white") return TFT_WHITE;
    if (color == "red") return TFT_RED;
    if (color == "green") return TFT_GREEN;
    if (color == "blue") return TFT_BLUE;
    if (color == "yellow") return TFT_YELLOW;
    return TFT_BLACK;
}

void mqtt_callback(char* raw_topic, byte* payload, unsigned int length) {
    StaticJsonDocument<256> doc;

    String topic = String(raw_topic);
    M5.Lcd.printf("T=%s\n", raw_topic);

    if (topic.equals("m5go/sleep")) {
        if (length >= 5 && strcmp("light", (const char *)payload)) {
            led_animation_stop();
            M5.Lcd.setBrightness(0);
            // M5.Power.lightSleep(SLEEP_SEC(10));
            return;
        }
    }

    M5.Lcd.setBrightness(200);

    // {"title":"WASHING","subtitle":"120 W","color":"green","menu":["check",null,"off"]}
    deserializeJson(doc, payload, length);
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
            int animation = 2; // SpinStop
            if (doc.containsKey("animation")) {
                animation = doc["animation"];
            }

            led_animation(animation, 150, color);
            // led_chase(color);
        }
    }
}

void loop() {
    if (!client.connected()) {
        reConnect();
        client.publish("m5go/button", "P");
    }

    client.loop();

    if (mp3 && mp3->isRunning()) {
        if (!mp3->loop()) {
            stop_sound();
        }
    }

    led_animation_loop();

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
    M5.Lcd.clearDisplay();
    M5.Lcd.setCursor(0, 10);
    M5.Lcd.printf("Connecting to wifi");
    WiFi.mode(WIFI_STA);  //Set the mode to WiFi station mode. 
    WiFi.begin(SSID, SSID_PASS); //Start Wifi connection.

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        M5.Lcd.print(".");
    }

    M5.Lcd.printf("\nSuccess\n");
}

void reConnect() {
    while (!client.connected()) {
        M5.Lcd.clearDisplay();
        M5.Lcd.setCursor(0, 10);
        M5.Lcd.print("Connecting to MQTT...");
        // Create a random client ID.
        String clientId = "M5Go-";
        clientId += String(random(0xffff), HEX);
        // Attempt to connect.
        if (client.connect(clientId.c_str(), MQTT_USER, MATT_PASS)) {
            delay(1000);
            M5.Lcd.printf("\nSuccess\n");
            // Once connected, publish an announcement to the topic. 
            client.publish("m5go/hello", "hello world");
            // ... and resubscribe. 
            client.subscribe("m5go/#");
            //M5.Lcd.clearDisplay();
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
