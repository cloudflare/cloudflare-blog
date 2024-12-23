#include <HTTPClient.h>
#include <WiFi.h>

/* Put the headers from vendor example in directory like ~/Arduino/libraries/epd */
#include <buff.h>
#include <epd.h>

/* For me it's "7.5 inch V2", which is "EPD_7in5_V2" symbol, which has index 22.
 * See epd.h for the full list. */
#define DEVICE_MODEL_EPD 22

#define FETCH_URL "http://weather-panel.userxxx.workers.dev/?binary=1"
#define REFRESH_FREQ_MS (60 * 1000)

const char *ssid = "xxx";
const char *password = "xxx";

void wifi_setup() {
  Serial.printf("Connecting to %s\n", ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.printf("\nWiFi connected %s\n", WiFi.localIP().toString().c_str());
}

void setup() {
  Serial.begin(115200);
  delay(10);

  wifi_setup();

  EPD_initSPI();
  EPD_dispIndex = DEVICE_MODEL_EPD;

  Serial.printf("Ok!\n");
}

void loop() {
  static unsigned long refresh_ts_ms;
  static String etag = "00000000";
  delay(100);

  if ((millis() - refresh_ts_ms) > REFRESH_FREQ_MS || refresh_ts_ms == 0) {
    refresh_ts_ms = millis();
    Serial.printf("Refresh\n");

    String payload;

    // Fetch if wifi is okay
    if ((WiFi.status() == WL_CONNECTED)) {
      HTTPClient http;

      const char *headerKeys[] = { "ETag" };
      const size_t headerKeysCount = sizeof(headerKeys) / sizeof(headerKeys[0]);
      http.collectHeaders(headerKeys, headerKeysCount);

      http.setConnectTimeout(30 * 1000);

      http.begin(FETCH_URL);
      http.addHeader("If-None-Match", etag, false, true);

      int httpCode = http.GET();
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);

      // httpCode will be negative on error
      if (httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        if (httpCode == HTTP_CODE_OK) {
          payload = http.getString();
          etag = http.header("ETag");
        } else {
          // 304 goes here
          payload = "";
        }
      } else {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }
      Serial.printf("HTTP ETag ", etag.c_str());
      http.end();
    }

    // Only on HTTP 200 OK, not error or 304
    if (payload.length() >= 800 * 480 / 8) {
      EPD_dispInit();

      int i;
      for (i = 0; i < 480 * 800 / 8; i++) {
        EPD_SendData(~payload[i]);
      }

      EPD_dispMass[EPD_dispIndex].show();
    }
  }
}
