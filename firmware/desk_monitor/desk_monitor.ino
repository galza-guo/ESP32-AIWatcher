// Desk monitor serial client for ESP32-S3 + CH340.
// Host writes one JSON line per sample at 115200.

#include <Arduino.h>

static char line[1024];
static size_t filled = 0;
static uint32_t last_ms = 0;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("{\"hello\":\"desk-monitor\"}");
}

static void take_line(char *raw) {
  last_ms = millis();
  // Keep the link honest; a panel renderer can parse sys.cpu here later.
  Serial.print("{\"ack\":true,\"n\":");
  Serial.print(strlen(raw));
  Serial.println("}");
}

void loop() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n') {
      line[filled] = 0;
      if (filled) take_line(line);
      filled = 0;
    } else if (filled + 1 < sizeof(line)) {
      line[filled++] = c;
    } else {
      filled = 0;
    }
  }
  if (millis() - last_ms > 4000) {
    last_ms = millis();
    Serial.println("{\"waiting\":true}");
  }
}
