#include <Arduino.h>
#include <Preferences.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include "esp_camera.h"

// AI-Thinker ESP32-CAM pin assignment. Do not use GPIO 0, 1, 3, 12 or 16 here.
namespace Pins {
constexpr int PWDN = 32, RESET = -1, XCLK = 0, SIOD = 26, SIOC = 27;
constexpr int Y9 = 35, Y8 = 34, Y7 = 39, Y6 = 36, Y5 = 21, Y4 = 19, Y3 = 18, Y2 = 5;
constexpr int VSYNC = 25, HREF = 23, PCLK = 22;
}

WebServer server(80);
Preferences preferences;
String accessPoint;

bool configured() { return preferences.getString("admin_pass", "").length() >= 12; }

bool requireAdmin() {
  if (!configured()) return true; // Only the password-creation screen is open during first setup.
  if (server.authenticate("admin", preferences.getString("admin_pass").c_str())) return true;
  server.requestAuthentication();
  return false;
}

String csrfToken() {
  String token = preferences.getString("csrf", "");
  if (!token.isEmpty()) return token;
  char value[25];
  snprintf(value, sizeof(value), "%08lX%08lX%08lX", (unsigned long)esp_random(), (unsigned long)esp_random(), (unsigned long)esp_random());
  preferences.putString("csrf", value);
  return String(value);
}

String suffix() {
  uint64_t mac = ESP.getEfuseMac();
  char value[7];
  snprintf(value, sizeof(value), "%06llX", mac & 0xFFFFFFULL);
  return String(value);
}

bool initialiseCamera() {
  camera_config_t config{};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Pins::Y2; config.pin_d1 = Pins::Y3; config.pin_d2 = Pins::Y4; config.pin_d3 = Pins::Y5;
  config.pin_d4 = Pins::Y6; config.pin_d5 = Pins::Y7; config.pin_d6 = Pins::Y8; config.pin_d7 = Pins::Y9;
  config.pin_xclk = Pins::XCLK; config.pin_pclk = Pins::PCLK; config.pin_vsync = Pins::VSYNC; config.pin_href = Pins::HREF;
  config.pin_sccb_sda = Pins::SIOD; config.pin_sccb_scl = Pins::SIOC;
  config.pin_pwdn = Pins::PWDN; config.pin_reset = Pins::RESET;
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_GRAYSCALE; // raw frames stay exclusively in RAM
  config.frame_size = FRAMESIZE_QQVGA;        // 160x120: fast enough for the local detector
  config.jpeg_quality = 12;
  config.fb_count = psramFound() ? 2 : 1;
  config.grab_mode = CAMERA_GRAB_LATEST;
  if (esp_camera_init(&config) != ESP_OK) return false;
  sensor_t *sensor = esp_camera_sensor_get();
  sensor->set_vflip(sensor, 1);
  sensor->set_brightness(sensor, 0);
  return true;
}

void startNetwork() {
  accessPoint = "AutoCounter-" + suffix();
  const String apPassword = "cam-" + suffix();
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(accessPoint.c_str(), apPassword.c_str());
  const String ssid = preferences.getString("ssid", "");
  const String password = preferences.getString("wifi_pass", "");
  if (!ssid.isEmpty()) WiFi.begin(ssid.c_str(), password.c_str());
  Serial.printf("Setup WLAN: %s  Passwort: %s\n", accessPoint.c_str(), apPassword.c_str());
}

String page() {
  const String station = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "nicht verbunden";
  return "<!doctype html><html lang='de'><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Autozähler einrichten</title><style>body{margin:0;background:#10201c;color:#edf5ee;font:16px ui-monospace,Menlo,monospace}.wrap{max-width:640px;margin:auto;padding:8vh 24px}h1{font-size:clamp(2rem,7vw,3.8rem);line-height:1;margin:0 0 1.5rem;color:#d7ff71}.card{background:#19352d;border:1px solid #477164;padding:24px;border-radius:5px}label{display:block;margin:1rem 0 .35rem}input{box-sizing:border-box;width:100%;padding:.8rem;border:1px solid #70998a;background:#f5f7e8;color:#17221e;font:inherit}button{margin-top:1.2rem;padding:.85rem 1rem;border:0;background:#d7ff71;color:#10201c;font:700 1rem ui-monospace,monospace;cursor:pointer}.hint{color:#b8cbbf;font-size:.9rem;line-height:1.5}</style>"
    "<main class='wrap'><p>ESP32-CAM · lokale Einrichtung</p><h1>Verbinde den Zähler.</h1><section class='card'><p>Geräte-WLAN: <b>" + accessPoint + "</b><br>Passwort: <b>cam-" + suffix() + "</b></p>"
    "<p class='hint'>Aktuelle Netzwerkadresse: " + station + ". Die Kamera speichert oder überträgt keine Bilder.</p>"
    "<form method='post' action='/config'><input type='hidden' name='csrf' value='" + csrfToken() + "'><label>WLAN-Name <input required maxlength='32' name='ssid' autocomplete='off'></label><label>WLAN-Passwort <input maxlength='63' type='password' name='password' autocomplete='new-password'></label>" +
    (configured() ? "" : "<label>Neues Gerätepasswort <input required minlength='12' maxlength='63' type='password' name='admin_password' autocomplete='new-password'></label><p class='hint'>Dieses Passwort schützt das Dashboard und OTA-Updates nach der Einrichtung.</p>") +
    "<button>WLAN speichern und neu starten</button></form></section></main></html>";
}

void registerRoutes() {
  server.on("/", HTTP_GET, [] { if (requireAdmin()) server.send(200, "text/html; charset=utf-8", page()); });
  server.on("/status", HTTP_GET, [] {
    if (!requireAdmin()) return;
    String json = "{\"ap\":\"" + accessPoint + "\",\"wifi_connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",\"ip\":\"" + WiFi.localIP().toString() + "\",\"heap\":" + String(ESP.getFreeHeap()) + "}";
    server.send(200, "application/json", json);
  });
  server.on("/config", HTTP_POST, [] {
    if (!requireAdmin()) return;
    const String ssid = server.arg("ssid"), password = server.arg("password");
    const String adminPassword = server.arg("admin_password");
    if (server.arg("csrf") != csrfToken()) { server.send(403, "text/plain", "Ungültige Anfrage"); return; }
    if (ssid.isEmpty() || ssid.length() > 32 || password.length() > 63 || (!configured() && (adminPassword.length() < 12 || adminPassword.length() > 63))) { server.send(422, "text/plain", "Ungültige WLAN-Daten"); return; }
    preferences.putString("ssid", ssid); preferences.putString("wifi_pass", password);
    if (!configured()) preferences.putString("admin_pass", adminPassword);
    server.send(200, "text/html", "<meta http-equiv='refresh' content='3;url=/'><p>Gespeichert. Neustart …</p>");
    delay(500); ESP.restart();
  });
  server.on("/update", HTTP_GET, [] { if (!configured()) { server.send(403, "text/plain", "OTA erst nach der Einrichtung verfügbar"); return; } if (requireAdmin()) server.send(200, "text/html", "<!doctype html><form method='post' action='/update' enctype='multipart/form-data'><input type='hidden' name='csrf' value='" + csrfToken() + "'><input type='file' name='firmware' accept='.bin' required><button>Firmware aktualisieren</button></form>"); });
  server.on("/update", HTTP_POST, [] { if (!configured() || !requireAdmin()) return; if (server.arg("csrf") != csrfToken()) { server.send(403, "text/plain", "Ungültige Anfrage"); return; } server.sendHeader("Connection", "close"); server.send(Update.hasError() ? 500 : 200, "text/plain", Update.hasError() ? "Update fehlgeschlagen" : "Update erfolgreich; Neustart"); delay(500); ESP.restart(); }, [] {
    if (!configured() || !server.authenticate("admin", preferences.getString("admin_pass").c_str())) return;
    HTTPUpload &upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) Update.begin(UPDATE_SIZE_UNKNOWN);
    else if (upload.status == UPLOAD_FILE_WRITE && !Update.hasError()) Update.write(upload.buf, upload.currentSize);
    else if (upload.status == UPLOAD_FILE_END) Update.end(true);
  });
  server.onNotFound([] { server.send(404, "text/plain", "Nicht gefunden"); });
  server.begin();
}

void setup() {
  Serial.begin(115200); delay(250);
  preferences.begin("autocounter", false);
  if (!initialiseCamera()) Serial.println("Kamera konnte nicht initialisiert werden");
  startNetwork(); registerRoutes();
}

void loop() {
  server.handleClient();
}
