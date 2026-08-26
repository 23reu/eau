#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// =====================================================================
// CONFIGURATION WiFi
// =====================================================================
const char* ssid = "YOUR_SSID";           // Remplacer par votre SSID WiFi
const char* password = "YOUR_PASSWORD";   // Remplacer par votre mot de passe WiFi
const int webServerPort = 80;
WebServer server(webServerPort);

// =====================================================================
// CONFIGURATION I2C ESP-32D
// =====================================================================
// ESP-32D utilise I2C matériel (plus rapide et stable)
// SDA: GPIO 21
// SCL: GPIO 22
#define SDA_PIN 21
#define SCL_PIN 22
LiquidCrystal_I2C lcd(0x27, 20, 4);

// =====================================================================
// CONFIGURATION DES CAPTEURS - CALIBRATION REQUISE
// =====================================================================
// ESP-32D broches ADC disponibles: GPIO 32, 33, 34, 35 (ADC1 stable)
const int SENSOR_PINS[] = {32, 33, 34, 35, 2, 4};
const int NUM_SENSORS = 6;
const int SMOOTHING_FACTOR = 10;  // Lissage: moyenne sur 10 lectures

// IMPORTANT: Ces valeurs doivent être calibrées pour votre système!
// Mesurez les valeurs réelles de vos capteurs:
// - À sec (air): notez la valeur minimale
// - Immergé (eau): notez la valeur maximale
// Puis ajustez les seuils ci-dessous
const int THRESHOLD_AIR = 2000;    // Valeur capteur à l'air (à calibrer)
const int THRESHOLD_WATER = 500;   // Valeur capteur dans l'eau (à calibrer)

int sensorValues[NUM_SENSORS];
int sensorAveraged[NUM_SENSORS];
int sensorReadings[NUM_SENSORS][SMOOTHING_FACTOR];
int readingIndex = 0;

// Flags pour éviter de redessiner l'écran en boucle
int send_0, send_1, send_2, send_3, send_4, send_5, send_6;
int lastDisplayedLevel = -1;  // Suivi du dernier niveau affiché

// Mode debug pour la calibration
bool debugMode = true;  // Mettre à false après calibration

// Caractère personnalisé pour la barre
byte solidBlock[8] = {
  B11111, B11111, B11111, B11111, B11111, B11111, B11111, B11111
};

// Variables de temps pour WiFi
unsigned long lastWiFiCheck = 0;
const unsigned long wifiCheckInterval = 30000;

// =====================================================================
// BARRE DE PROGRESSION (3 blocs par niveau = 18 blocs total)
// =====================================================================
void drawBar(int level) {
  lcd.setCursor(0, 1);
  lcd.print("[");
  int totalBlocks = NUM_SENSORS * 3;  // 18 blocs
  int filledBlocks = level * 3;       // 3 blocs par niveau
  for (int i = 0; i < totalBlocks; i++) {
    if (i < filledBlocks) {
      lcd.write((uint8_t)0);
    } else {
      lcd.print(" ");
    }
  }
  lcd.print("]");
}

// =====================================================================
// FONCTION DE LISSAGE DES CAPTEURS
// =====================================================================
void updateSensorReadings() {
  // Ajouter les nouvelles lectures
  for (int i = 0; i < 4; i++) {
    sensorReadings[i][readingIndex] = analogRead(SENSOR_PINS[i]);  // GPIO 32-35 (ADC)
  }
  
  // Lecture broches numériques (converties en valeurs 0-4095)
  sensorReadings[4][readingIndex] = digitalRead(SENSOR_PINS[4]) ? 4095 : 0;  // GPIO 2
  sensorReadings[5][readingIndex] = digitalRead(SENSOR_PINS[5]) ? 4095 : 0;  // GPIO 4
  
  // Incrementer l'index
  readingIndex = (readingIndex + 1) % SMOOTHING_FACTOR;
  
  // Calculer la moyenne pour chaque capteur
  for (int i = 0; i < NUM_SENSORS; i++) {
    int sum = 0;
    for (int j = 0; j < SMOOTHING_FACTOR; j++) {
      sum += sensorReadings[i][j];
    }
    sensorAveraged[i] = sum / SMOOTHING_FACTOR;
    sensorValues[i] = sensorAveraged[i];
  }
}

// =====================================================================
// GESTION DU WiFi
// =====================================================================
void connectToWiFi() {
  Serial.println("\n[WiFi] Tentative de connexion...");
  lcd.setCursor(0, 3);
  lcd.print("WiFi: Connexion...   ");
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Connecté!");
    Serial.print("[WiFi] Adresse IP: ");
    Serial.println(WiFi.localIP());
    lcd.setCursor(0, 3);
    lcd.print("WiFi: OK             ");
  } else {
    Serial.println("\n[WiFi] Échec de connexion");
    lcd.setCursor(0, 3);
    lcd.print("WiFi: Deconnecte     ");
  }
}

void handleRoot() {
  String html = R"(
    <!DOCTYPE html>
    <html>
    <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>Détecteur Niveau d'Eau</title>
      <style>
        body {
          font-family: Arial, sans-serif;
          margin: 0;
          padding: 20px;
          background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
          min-height: 100vh;
        }
        .container {
          max-width: 600px;
          margin: 0 auto;
          background: white;
          border-radius: 10px;
          padding: 30px;
          box-shadow: 0 10px 25px rgba(0,0,0,0.2);
        }
        h1 {
          text-align: center;
          color: #333;
          margin: 0 0 30px 0;
        }
        .info-box {
          background: #f0f4ff;
          border-left: 4px solid #667eea;
          padding: 15px;
          margin: 15px 0;
          border-radius: 5px;
        }
        .level-display {
          font-size: 28px;
          font-weight: bold;
          color: #667eea;
          text-align: center;
          margin: 20px 0;
        }
        .progress-bar {
          width: 100%;
          height: 30px;
          background: #eee;
          border-radius: 5px;
          overflow: hidden;
          margin: 20px 0;
        }
        .progress-fill {
          height: 100%;
          background: linear-gradient(90deg, #667eea 0%, #764ba2 100%);
          width: 0%;
          transition: width 0.3s ease;
          display: flex;
          align-items: center;
          justify-content: center;
          color: white;
          font-weight: bold;
        }
        .sensors {
          display: grid;
          grid-template-columns: 1fr 1fr 1fr;
          gap: 10px;
          margin: 20px 0;
        }
        .sensor {
          background: #f9f9f9;
          padding: 10px;
          border-radius: 5px;
          border: 1px solid #ddd;
          text-align: center;
          font-size: 12px;
        }
        .sensor-name {
          font-weight: bold;
          color: #666;
          font-size: 11px;
        }
        .sensor-value {
          font-size: 16px;
          color: #333;
          margin-top: 3px;
        }
        .alert {
          background: #fff3cd;
          border-left: 4px solid #ff6b6b;
          padding: 15px;
          margin: 15px 0;
          border-radius: 5px;
          color: #d63031;
          font-weight: bold;
          display: none;
        }
        .status {
          text-align: center;
          margin-top: 20px;
          padding: 10px;
          background: #f0f0f0;
          border-radius: 5px;
          font-size: 12px;
          color: #666;
        }
        .calibration-info {
          background: #fff9e6;
          border-left: 4px solid #ff9800;
          padding: 10px;
          margin: 15px 0;
          border-radius: 5px;
          font-size: 12px;
          color: #e65100;
        }
      </style>
    </head>
    <body>
      <div class="container">
        <h1>💧 Détecteur Niveau d'Eau</h1>
        <div class="info-box">
          <strong>Capacité:</strong> 20 m³
        </div>
        
        <div class="calibration-info" id="calibrationInfo">
          ⚙️ <strong>Mode calibration activé</strong> - Les valeurs brutes des capteurs sont affichées ci-dessous pour la calibration.
        </div>
        
        <div class="level-display" id="levelDisplay">Chargement...</div>
        
        <div class="progress-bar">
          <div class="progress-fill" id="progressFill">0%</div>
        </div>
        
        <div class="alert" id="alertBox"></div>
        
        <div class="sensors" id="sensorsContainer">
          <!-- Sera rempli par JavaScript -->
        </div>
        
        <div class="status">
          <strong>Adresse IP:</strong> <span id="ipAddress"></span><br>
          <strong>Dernière mise à jour:</strong> <span id="lastUpdate">--:--:--</span>
        </div>
      </div>

      <script>
        function updateData() {
          fetch('/api/data')
            .then(response => response.json())
            .then(data => {
              // Afficher les valeurs brutes des capteurs
              const sensorsContainer = document.getElementById('sensorsContainer');
              sensorsContainer.innerHTML = '';
              data.sensors.forEach((value, index) => {
                const sensorDiv = document.createElement('div');
                sensorDiv.className = 'sensor';
                sensorDiv.innerHTML = `
                  <div class="sensor-name">GPIO ${data.pins[index]}</div>
                  <div class="sensor-value">${value}</div>
                `;
                sensorsContainer.appendChild(sensorDiv);
              });
              
              // Mise à jour du niveau
              const levelDisplay = document.getElementById('levelDisplay');
              const percentage = data.level * 16.666;
              levelDisplay.textContent = `Niveau: ${Math.round(percentage)}% (${data.volume}m³)`;
              
              // Mise à jour de la barre de progression
              const progressFill = document.getElementById('progressFill');
              progressFill.style.width = percentage + '%';
              progressFill.textContent = Math.round(percentage) + '%';
              
              // Gestion des alertes
              const alertBox = document.getElementById('alertBox');
              if (data.level === 0) {
                alertBox.textContent = '⚠️ ALERTE: Réserve vide!';
                alertBox.style.display = 'block';
              } else if (data.level === 6) {
                alertBox.textContent = '⚠️ ALERTE: Débordement!';
                alertBox.style.display = 'block';
              } else if (data.anomaly) {
                alertBox.textContent = '⚠️ ANOMALIE: Capteur défectueux probable';
                alertBox.style.display = 'block';
              } else {
                alertBox.style.display = 'none';
              }
              
              // Mise à jour du timestamp
              const now = new Date();
              document.getElementById('lastUpdate').textContent = 
                now.getHours().toString().padStart(2, '0') + ':' +
                now.getMinutes().toString().padStart(2, '0') + ':' +
                now.getSeconds().toString().padStart(2, '0');
            })
            .catch(error => console.error('Erreur:', error));
        }

        // Mise à jour initiale
        updateData();
        
        // Mise à jour toutes les secondes
        setInterval(updateData, 1000);

        // Afficher l'IP au chargement
        fetch('/api/info')
          .then(response => response.json())
          .then(data => {
            document.getElementById('ipAddress').textContent = data.ip;
          });
      </script>
    </body>
    </html>
  )";
  server.send(200, "text/html; charset=UTF-8", html);
}

void handleApiData() {
  StaticJsonDocument<512> doc;
  doc["level"] = 0;
  doc["volume"] = 0;
  doc["anomaly"] = false;
  
  JsonArray sensors = doc.createNestedArray("sensors");
  JsonArray pins = doc.createNestedArray("pins");
  
  // Calculer le niveau
  int currentLevel = 0;
  for (int i = NUM_SENSORS - 1; i >= 0; i--) {
    if (sensorValues[i] < THRESHOLD_WATER) {
      currentLevel = i + 1;
      break;
    }
  }
  
  // Ajouter les valeurs des capteurs
  for (int i = 0; i < NUM_SENSORS; i++) {
    sensors.add(sensorValues[i]);
    pins.add(SENSOR_PINS[i]);
  }
  
  // Vérifier les anomalies
  bool anomaly = false;
  for (int i = 0; i < currentLevel - 1; i++) {
    if (sensorValues[i] >= THRESHOLD_WATER) {
      anomaly = true;
      break;
    }
  }
  
  // Calculer le volume (20m³ total, 6 niveaux)
  float volume = 0;
  switch(currentLevel) {
    case 1: volume = 3.3; break;
    case 2: volume = 6.7; break;
    case 3: volume = 10.0; break;
    case 4: volume = 13.3; break;
    case 5: volume = 16.7; break;
    case 6: volume = 20.0; break;
    default: volume = 0; break;
  }
  
  doc["level"] = currentLevel;
  doc["volume"] = volume;
  doc["anomaly"] = anomaly;
  
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleApiInfo() {
  StaticJsonDocument<256> doc;
  doc["ip"] = WiFi.localIP().toString();
  doc["ssid"] = WiFi.SSID();
  doc["rssi"] = WiFi.RSSI();
  doc["uptime"] = millis() / 1000;
  
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleNotFound() {
  server.send(404, "text/plain", "404 - Page not found");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n[BOOT] Démarrage du système...");
  Serial.println("[BOOT] MODE CALIBRATION ACTIVÉ");
  Serial.println("[BOOT] Consultez le moniteur série pour les valeurs brutes des capteurs");
  Serial.println("[BOOT] À sec (air): valeurs devraient être hautes (~3000+)");
  Serial.println("[BOOT] Dans l'eau: valeurs devraient être basses (~500 ou moins)");
  
  send_0 = send_1 = send_2 = send_3 = send_4 = send_5 = send_6 = 0;
  
  Wire.begin(SDA_PIN, SCL_PIN);
  
  lcd.init();
  lcd.backlight();
  lcd.createChar(0, solidBlock);
  
  for (int i = 0; i < 4; i++) {
    pinMode(SENSOR_PINS[i], INPUT);
  }
  for (int i = 4; i < NUM_SENSORS; i++) {
    pinMode(SENSOR_PINS[i], INPUT);
  }
  
  // Initialiser le tableau de lissage
  for (int i = 0; i < NUM_SENSORS; i++) {
    for (int j = 0; j < SMOOTHING_FACTOR; j++) {
      sensorReadings[i][j] = 0;
    }
  }
  
  lcd.clear();
  lcd.setCursor(0, 0); 
  lcd.print("  Detecteur Niveau  ");
  lcd.setCursor(0, 1); 
  lcd.print("   d'Eau - 20m3     ");
  lcd.setCursor(0, 2); 
  lcd.print("  Demarrage...      ");
  lcd.setCursor(0, 3); 
  lcd.print("WiFi: Connexion...   ");
  delay(2000);
  
  connectToWiFi();
  
  server.on("/", handleRoot);
  server.on("/api/data", handleApiData);
  server.on("/api/info", handleApiInfo);
  server.onNotFound(handleNotFound);
  server.begin();
  
  Serial.println("[BOOT] Serveur web démarré sur le port 80");
  Serial.println("[BOOT] Accédez à http://" + WiFi.localIP().toString());
  Serial.println("[BOOT] Interface web en mode CALIBRATION");
  
  lcd.clear();
}

void loop() {
  server.handleClient();
  
  if (millis() - lastWiFiCheck > wifiCheckInterval) {
    lastWiFiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[WiFi] Reconnexion...");
      connectToWiFi();
    }
  }
  
  updateSensorReadings();

  int currentLevel = 0;
  for (int i = NUM_SENSORS - 1; i >= 0; i--) {
    if (sensorValues[i] < THRESHOLD_WATER) {
      currentLevel = i + 1;
      break;  
    }
  }

  // Affichage des valeurs brutes en continu pour calibration
  static unsigned long lastDebugTime = 0;
  if (millis() - lastDebugTime > 1000) {
    lastDebugTime = millis();
    
    Serial.print("Niveau: "); 
    Serial.print(currentLevel);
    Serial.print(" | RAW: ");
    for (int i = 0; i < NUM_SENSORS; i++) {
      Serial.print("GPIO");
      Serial.print(SENSOR_PINS[i]);
      Serial.print("=");
      Serial.print(sensorValues[i]);
      if (i < NUM_SENSORS - 1) Serial.print(" | ");
    }
    Serial.println();
  }

  if (currentLevel != lastDisplayedLevel) {
    lastDisplayedLevel = currentLevel;
    displayLevel(currentLevel);
  }
  
  delay(50);
}

// =====================================================================
// AFFICHAGE LCD
// =====================================================================
void displayLevel(int level) {
  switch(level) {
    case 0:
      if (send_0 == 0) {
        lcd.setCursor(0, 0); 
        lcd.print("Niveau:  0% (0.0m3) ");
        drawBar(0);
        lcd.setCursor(0, 2); 
        lcd.print("ALERTE: Reserve vide! ");
        send_0=1; send_1=0; send_2=0; send_3=0; send_4=0; send_5=0; send_6=0;
      }
      break;
    case 1:
      if (send_1 == 0) {
        lcd.setCursor(0, 0); 
        lcd.print("Niveau: 17% (3.3m3) ");
        drawBar(1);
        lcd.setCursor(0, 2); 
        lcd.print("Remplissage en cours  ");
        send_0=0; send_1=1; send_2=0; send_3=0; send_4=0; send_5=0; send_6=0;
      }
      break;
    case 2:
      if (send_2 == 0) {
        lcd.setCursor(0, 0); 
        lcd.print("Niveau: 33% (6.7m3) ");
        drawBar(2);
        lcd.setCursor(0, 2); 
        lcd.print("Remplissage en cours  ");
        send_0=0; send_1=0; send_2=1; send_3=0; send_4=0; send_5=0; send_6=0;
      }
      break;
    case 3:
      if (send_3 == 0) {
        lcd.setCursor(0, 0); 
        lcd.print("Niveau: 50% (10m3)  ");
        drawBar(3);
        lcd.setCursor(0, 2); 
        lcd.print("Remplissage en cours  ");
        send_0=0; send_1=0; send_2=0; send_3=1; send_4=0; send_5=0; send_6=0;
      }
      break;
    case 4:
      if (send_4 == 0) {
        lcd.setCursor(0, 0); 
        lcd.print("Niveau: 67% (13.3m3)");
        drawBar(4);
        lcd.setCursor(0, 2); 
        lcd.print("Remplissage en cours  ");
        send_0=0; send_1=0; send_2=0; send_3=0; send_4=1; send_5=0; send_6=0;
      }
      break;
    case 5:
      if (send_5 == 0) {
        lcd.setCursor(0, 0); 
        lcd.print("Niveau: 83% (16.7m3)");
        drawBar(5);
        lcd.setCursor(0, 2); 
        lcd.print("Remplissage en cours  ");
        send_0=0; send_1=0; send_2=0; send_3=0; send_4=0; send_5=1; send_6=0;
      }
      break;
    case 6:
      if (send_6 == 0) {
        lcd.setCursor(0, 0); 
        lcd.print("Niveau: 100% (20m3) ");
        drawBar(6);
        lcd.setCursor(0, 2); 
        lcd.print("ALERTE: Debordement!  ");
        send_0=0; send_1=0; send_2=0; send_3=0; send_4=0; send_5=0; send_6=1;
      }
      break;
  }
  
  lcd.setCursor(2, 3);
  lcd.print("Niveau d'eau");
}
