

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <WiFiManagerESP32.h>
#include <esp_system.h>
#include <esp_timer.h>


// ============================================================================
// 🧩 Déclarations globales
// ============================================================================
WiFiManagerESP32 wifiMgr;
AsyncWebServer server(80);

// Tâche de "heartbeat" pour surveillance système
TaskHandle_t heartbeatTaskHandle = nullptr;

// ============================================================================
// 🔁 Tâche secondaire : clignotement LED et ping périodique
// ============================================================================
void heartbeatTask(void *pv) {
  const gpio_num_t LED = GPIO_NUM_2;  // LED intégrée sur certaines cartes
  pinMode(LED, OUTPUT);

  TickType_t xLastWake = xTaskGetTickCount();
  for (;;) {
    digitalWrite(LED, !digitalRead(LED));  // toggle
    if (wifiMgr.isConnected()) {
      Serial.println("[HB] Wi-Fi OK");
    } else if (wifiMgr.isAPActive()) {
      Serial.println("[HB] Mode AP");
    } else {
      Serial.println("[HB] Wi-Fi inactif");
    }

    // Ping ou tâche de fond toutes les 5 secondes
    vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(5000));
  }
}

// ============================================================================
// 🚀 SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  vTaskDelay(pdMS_TO_TICKS(500));  // petit délai de stabilité

  Serial.println("\n[MAIN] Initialisation du système...");

  // --- Montage du système de fichiers ---
  if (!LittleFS.begin(true)) {
    Serial.println("[MAIN] ❌ Erreur LittleFS");
  } else {
    Serial.println("[MAIN] ✅ LittleFS prêt");
  }

  // --- Configuration du Wi-Fi ---
  wifiMgr.Debug(true);              // Mode verbose
  wifiMgr.Name("ESP32-Station");   // Nom du projet (pour AP + page web)
  bool ok = wifiMgr.begin(
    true,   // attendre Wi-Fi
    true,   // attendre NTP
    3,      // maxFails avant AP
    10      // checkTime_s
  );

  if (ok) Serial.println("[MAIN] ✅ Connecté au réseau");
  else    Serial.println("[MAIN] ⚠️ Démarrage en mode AP");

  // --- Configuration des routes web ---
  wifiMgr.WebConfig(server);

  // --- Autres pages (exemple) ---
server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    AsyncResponseStream *res = req->beginResponseStream("text/html");

    // --- Uptime 64 bits (pas de débordement)
    uint64_t uptime_us = esp_timer_get_time();
    uint64_t uptime_s = uptime_us / 1000000ULL;
    uint32_t days = uptime_s / 86400ULL;
    uint32_t hours = (uptime_s % 86400ULL) / 3600ULL;
    uint32_t minutes = (uptime_s % 3600ULL) / 60ULL;
    uint32_t seconds = uptime_s % 60ULL;

    // --- Infos mémoire
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t minFreeHeap = ESP.getMinFreeHeap();
    uint32_t maxAllocHeap = ESP.getMaxAllocHeap();
    uint32_t psramFree = ESP.getFreePsram();
    uint32_t psramSize = ESP.getPsramSize();

    // --- Infos système
    uint32_t cpuFreq = getCpuFrequencyMhz();
    const char *sdkVersion = esp_get_idf_version();

    // --- Infos Wi-Fi
    bool wifiOK = WiFi.isConnected();
    int32_t rssi = wifiOK ? WiFi.RSSI() : 0;
    IPAddress ip = WiFi.localIP();

    char ipStr[32];
    snprintf(ipStr, sizeof(ipStr), "%s", wifiOK ? ip.toString().c_str() : "0.0.0.0");

    // --- Page HTML simple
    res->printf(
        "<html><meta charset='utf-8'><body><center>"
        "<h2>ESP32 Diagnostic H24</h2>"
        "<p><b>Wi-Fi :</b> %s</p>"
        "<p><b>IP :</b> %s</p>"
        "<p><b>RSSI :</b> %d dBm</p>"
        "<hr>"
        "<p><b>Uptime :</b> %luj %luh %lum %lus</p>"
        "<hr>"
        "<h3>Mémoire</h3>"
        "<p>Heap libre : %lu octets<br>"
        "Heap minimum : %lu octets<br>"
        "Bloc max allocable : %lu octets<br>"
        "PSRAM : %lu / %lu octets libres</p>"
        "<hr>"
        "<h3>Système</h3>"
        "<p>CPU : %lu MHz<br>"
        "SDK : %s</p>"
        "<hr>"
        "<a href='/wifi'>Configurer le Wi-Fi</a>"
        "<p><small>Actualisation automatique toutes les 5s</small></p>"
        "<script>setTimeout(()=>location.reload(),5000);</script>"
        "</center></body></html>",
        wifiOK ? "Connecté" : "Hors ligne",
        ipStr, rssi,
        days, hours, minutes, seconds,
        freeHeap, minFreeHeap, maxAllocHeap,
        psramFree, psramSize,
        cpuFreq, sdkVersion
    );

    req->send(res);
});

  // --- Lancement du serveur web ---
  server.begin();
  Serial.println("[MAIN] 🌐 Serveur web lancé sur port 80");

  // --- Lancement de la tâche heartbeat ---
  xTaskCreatePinnedToCore(
    heartbeatTask,
    "heartbeat",
    2048,
    nullptr,
    1,
    &heartbeatTaskHandle,
    APP_CPU_NUM
  );
  Serial.println("[MAIN] 💓 Tâche Heartbeat lancée");
}

// ============================================================================
// 🔄 LOOP — vide (tout tourne en tâches FreeRTOS)
// ============================================================================
void loop() {
  // Boucle principale vide pour stabilité 24/7
  vTaskDelay(pdMS_TO_TICKS(1000));
}
