#include <SoftwareWire.h>
#include <LiquidCrystal_SoftI2C.h>

// =====================================================================
// CONFIGURATION I2C LOGICIEL
// =====================================================================
#define SDA_PIN 8
#define SCL_PIN 9 
SoftwareWire myWire(SDA_PIN, SCL_PIN);
LiquidCrystal_I2C lcd(0x27, 20, 4, &myWire, LCD_5x8DOTS);

// =====================================================================
// CONFIGURATION DES CAPTEURS
// =====================================================================
const int SENSOR_PINS[] = {A0, A1, A2, A3, A4, A5};
const int NUM_SENSORS = 6;
const int THRESHOLD = 30;
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
  Serial.begin(9600);
  send_0 = send_1 = send_2 = send_3 = send_4 = send_5 = send_6 = 0;
  
  lcd.begin();
  lcd.backlight();
  lcd.createChar(0, solidBlock);
  
  for (int i = 0; i < NUM_SENSORS; i++) {
    pinMode(SENSOR_PINS[i], INPUT);
  }
  
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("  Detecteur Niveau  ");
  lcd.setCursor(0, 1); lcd.print("   d'Eau - 20m3     ");
  delay(2000);
  lcd.clear();
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
  Serial.print(" | A0:"); Serial.print(sensorValues[0]);
  Serial.print(" A1:"); Serial.print(sensorValues[1]);
  Serial.print(" A2:"); Serial.print(sensorValues[2]);
  Serial.print(" A3:"); Serial.print(sensorValues[3]);
  Serial.print(" A4:"); Serial.print(sensorValues[4]);
  Serial.print(" A5:"); Serial.print(sensorValues[5]);

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
