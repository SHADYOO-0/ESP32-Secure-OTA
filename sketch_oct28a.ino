#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include "mbedtls/pk.h"
#include "mbedtls/sha256.h"

const char* WIFI_SSID     = "DESKTOP-PI4QIBE 7718";
const char* WIFI_PASS     = "V559g1{4";
const char* FIRMWARE_URL  = "http://192.168.137.1:8000/firmware.bin";
const char* SIGNATURE_URL = "http://192.168.137.1:8000/firmware.sig";
const char* REPORT_URL    = "http://192.168.137.1:8000/report?status=";

// Clean Public Key with explicit array size
const char PUBLIC_KEY_PEM[] = 
"-----BEGIN PUBLIC KEY-----\n"
"MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEeBfo26pieNEuBnX8jENl9oul+9Dx\n"
"7LO8vaRdzL3xeqTjw5zE0JfpFjia0yMpeaJmNHMc7rcQBCVlHvcdm+SL+w==\n"
"-----END PUBLIC KEY-----\n";

const int BOOT_BUTTON_PIN = 0;
const int ONBOARD_LED     = 2;

void sendReportToServer(const char* status) {
  HTTPClient http;
  String url = String(REPORT_URL) + status;
  http.begin(url);
  http.GET();
  http.end();
}

void indicateErrorTampered() {
  Serial.println("\n[SECURITY ALERT] Signature Verification Failed! Tampered update discarded.");
  for (int i = 0; i < 20; i++) {
    digitalWrite(ONBOARD_LED, HIGH);
    delay(75);
    digitalWrite(ONBOARD_LED, LOW);
    delay(75);
  }
}

void indicateSuccess() {
  Serial.println("\n[SUCCESS] Update valid and applied! Rebooting...");
  for (int i = 0; i < 3; i++) {
    digitalWrite(ONBOARD_LED, HIGH);
    delay(400);
    digitalWrite(ONBOARD_LED, LOW);
    delay(400);
  }
}

bool downloadSignature(uint8_t* sig_buffer, size_t* sig_len) {
  HTTPClient http;
  http.begin(SIGNATURE_URL);
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("[ERROR] Failed to download signature, HTTP: %d\n", httpCode);
    http.end();
    return false;
  }
  
  WiFiClient* stream = http.getStreamPtr();
  *sig_len = 0;
  while (http.connected() && *sig_len < 128) {
    while (stream->available()) {
      sig_buffer[(*sig_len)++] = stream->read();
    }
  }
  http.end();
  Serial.printf("[OTA] Signature received (%d bytes).\n", *sig_len);
  return true;
}

void triggerSecureOTA() {
  digitalWrite(ONBOARD_LED, HIGH);
  Serial.println("\n--- Starting Secure OTA Process ---");

  uint8_t signature[128];
  size_t sig_len = 0;
  if (!downloadSignature(signature, &sig_len)) {
    digitalWrite(ONBOARD_LED, LOW);
    return;
  }

  HTTPClient http;
  http.begin(FIRMWARE_URL);
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("[ERROR] Failed to connect to firmware, HTTP: %d\n", httpCode);
    http.end();
    digitalWrite(ONBOARD_LED, LOW);
    return;
  }

  int contentLength = http.getSize();
  if (contentLength <= 0) {
    Serial.println("[ERROR] Invalid firmware content length.");
    http.end();
    digitalWrite(ONBOARD_LED, LOW);
    return;
  }

  mbedtls_sha256_context sha_ctx;
  mbedtls_sha256_init(&sha_ctx);
  mbedtls_sha256_starts(&sha_ctx, 0);

  if (!Update.begin(contentLength)) {
    Serial.println("[ERROR] Not enough space for OTA.");
    http.end();
    return;
  }

  Serial.println("[OTA] Downloading and hashing firmware...");
  WiFiClient* stream = http.getStreamPtr();
  uint8_t buff[512];
  int written = 0;

  while (http.connected() && (written < contentLength)) {
    size_t availableBytes = stream->available();
    if (availableBytes) {
      int bytesToRead = availableBytes > sizeof(buff) ? sizeof(buff) : availableBytes;
      int bytesRead = stream->readBytes(buff, bytesToRead);

      mbedtls_sha256_update(&sha_ctx, buff, bytesRead);
      Update.write(buff, bytesRead);
      written += bytesRead;
    }
  }
  http.end();

  uint8_t hash[32];
  mbedtls_sha256_finish(&sha_ctx, hash);
  mbedtls_sha256_free(&sha_ctx);

  Serial.println("[OTA] Download complete. Verifying cryptographic signature...");

  mbedtls_pk_context pk;
  mbedtls_pk_init(&pk);
  int ret = mbedtls_pk_parse_public_key(&pk, (const unsigned char*)PUBLIC_KEY_PEM, sizeof(PUBLIC_KEY_PEM));
  if (ret != 0) {
    Serial.printf("[ERROR] Public key parse failed: -0x%04x\n", -ret);
    Update.abort();
    mbedtls_pk_free(&pk);
    sendReportToServer("TAMPER_DETECTED");
    indicateErrorTampered();
    return;
  }

  ret = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, 32, signature, sig_len);
  mbedtls_pk_free(&pk);

  if (ret != 0) {
    Update.abort();
    sendReportToServer("TAMPER_DETECTED"); // Notify server!
    indicateErrorTampered();
  } else {
    if (Update.end()) {
      sendReportToServer("SUCCESS"); // Notify server!
      indicateSuccess();
      ESP.restart();
    } else {
      Serial.printf("[ERROR] Update.end() failed: %s\n", Update.errorString());
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(ONBOARD_LED, OUTPUT);

  WiFi.disconnect(true);
  delay(1000);
  WiFi.mode(WIFI_STA);

  Serial.println("\nConnecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Connected!");
    Serial.printf("[Device] Ready. Running Firmware v1.0. IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.println("[Prompt] Press the BOOT button to check for updates...");
  } else {
    Serial.println("\n[WiFi ERROR] Failed to connect.");
  }
}

void loop() {
  if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
    delay(200);
    triggerSecureOTA();
  }
}