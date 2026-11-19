// =============================================================================
//  ESP32 – Wi-Fi most pre dron (VRS)
//
//  - ESP32 vytvori vlastnu Wi-Fi siet (AP) "DRON-VRS"
//  - na http://192.168.4.1/ posle webove UI (index_html.h)
//  - WebSocket na porte 81: binarne ramce z UI posiela 1:1 na UART2 do STM32
//    a vsetko co pride z STM32 posiela spat do UI
//  - modra LED (GPIO2) blika, ked su motory zapnute (arm)
//  - DEMO rezim: ked STM32 este nie je, ESP si telemetriu vymysla samo,
//    aby sa dalo UI skusat (naklon reaguje na packy)
//
//  Doska:      klasicke ESP32 (ESP32-WROOM DevKit)
//  Kniznica:   "WebSockets" od Markus Sattler (Arduino Library Manager)
//  Zapojenie:  ESP32 GPIO16 (RX2) <- STM32 PA9  (USART1_TX)
//              ESP32 GPIO17 (TX2) -> STM32 PA10 (USART1_RX)
//              GND <-> GND
//  Protokol:   [0xAA][TYPE][LEN][PAYLOAD][CRC8], viac v Komunikacia/README.md
// =============================================================================

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <WebSocketsServer.h>
#include "index_html.h"

// ---------- nastavenia ----------
#define DEMO        1          // 1 = vymyslena telemetria (bez STM32), 0 = most do STM32
#define AP_SSID     "DRON-VRS"
#define AP_PASS     "dron1234" // min. 8 znakov
#define UART_RX     16
#define UART_TX     17
#define UART_BAUD   115200
#define LED_PIN     2          // modra LED na ESP32 DevKit; blika, ked su motory zapnute

WebServer http(80);
DNSServer dns;   // vsetky domeny -> 192.168.4.1, aby telefon nepovazoval siet za "bez internetu"
WebSocketsServer ws(81);

// ---------- CRC8 (poly 0x07, init 0x00) – rovnake ako v UI ----------
uint8_t crc8(const uint8_t *data, size_t len) {
  uint8_t c = 0;
  for (size_t i = 0; i < len; i++) {
    c ^= data[i];
    for (int b = 0; b < 8; b++) c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x07) : (uint8_t)(c << 1);
  }
  return c;
}

// ---------- vypis ramca na USB serial (ladenie) ----------
void printFrame(const char *label, const uint8_t *data, size_t len) {
  Serial.print(label);
  for (size_t i = 0; i < len; i++) Serial.printf(" %02x", data[i]);
  Serial.println();
}

// ---------- posledne ovladanie z UI (LED aj DEMO) ----------
int16_t ctlThr = 0, ctlRoll = 0, ctlPitch = 0, ctlYaw = 0;
bool ctlArm = false;
unsigned long lastControlMs = 0;

void readControl(const uint8_t *f, size_t len) {
  // ovladaci ramec: AA 01 09 thr(2) roll(2) pitch(2) yaw(2) flags(1) crc
  if (len != 13 || f[0] != 0xAA || f[1] != 0x01 || f[2] != 9) return;
  if (crc8(f + 1, 11) != f[12]) return;
  ctlThr   = (int16_t)(f[3] | f[4] << 8);
  ctlRoll  = (int16_t)(f[5] | f[6] << 8);
  ctlPitch = (int16_t)(f[7] | f[8] << 8);
  ctlYaw   = (int16_t)(f[9] | f[10] << 8);
  ctlArm   = f[11] & 1;
  lastControlMs = millis();
}

// vymyslena telemetria 20 Hz: naklon podla packy, vyska podla plynu, batéria pomaly klesa
void demoSendTelemetry() {
  static float yaw = 0, alt = 0, bat = 12.4;
  static uint8_t state = 0;
  bool lost = millis() - lastControlMs > 250;           // failsafe ako na STM32

  if (!ctlArm && alt <= 0) state = 0;                    // VYPNUTY
  else if (lost) state = 3;                              // PRISTAVA
  else state = alt > 0.05 ? 2 : 1;                       // LETI / PRIPRAVENY

  if (state == 3) alt -= 0.02;                           // pomale klesanie
  else if (ctlArm) alt += ctlThr / 1000.0 * 0.05;
  if (alt < 0) alt = 0;
  if (alt > 5) alt = 5;
  if (state == 3 && alt == 0) ctlArm = false;

  yaw += ctlYaw / 1000.0 * 3;                            // max 3 ° za krok
  if (yaw >= 180) yaw -= 360;
  if (yaw < -180) yaw += 360;
  bat -= 0.00005;

  int16_t roll  = lost ? 0 : ctlRoll * 3;                // 1000 -> 30.00 °
  int16_t pitch = lost ? 0 : ctlPitch * 3;
  int16_t yawC  = (int16_t)(yaw * 100);
  uint16_t altMm = (uint16_t)(alt * 1000);
  uint16_t batMv = (uint16_t)(bat * 1000);

  uint8_t f[15] = { 0xAA, 0x10, 11,
    (uint8_t)roll,  (uint8_t)(roll >> 8),
    (uint8_t)pitch, (uint8_t)(pitch >> 8),
    (uint8_t)yawC,  (uint8_t)(yawC >> 8),
    (uint8_t)altMm, (uint8_t)(altMm >> 8),
    (uint8_t)batMv, (uint8_t)(batMv >> 8),
    state, 0 };
  f[14] = crc8(f + 1, 13);
  ws.broadcastBIN(f, sizeof(f));
}

// ---------- WebSocket udalosti ----------
void onWs(uint8_t client, WStype_t type, uint8_t *payload, size_t len) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.printf("[ws] klient %u pripojeny\n", client);
      break;
    case WStype_DISCONNECTED:
      Serial.printf("[ws] klient %u odpojeny\n", client);
      break;
    case WStype_BIN: {
      Serial2.write(payload, len);                       // most: UI -> STM32
      readControl(payload, len);
      static unsigned long lastPrint = 0;                // vypis max 2x za sekundu
      if (millis() - lastPrint > 500) { lastPrint = millis(); printFrame("[TX]", payload, len); }
      break;
    }
    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(UART_BAUD, SERIAL_8N1, UART_RX, UART_TX);
  pinMode(LED_PIN, OUTPUT);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  WiFi.setSleep(false);                                  // nizsia latencia
  WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t) { Serial.println("[wifi] zariadenie sa pripojilo"); },
               ARDUINO_EVENT_WIFI_AP_STACONNECTED);
  WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t) { Serial.println("[wifi] zariadenie sa odpojilo"); },
               ARDUINO_EVENT_WIFI_AP_STADISCONNECTED);
  dns.start(53, "*", WiFi.softAPIP());
  Serial.printf("\nWi-Fi: %s / %s, UI: http://%s/\n", AP_SSID, AP_PASS, WiFi.softAPIP().toString().c_str());

  http.on("/", []() {
    Serial.printf("[http] UI pre %s\n", http.client().remoteIP().toString().c_str());
    http.send(200, "text/html", INDEX_HTML);
  });
  // kontrola internetu telefonov / PC: odpovieme "internet funguje",
  // inak Android a iOS posielaju poziadavky cez mobilne data a 192.168.4.1 je "nedostupne"
  http.on("/generate_204", []() { http.send(204); });                       // Android
  http.on("/gen_204", []() { http.send(204); });                            // Android
  http.on("/hotspot-detect.html", []() {                                    // iPhone / Mac
    http.send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
  });
  http.on("/connecttest.txt", []() { http.send(200, "text/plain", "Microsoft Connect Test"); });  // Windows
  http.on("/ncsi.txt", []() { http.send(200, "text/plain", "Microsoft NCSI"); });                 // Windows
  http.on("/204", []() { http.send(204); });                                // Steam Deck (steamconnecttest.com)
  http.on("/nm-check.txt", []() { http.send(200, "text/plain", "NetworkManager is online\n"); });  // Linux / SteamOS
  // vsetko ostatne: prazdna odpoved 204 (ziadne presmerovanie – inak to zariadenia beru ako
  // prihlasovaciu stranku siete a skusaju to dokola)
  http.onNotFound([]() { http.send(204); });
  http.begin();

  ws.begin();
  ws.onEvent(onWs);
#if DEMO
  Serial.println("DEMO rezim: telemetriu generuje ESP32");
#endif
}

void loop() {
  dns.processNextRequest();
  http.handleClient();
  ws.loop();

  // most: STM32 -> UI (co pride na UART, posle sa do vsetkych pripojenych UI)
  uint8_t buf[128];
  size_t n = 0;
  while (Serial2.available() && n < sizeof(buf)) buf[n++] = Serial2.read();
  if (n) ws.broadcastBIN(buf, n);

  // LED: motory zapnute a spojenie zive -> blika 2x za sekundu, inak nesvieti
  bool armed = ctlArm && millis() - lastControlMs < 250;
  digitalWrite(LED_PIN, armed && (millis() / 250) % 2);

#if DEMO
  static unsigned long lastTelemetry = 0;
  if (millis() - lastTelemetry >= 50) { lastTelemetry = millis(); demoSendTelemetry(); }
#endif
}
