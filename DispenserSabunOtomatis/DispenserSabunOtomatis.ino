// DispenserSabunOtomatis — Dispenser sabun otomatis hands-free
// (ESP32 + sensor proximity IR + relay + pompa mini submersible 5V)
// ---------------------------------------------------------------------------
// Dekatkan tangan ke sensor, sabun keluar sendiri tanpa menyentuh apa pun.
// Sensor proximity IR (modul obstacle avoidance / E18-D80NK, output digital)
// mendeteksi tangan di depannya, ESP32 menghidupkan pompa mini lewat relay
// selama waktu semprot yang sudah diatur, lalu pompa mati dan masuk masa
// jeda supaya sabun tidak keluar terus-menerus. LED hijau menyala selama
// pompa menyemprot, jadi pengguna tahu sabun sedang keluar.
//
// Cara pakai:
//   - Isi botol/tandon dengan sabun cair (encerkan sedikit bila terlalu
//     kental, pompa mini tidak kuat mendorong sabun yang sangat kental).
//   - Atur jarak deteksi sensor lewat trimpot di modul (putar sampai LED
//     modul menyala saat tangan berada sekitar 5-10 cm di depan sensor).
//   - Perintah Serial Monitor (115200 baud):
//       ketik 't' lalu <Enter>  ->  uji semprot sekali (tanpa tangan)
//       ketik 's' lalu <Enter>  ->  tampilkan status dan jumlah semprotan
//
// Wiring (lihat schematic.png):
//   Sensor proximity IR (output digital, aktif LOW)
//       OUT -> GPIO15, VCC -> 3V3, GND -> GND
//       Modul dicatu 3V3 supaya output-nya tetap logika 3,3V yang aman
//       untuk GPIO ESP32. Jangan catu modul dari 5V bila output-nya
//       ikut naik ke 5V.
//   Relay 1 channel (aktif LOW)
//       IN  -> GPIO26, VCC -> 5V (VIN), GND -> GND
//   Pompa mini submersible 5V
//       kabel merah (+) -> 5V (VIN), kabel hitam (-) -> relay NO
//       Relay COM -> GND. Arus pompa lewat relay, bukan lewat pin ESP32.
//   LED hijau (seri resistor 220 ohm)
//       anoda -> GPIO33, katoda -> GND
//   ESP32 dicatu USB 5V; pompa 5V diambil dari pin VIN (satu sumber 5V
//   yang sama, tapi arusnya lewat relay, bukan lewat regulator ESP32).
//
// Library yang dipakai: TIDAK ADA (hanya fungsi bawaan Arduino/ESP32).
// ---------------------------------------------------------------------------

// ------------------------- Konfigurasi -------------------------------------
// Pin sensor dan aktuator.
constexpr uint8_t PIN_SENSOR_IR = 15;  // OUT sensor IR, aktif LOW (tangan terdeteksi)
constexpr uint8_t PIN_RELAY     = 26;  // IN relay, aktif LOW
constexpr uint8_t PIN_LED       = 33;  // LED hijau, aktif HIGH

// Waktu dan ambang.
constexpr uint32_t WAKTU_SEMPROT_MS = 800;   // pompa jalan tiap semprotan
constexpr uint32_t WAKTU_JEDA_MS    = 1200;  // jeda antar semprotan
constexpr uint32_t INTERVAL_CEK_MS  = 100;   // baca sensor tiap 100 ms
constexpr uint32_t BAUD_SERIAL      = 115200;

// ------------------------- Status ------------------------------------------
// State pompa: false = diam, true = sedang menyemprot.
bool sedangMenyemprot = false;
uint32_t waktuSemprotMulai = 0;  // millis() saat semprotan dimulai
uint32_t waktuSemprotAkhir = 0;  // millis() saat semprotan terakhir selesai
uint32_t waktuCekTerakhir  = 0;  // millis() baca sensor terakhir
uint32_t jumlahSemprotan   = 0;  // penghitung semprotan sejak alat dinyalakan

// ------------------------- Fungsi ------------------------------------------
// Mulai semprotan: relay ON (LOW), LED hijau ON.
void mulaiSemprot() {
  digitalWrite(PIN_RELAY, LOW);
  digitalWrite(PIN_LED, HIGH);
  sedangMenyemprot = true;
  waktuSemprotMulai = millis();
  jumlahSemprotan++;
  Serial.print("Semprot #");
  Serial.print(jumlahSemprotan);
  Serial.println(" — sabun keluar...");
}

// Akhiri semprotan: relay OFF (HIGH), LED hijau OFF, catat waktu selesai.
void akhiriSemprot() {
  digitalWrite(PIN_RELAY, HIGH);
  digitalWrite(PIN_LED, LOW);
  sedangMenyemprot = false;
  waktuSemprotAkhir = millis();
  Serial.println("Semprot selesai.");
}

// Tampilkan status alat dan jumlah semprotan di Serial Monitor.
void tampilkanStatus() {
  Serial.println("--- Status dispenser sabun ---");
  Serial.print("Sensor IR (GPIO15): ");
  Serial.println(digitalRead(PIN_SENSOR_IR) == LOW
                     ? "tangan terdeteksi"
                     : "tidak ada tangan");
  Serial.print("Pompa: ");
  Serial.println(sedangMenyemprot ? "menyemprot" : "diam");
  Serial.print("Jumlah semprotan: ");
  Serial.println(jumlahSemprotan);
  Serial.print("Waktu semprot: ");
  Serial.print(WAKTU_SEMPROT_MS);
  Serial.println(" ms");
  Serial.print("Waktu jeda: ");
  Serial.print(WAKTU_JEDA_MS);
  Serial.println(" ms");
}

// Tangani satu perintah dari Serial Monitor ('t' = uji, 's' = status).
void tanganiPerintahSerial(char perintah) {
  switch (perintah) {
    case 't':
    case 'T':
      if (!sedangMenyemprot) {
        Serial.println("Uji semprot dari Serial.");
        mulaiSemprot();
      } else {
        Serial.println("Pompa sedang menyemprot, tunggu selesai.");
      }
      break;
    case 's':
    case 'S':
      tampilkanStatus();
      break;
    default:
      Serial.println("Perintah tidak dikenal. 't' = uji semprot, 's' = status.");
      break;
  }
}

// ------------------------- setup / loop ------------------------------------
void setup() {
  Serial.begin(BAUD_SERIAL);
  pinMode(PIN_SENSOR_IR, INPUT);
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_LED, OUTPUT);

  // Kondisi awal: pompa mati (relay OFF = HIGH), LED mati.
  digitalWrite(PIN_RELAY, HIGH);
  digitalWrite(PIN_LED, LOW);

  Serial.println("Dispenser sabun otomatis siap.");
  Serial.println("Dekatkan tangan ke sensor untuk menyemprot.");
  Serial.println("Ketik 't' = uji semprot, 's' = status.");
  tampilkanStatus();
}

void loop() {
  const uint32_t sekarang = millis();

  // Baca sensor tiap INTERVAL_CEK_MS (bukan tiap loop, supaya stabil).
  if (sekarang - waktuCekTerakhir >= INTERVAL_CEK_MS) {
    waktuCekTerakhir = sekarang;

    if (!sedangMenyemprot) {
      // Tangan terdeteksi (sensor LOW) dan masa jeda sudah lewat.
      const bool tanganTerdeteksi =
          digitalRead(PIN_SENSOR_IR) == LOW;
      const bool jedaSudahLewat =
          (sekarang - waktuSemprotAkhir) >= WAKTU_JEDA_MS;
      if (tanganTerdeteksi && jedaSudahLewat) {
        mulaiSemprot();
      }
    }
  }

  // Akhiri semprotan setelah WAKTU_SEMPROT_MS, tanpa menahan loop.
  if (sedangMenyemprot &&
      (sekarang - waktuSemprotMulai >= WAKTU_SEMPROT_MS)) {
    akhiriSemprot();
  }

  // Perintah Serial Monitor, dibaca tanpa menahan loop.
  while (Serial.available() > 0) {
    const char perintah = static_cast<char>(Serial.read());
    if (perintah == '\n' || perintah == '\r' || perintah == ' ') {
      continue;
    }
    tanganiPerintahSerial(perintah);
  }
}
