#include <Wire.h>
#include <LiquidCrystal_I2C.h>

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
// CONFIGURATION DES CAPTEURS
// =====================================================================
// ESP-32D dispose de 12 entrées analogiques (ADC1 et ADC2)
// Broches ADC1 recommandées: GPIO 32-39
const int SENSOR_PINS[] = {32, 33, 34, 35, 36, 39};  // ADC1_4 à ADC1_7, ADC1_0, ADC1_3
const int NUM_SENSORS = 6;
const int THRESHOLD = 1500;  // Ajusté pour la plage 0-4095 de l'ESP-32D
int sensorValues[NUM_SENSORS];

// Flags pour éviter de redessiner l'écran en boucle
int send_0, send_1, send_2, send_3, send_4, send_5, send_6;

// Caractère personnalisé pour la barre
byte solidBlock[8] = {
  B11111, B11111, B11111, B11111, B11111, B11111, B11111, B11111
};

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

void setup() {
  Serial.begin(115200);  // ESP-32D utilise 115200 par défaut
  delay(1000);  // Attendre la stabilisation
  
  send_0 = send_1 = send_2 = send_3 = send_4 = send_5 = send_6 = 0;
  
  // Initialiser I2C avec les broches GPIO 21 et 22
  Wire.begin(SDA_PIN, SCL_PIN);
  
  // Initialiser l'écran LCD
  lcd.init();
  lcd.backlight();
  lcd.createChar(0, solidBlock);
  
  // Configurer les broches des capteurs
  for (int i = 0; i < NUM_SENSORS; i++) {
    pinMode(SENSOR_PINS[i], INPUT);
  }
  
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("  Detecteur Niveau  ");
  lcd.setCursor(0, 1); lcd.print("   d'Eau - 20m3     ");
  delay(2000);
  lcd.clear();
  
  Serial.println("Sketch ESP-32D - Détecteur de niveau d'eau démarré");
}

void loop() {
  // Lecture des 6 capteurs
  for (int i = 0; i < NUM_SENSORS; i++) {
    sensorValues[i] = analogRead(SENSOR_PINS[i]);
  }

  // =====================================================================
  // LOGIQUE TOLÉRANTE AUX PANNES
  // =====================================================================
  int currentLevel = 0;
  for (int i = NUM_SENSORS - 1; i >= 0; i--) {
    if (sensorValues[i] > THRESHOLD) {
      currentLevel = i + 1;
      break;  
    }
  }

  // Debug série détaillé
  Serial.print("Niveau detecte: "); Serial.print(currentLevel);
  Serial.print(" | GPIO32:"); Serial.print(sensorValues[0]);
  Serial.print(" GPIO33:"); Serial.print(sensorValues[1]);
  Serial.print(" GPIO34:"); Serial.print(sensorValues[2]);
  Serial.print(" GPIO35:"); Serial.print(sensorValues[3]);
  Serial.print(" GPIO36:"); Serial.print(sensorValues[4]);
  Serial.print(" GPIO39:"); Serial.print(sensorValues[5]);

  bool anomaly = false;
  for (int i = 0; i < currentLevel - 1; i++) {
    if (sensorValues[i] <= THRESHOLD) {
      anomaly = true;
      break;
    }
  }
  if (anomaly && currentLevel > 0) {
    Serial.print(" | ⚠ ANOMALIE: capteur defectueux probable");
  }
  Serial.println();

  // Affichage LCD
  displayLevel(currentLevel);
  delay(100);
}

// =====================================================================
// AFFICHAGE LCD (POURCENTAGE + CONTENANCE 20m3)
// =====================================================================
void displayLevel(int level) {
  switch(level) {
    case 0:
      if (send_0 == 0) {
        lcd.setCursor(0, 0); lcd.print("Niveau:  0% (0.0m3) "); // 20 caractères
        drawBar(0);
        lcd.setCursor(0, 2); lcd.print("ALERTE: Reserve vide! ");
        send_0=1; send_1=0; send_2=0; send_3=0; send_4=0; send_5=0; send_6=0;
      }
      break;
    case 1:
      if (send_1 == 0) {
        lcd.setCursor(0, 0); lcd.print("Niveau: 17% (3.3m3) "); // 20 caractères
        drawBar(1);
        lcd.setCursor(0, 2); lcd.print("Remplissage en cours  ");
        send_0=0; send_1=1; send_2=0; send_3=0; send_4=0; send_5=0; send_6=0;
      }
      break;
    case 2:
      if (send_2 == 0) {
        lcd.setCursor(0, 0); lcd.print("Niveau: 33% (6.7m3) "); // 20 caractères
        drawBar(2);
        lcd.setCursor(0, 2); lcd.print("Remplissage en cours  ");
        send_0=0; send_1=0; send_2=1; send_3=0; send_4=0; send_5=0; send_6=0;
      }
      break;
    case 3:
      if (send_3 == 0) {
        lcd.setCursor(0, 0); lcd.print("Niveau: 50% (10m3)  "); // 20 caractères
        drawBar(3);
        lcd.setCursor(0, 2); lcd.print("Remplissage en cours  ");
        send_0=0; send_1=0; send_2=0; send_3=1; send_4=0; send_5=0; send_6=0;
      }
      break;
    case 4:
      if (send_4 == 0) {
        lcd.setCursor(0, 0); lcd.print("Niveau: 67% (13.3m3)"); // 20 caractères
        drawBar(4);
        lcd.setCursor(0, 2); lcd.print("Remplissage en cours  ");
        send_0=0; send_1=0; send_2=0; send_3=0; send_4=1; send_5=0; send_6=0;
      }
      break;
    case 5:
      if (send_5 == 0) {
        lcd.setCursor(0, 0); lcd.print("Niveau: 83% (16.7m3)"); // 20 caractères
        drawBar(5);
        lcd.setCursor(0, 2); lcd.print("Remplissage en cours  ");
        send_0=0; send_1=0; send_2=0; send_3=0; send_4=0; send_5=1; send_6=0;
      }
      break;
    case 6:
      if (send_6 == 0) {
        lcd.setCursor(0, 0); lcd.print("Niveau: 100% (20m3) "); // 20 caractères
        drawBar(6);
        lcd.setCursor(0, 2); lcd.print("ALERTE: Debordement!  ");
        send_0=0; send_1=0; send_2=0; send_3=0; send_4=0; send_5=0; send_6=1;
      }
      break;
  }
  
  // --- FIN DE LA FONCTION ---
  lcd.setCursor(2, 3);
  lcd.print("Niveau d'eau");
}
