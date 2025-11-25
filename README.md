# 🌱 Luftqualität messen mit dem Sensirion SEN66 + ESPHome

Dieses Projekt bringt den **SEN66 Luftqualitätssensor** von Sensirion vollständig in ESPHome und damit in **Home Assistant**.  
Der SEN66 ist ein professioneller Sensor, der **Feinstaub, Luftfeuchtigkeit, Temperatur, VOC (Luftqualität), NOx (Stickoxide) und CO₂** messen kann – alles in einem einzigen Gerät.

Mit diesem Projekt kannst du ihn **per ESP32 und I²C direkt auslesen**, grafisch darstellen, Automationen auslösen und sogar die **Lüfterreinigung steuern**.

---

## 💨 Was kann der SEN66?

Der SEN66 erfasst gleich **mehrere Luftgüte-Aspekte gleichzeitig**:

| Messwert | Bedeutung |
|----------|-----------|
| 🌫 PM1–PM10 | Feinstaub in µg/m³ (z.B. Kerzen, Kochen, Rauch, Straßenverkehr) |
| 🔬 Number Concentration (p/cm³) | Anzahl einzelner Partikel in der Luft |
| 🫁 VOC IAQ | Luftqualität durch organische Gase (Haushaltschemikalien, Düfte, Kochen usw.) |
| 🔥 NOx Index | Belastung durch Stickoxide (Kochen mit Gas, Verkehr, Rauchen) |
| 🥽 CO₂ (ppm) | Luftqualität durch Atmung/Besucherzahl |
| 🌡 Temperatur | für Thermik + VOC/NOx Berechnung |
| 💧 Luftfeuchtigkeit | für Komfort + Sensor-Korrektur |

➡️ Damit liefert der SEN66 **mehr Informationen als ein CO₂-Sensor oder Feinstaubmessgerät allein**.

---

## 🖼 Darstellung in ESPHome

### 📊 Einzelne Messwerte (Sensor)

![Sensoren Beispiel](examples/Sensoren.jpg)

### 🕹️ Steuerung (Lüfterreinigung & Messung starten/stoppen)

![Steuerelemente Beispiel](examples/Steuerelemente.jpg)

---

## 🛠️ Funktionen dieser Firmware

✔️ vollständige Unterstützung aller SEN66-Sensorwerte  
✔️ **Number Concentration** in Partikel/cm³  
✔️ **manuelle Lüfterreinigung** mit 30-Sekunden-Cooldown  
✔️ **Messung ein-/ausschalten** per Home Assistant  
✔️ **VOC-Baseline speichern** (Lerndaten bleiben dauerhaft erhalten)  
✔️ stabiler Betrieb ohne Timeouts durch getaktete Abfragen  
✔️ Temperatur-Offset konfigurierbar (bei Stauwärme)

📌 **Andere Modelle (SEN54/SEN55)** könnten teilweise funktionieren, sind aber **nicht unterstützt**.

---

## 🔌 Hardware & Verdrahtung

| ESP32 | SEN66 |
|-------|-------|
| 3V3 | VCC |
| GND | GND |
| GPIO21 | SDA |
| GPIO22 | SCL |

🛑 **Wichtig:** Der SEN66 benötigt **3,3V**. Nicht mit 5V betreiben!

I²C-Adresse: `0x6B` (fest eingestellt)

---

## 📦 Installation

1. Projekt herunterladen
2. In ESPHome hinzufügen oder externes Component-Verzeichnis verlinken
3. **Beispiel-YAML aus dem Ordner `examples/` verwenden**
4. Auf den ESP32 flashen (OTA oder USB)

📎 Die vollständige Konfiguration findest du hier:  
`/examples/esp32_sen66.yaml`

---

## 📍 Aufstellungs-Tipps (für korrekte Messwerte)

Damit der Sensor **nicht falsche Werte misst**:

✔️ **Nicht direkt über Heizquelle**
✔️ **Nicht direkt am Fenster**
✔️ **Nicht hinter Möbeln**
✔️ **Nicht in Zugluft**
✔️ **Mind. 20–30 cm Abstand zu Wand**
✔️ **Nicht unter der Decke (CO₂ sammelt sich unten)**

🌬 Bei Geräten mit Abluft (Gasherd, Kamin, 3D-Drucker) → **SEN66 nicht direkt im Luftstrom platzieren**.

---

## 🧼 Lüfterreinigung: Warum und wofür?

Der SEN66 hat einen kleinen Lüfter, der regelmäßig Staub aus der Messkammer bläst.  
Diese Firmware ermöglicht:

🌀 **manuelle Autoreinigung per Knopfdruck**  
⏳ mit **30 Sekunden Sicherheits-Cooldown**  
❌ währenddessen keine Messungen → Werte gehen auf *Unbekannt*

Warum?  
▶ Sonst würde der Sensor falsche Werte liefern.

---

## 🔍 VOC & NOx: Was bedeutet das?

Der SEN66 verwendet **Sensirions eigene Luftqualitäts-Indizes (IAQ)**.

| Wert | Qualität |
|------|----------|
| 0–50 | Sehr gut |
| 51–100 | Gut |
| 101–150 | Mittel |
| 151–200 | Schlecht |
| >200 | Sehr schlecht |

🧪 **VOC**: Haushaltschemikalien, Ausgasung von Möbeln, Kochen, Parfum, Lösungsmittel  
🔥 **NOx**: Gasherde, Abgase, Rauchen, Bauen, Feuer/Ofen

Es sind **keine ppm-Werte**, sondern **Gesundheits-Indizes**.

---

## 🧠 Autokalibrierung (VOC-Baseline)

Der Sensor „lernt“ über Tage, welche Luftqualität im Raum normal ist.  
Diese Firmware:

✔ speichert Baseline  
✔ hält sie OTA-sicher  
✔ nutzt sie nach Neustart wieder

🔔 → **VOC wird dadurch extrem stabil und genau.**

---

🎉 Viel Spaß beim Messen, Atmen & Automatisieren!
