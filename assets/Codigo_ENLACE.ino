/*Manuel Bayonas Martínez - Proyecto 3, Aplicación interactiva - UOC 2026*/
/*Cargamos las tres librerías básicas que vamos a emplear*/
#include <Wire.h>/*Librería de comunicación I2C*/
#include <Adafruit_PN532.h>/*Librería de uso del módulo NFC*/
#include <DFPlayerMini_Fast.h>/*Librería de uso del reproductor de MP3*/

/* ================= CONFIGURACION ================= */

#define MAX_SLOTS 4/*Número máximo de pistas posibles en la playlist*/
#define UID_LEN   7/*Tamaño en bytes de las tarjetas NFC*/

/*Definimos los pines que se van a utilizar*/
const uint8_t ledPins[MAX_SLOTS] = {5, 6, 7, 8};/*LEDs indicadores de cada slot*/
const uint8_t playButtonPin = 10;/*Botón de reproducción y pausa*/
const uint8_t nextButtonPin = 16;/*Botón de salto a la siguiente pista*/
const uint8_t MP3_BUSY_PIN  = 4;/*Pin BUSY del DFPlayer. LOW indica reproducción*/

/* ================= LECTOR DE UIDS ================= */

#define MAX_READ_UIDS 20

uint8_t readUIDs[MAX_READ_UIDS][UID_LEN];/*UIDs leídos en modo generador*/
uint8_t readUIDCount = 0;/*Número de UIDs almacenados*/
bool uidReaderCardPresent = false;/*Evita lecturas repetidas en modo generador*/

/* ================= UIDS CARGADOS ================= */


/*UIDs de las tarjetas NFC asociadas a cada pista*/
const uint8_t UID_LIST[][UID_LEN] = {
  {0x53,0x52,0x74,0x68,0x42,0x00,0x01},/*Pista 1*/
  {0x53,0x51,0x74,0x68,0x42,0x00,0x01},/*Pista 2*/
  {0x53,0x50,0x74,0x68,0x42,0x00,0x01},/*Pista 3*/
  {0x53,0x47,0x74,0x68,0x42,0x00,0x01},/*Pista 4*/
  {0x53,0x48,0x74,0x68,0x42,0x00,0x01},/*Pista 5*/
  {0x53,0x49,0x74,0x68,0x42,0x00,0x01},/*Pista 6*/
  {0x53,0x4A,0x74,0x68,0x42,0x00,0x01},/*Pista 7*/
  {0x53,0x41,0x74,0x68,0x42,0x00,0x01},/*Pista 8*/
  {0x53,0x42,0x74,0x68,0x42,0x00,0x01},/*Pista 9*/
  {0x53,0x43,0x74,0x68,0x42,0x00,0x01},/*Pista 10*/
  {0x53,0x3F,0x74,0x68,0x42,0x00,0x01},/*Pista 11*/
  {0x53,0x37,0x74,0x68,0x42,0x00,0x01},/*Pista 12*/
  /*----LOS NUEVOS UIDS HAY QUE COLOCARLOS SOBRE ESTA LÍNEA----*/
};

const uint8_t UID_COUNT = sizeof(UID_LIST) / sizeof(UID_LIST[0]);/*Número de UIDs cargados*/

/*----CAMBIAR ESTA LÍNEA CUANDO SE AÑADEN ETIQUETAS A LA LISTA DE UIUDS----*/
uint8_t uidStartIndex = 13;/*Siguiente número de pista libre*/

/* ================= OBJETOS ================= */

Adafruit_PN532 nfc(-1, -1);/*Objeto NFC configurado para comunicación I2C*/
DFPlayerMini_Fast mp3;/*Objeto para controlar el reproductor MP3*/

/* ================= PLAYLIST ================= */

uint8_t playlistUID[MAX_SLOTS][UID_LEN];/*Lista de UIDs cargados*/
uint8_t playlistTracks[MAX_SLOTS];/*Número de pista asociado a cada UID*/
uint8_t slotCount = 0;/*Número de tarjetas guardadas*/
uint8_t playIndex = 0;/*Índice de la pista en reproducción*/
    b
/* ================= ESTADOS AUDIO ================= */

bool isPlaying = false;/*Indica si el sistema está reproduciendo*/
bool isPaused  = false;/*Indica si la reproducción está pausada*/
bool listEnded = false;/*Indica si la playlist ha llegado al final*/
bool trackStarted = false;/*Control junto con pin BUSY de estado de reproducción*/

/* ================= CONTROL USUARIO ================= */

bool userAction = false;/*Indica que acaba de producirse una acción del usuario*/
unsigned long userActionAt = 0;/*Momento en el que se produjo la acción*/
const unsigned long USER_ACTION_GUARD = 300;/*Añadimos una guarda para evitar saltos
de pista erróneos u otros cambios de estado*/

/* --- Cooldown de botones --- */
/*Evitamos pulsaciones repetidas*/
unsigned long lastButtonAction = 0;
const unsigned long BUTTON_COOLDOWN = 600;

const unsigned long UID_READER_HOLD_TIME = 4000;/*Tiempo necesario para entrar en modo lector de UIDs*/
bool uidReaderEntryTriggered = false;/*Evita acciones residuales al soltar los botones*/

/* ================= ESTADO NFC ================= */

/*Control de lectura de tarjetas para evitar lecturas repetidas*/
bool cardPresent = false;

/* ================= ESTADOS GENERALES ================= */

enum State {
  CAPTURE_SLOT,/*El sistema espera una tarjeta nueva*/
  WAIT_REMOVE,/*El sistema espera que se retire la tarjeta ya leída*/
  READY,/*La playlist está lista para reproducirse*/
  PLAYING,/*Sistema en reproducción*/
  UID_READER/*Modo especial para leer tarjetas y generar código*/
};

State state = CAPTURE_SLOT;

/* ================= LED ================= */

/*Reseteo de los LEDs*/
void allLedsOff() {
  for (uint8_t i = 0; i < MAX_SLOTS; i++) {
    digitalWrite(ledPins[i], LOW);
  }
}

/*Enciende los LED de las tarjetas ya cargadas*/
void showLoadedSlots() {
  allLedsOff();
  for (uint8_t i = 0; i < slotCount && i < MAX_SLOTS; i++) {
    digitalWrite(ledPins[i], HIGH);
  }
}

/*Apaga todos los LED y deja encendido el que se está reproduciendo*/
void showPlayingLed() {
  allLedsOff();
  if (playIndex < slotCount && playIndex < MAX_SLOTS) {
    digitalWrite(ledPins[playIndex], HIGH);
  }
}

/*Parpadeo de LED mientras espera lectura de tarjeta*/
void blinkActiveSlot(uint8_t index) {
  if (index >= MAX_SLOTS) return;

  static unsigned long last = 0;
  static bool on = false;

  if (millis() - last > 700) {
    on = !on;
    digitalWrite(ledPins[index], on);
    last = millis();
  }
}

/*Parpadeo simultáneo de todos los LEDs para indicar modo lector de UIDs*/
void blinkAllLeds() {
  static unsigned long last = 0;
  static bool on = false;

  if (millis() - last > 400) {
    on = !on;

    for (uint8_t i = 0; i < MAX_SLOTS; i++) {
      digitalWrite(ledPins[i], on);
    }

    last = millis();
  }
}

/* ================= UTILES ================= */

/*Compara dos UIDs byte a byte*/
bool uidEquals(const uint8_t *a, const uint8_t *b) {
  for (uint8_t i = 0; i < UID_LEN; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

/*Si la tarjeta ya se ha leído no se puede añadir de nuevo*/
bool uidAlreadyUsed(const uint8_t *uid) {
  for (uint8_t i = 0; i < slotCount; i++) {
    if (uidEquals(uid, playlistUID[i])) return true;
  }
  return false;
}

/*Convierte automáticamente un UID en número de pista*/
int uidToTrack(const uint8_t *uid) {
  for (uint8_t i = 0; i < UID_COUNT; i++) {
    if (uidEquals(uid, UID_LIST[i])) {
      return i + 1;
    }
  }

  return -1;/*Tarjeta no reconocida*/
}

/*Comprueba si un UID ya está cargado en el array principal*/
bool uidAlreadyLoaded(const uint8_t *uid) {
  for (uint8_t i = 0; i < UID_COUNT; i++) {
    if (uidEquals(uid, UID_LIST[i])) return true;
  }
  return false;
}

/* ================= SETUP ================= */

void setup() {
  Serial.begin(115200);

/*Configuración de los LEDs*/
  for (uint8_t i = 0; i < MAX_SLOTS; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }

/*Configuración de los botones*/
  pinMode(playButtonPin, INPUT_PULLUP);
  pinMode(nextButtonPin, INPUT_PULLUP);
  pinMode(MP3_BUSY_PIN, INPUT);

/*Inicializa el reproductor MP3*/
  Serial1.begin(9600);
  mp3.begin(Serial1);
  mp3.volume(10);

/*Inicializa el I2C*/
  Wire.begin();

/*Inicializa el NFC*/
  nfc.begin();
  nfc.SAMConfig();
}

/* ================= LOOP ================= */

/*Ejecución del programa en bucle*/
void loop() {

  handleButtons();/*Leemos los botones*/

  if (userAction && millis() - userActionAt > USER_ACTION_GUARD) {
    userAction = false;
  }/*Desactiva la protección tras una acción del usuario*/

/*Máquina de estados que decide la acción*/
  switch (state) {

    /*Slot vacío parpadeando y leyendo NFC*/
    case CAPTURE_SLOT:
      blinkActiveSlot(slotCount);
      readCard();
      break;

    /*Espera a que se retire la tarjeta ya leída*/
    case WAIT_REMOVE:
      showLoadedSlots();
      waitForRemoval();
      break;

    /*Cuando termina la lectura mantiene los slots cargados encendidos*/
    case READY:
      showLoadedSlots();
      break;

    /*Control de reproducción y de final de pista*/
    case PLAYING:
      showPlayingLed();
      handlePlayback();
      break;
    
    /*Modo de lectura de UIDs para asignarlos a pistas*/
    case UID_READER:
      blinkAllLeds();
      handleUIDReader();
      break;
  }
}

/* ================= NFC ================= */

/*Lee una tarjeta NFC y la añade a la playlist si es válida*/
void readCard() {
  if (slotCount >= MAX_SLOTS) {
    state = READY;
    return;
  }

/*Variables para almacenar los UID*/
  uint8_t uid[UID_LEN];
  uint8_t uidLength;

/*Permite leer una tarjeta durante un tiempo determinado para evitar bloqueos*/
  bool success = nfc.readPassiveTargetID(
    PN532_MIFARE_ISO14443A, uid, &uidLength, 50
  );

/*Control de lectura de tarjetas*/
  if (success && !cardPresent) {
    if (uidLength != UID_LEN) return;/*Descarta tarjetas con UID de otro tamaño*/
    if (uidAlreadyUsed(uid)) return;/*Descarta tarjetas ya usadas*/

    int track = uidToTrack(uid);
    if (track < 0) return;/*Descarta tarjetas no reconocidas*/

    cardPresent = true;
    memcpy(playlistUID[slotCount], uid, UID_LEN);
    playlistTracks[slotCount] = track;
    slotCount++;
    state = WAIT_REMOVE;
  }
}

/*Espera a que se retire la tarjeta antes de permitir una nueva lectura*/
void waitForRemoval() {
  uint8_t uid[UID_LEN];
  uint8_t uidLength;

  bool success = nfc.readPassiveTargetID(
    PN532_MIFARE_ISO14443A, uid, &uidLength, 50
  );

  if (!success && cardPresent) {
    cardPresent = false;
    state = (slotCount < MAX_SLOTS) ? CAPTURE_SLOT : READY;
  }
}

/* ================= BOTONES ================= */

void handleButtons() {

  /*Entrada al modo lector de UIDs manteniendo Play + Next durante más de 4 segundos*/
  static unsigned long bothButtonsAt = 0;
  static bool lastPlay = HIGH, lastNext = HIGH;
  static unsigned long playAt = 0, nextAt = 0;

  bool pNow = digitalRead(playButtonPin);
  bool nNow = digitalRead(nextButtonPin);

  bool bothPressed = (pNow == LOW && nNow == LOW);

  if (bothPressed && state != UID_READER) {
    if (bothButtonsAt == 0) {
      bothButtonsAt = millis();
    }

    if (millis() - bothButtonsAt > UID_READER_HOLD_TIME) {
      enterUIDReaderMode();
      bothButtonsAt = 0;
      uidReaderEntryTriggered = true;

      lastPlay = pNow;
      lastNext = nNow;
      return;
    }
  }

  if (!bothPressed) {
    bothButtonsAt = 0;
  }

  /*Si venimos de activar el modo lector, ignoramos la suelta de botones*/
  if (uidReaderEntryTriggered) {
    if (pNow == HIGH && nNow == HIGH) {
      uidReaderEntryTriggered = false;
    }

    lastPlay = pNow;
    lastNext = nNow;
    return;
  }

  /*Si estamos en modo lector, los botones los gestiona handleUIDReader()*/
  if (state == UID_READER) {
    lastPlay = pNow;
    lastNext = nNow;
    return;
  }

  /*Cooldown global de botones para evitar acciones dobles*/ 
  if (millis() - lastButtonAction < BUTTON_COOLDOWN) {
    lastPlay = pNow;
    lastNext = nNow;
    return;
  }

  /*Detecta el momento en el que se empieza a pulsar cada botón*/
  if (lastPlay == HIGH && pNow == LOW) playAt = millis();
  if (lastNext == HIGH && nNow == LOW) nextAt = millis();

  /*Comprueba si el botón Play se ha pulsado más de 2s para activar el reset*/
  if (lastPlay == LOW && pNow == HIGH) {
    if (millis() - playAt > 2000) {
      resetPlaylist();
    } else {
      handlePlay();
    }
    lastButtonAction = millis();
  }

  /*Comprueba si el botón Next se ha pulsado más de 2s para activar el reset*/
  if (lastNext == LOW && nNow == HIGH) {
    if (millis() - nextAt > 2000) {
      resetPlaylist();
    } else {
      handleNext();
    }
    lastButtonAction = millis();
  }

  /*Guarda el estado actual para compararlo en la siguiente vuelta*/
  lastPlay = pNow;
  lastNext = nNow;
}

/* ================= CONTROL AUDIO ================= */

void advanceToNextTrack() {

/*Detiene la reproducción actual, aplica un retardo y pasa a la siguiente pista*/
  mp3.stop();
  delay(60);

  playIndex++;

/*Si no quedan pistas, marca la lista como terminada y vuelve a READY*/
  if (playIndex >= slotCount) {
    isPlaying = false;
    isPaused = false;
    listEnded = true;
    trackStarted = false;
    state = READY;
    return;
  }

/*Reproduce la siguiente pista de la playlist*/
  mp3.play(playlistTracks[playIndex]);
  trackStarted = false;
  isPaused = false;
  state = PLAYING;
}

/*Gestiona Play/Pausa y reinicio de la playlist cuando ha terminado*/
void handlePlay() {
  if (slotCount == 0) return;/*Si no hay pistas guardadas no hace nada*/

  userAction = true;
  userActionAt = millis();

  if (!isPlaying) {
    if (listEnded) {
      playIndex = 0;
      listEnded = false;
    }

    mp3.play(playlistTracks[playIndex]);
    isPlaying = true;
    isPaused = false;
    trackStarted = false;
    state = PLAYING;
    return;
  }

/*Si ya está reproduciendo, alterna entre pausa y reanudación*/
  if (!isPaused) {
    mp3.pause();
    isPaused = true;
  } else {
    mp3.resume();
    isPaused = false;
  }
}

/*Salta a la siguiente pista si hay una reproducción activa*/
void handleNext() {
  if (!isPlaying) return;

  userAction = true;
  userActionAt = millis();

  advanceToNextTrack();
}

/*Comprueba el pin BUSY para detectar cuándo empieza y termina una pista*/
void handlePlayback() {
  if (!isPlaying || isPaused) return;
  if (userAction) return;

  int busy = digitalRead(MP3_BUSY_PIN);

  if (!trackStarted && busy == LOW) {
    trackStarted = true;
  }

  if (trackStarted && busy == HIGH) {
    trackStarted = false;
    advanceToNextTrack();
  }
}

/* ================= RESET ================= */

/*Detiene la reproducción y borra la playlist cargada*/
void resetPlaylist() {
  mp3.stop();
  delay(50);

  allLedsOff();

  slotCount = 0;
  playIndex = 0;
  cardPresent = false;
  isPlaying = false;
  isPaused = false;
  listEnded = false;
  trackStarted = false;

  state = CAPTURE_SLOT;
}

/* ============== LECTURA UIDS ============== */

/*Entra en modo lector de UIDs*/
void enterUIDReaderMode() {
  mp3.stop();
  delay(50);

  allLedsOff();

  readUIDCount = 0;
  uidReaderCardPresent = false;

  isPlaying = false;
  isPaused = false;
  listEnded = false;
  trackStarted = false;

  state = UID_READER;

  Serial.println();
  Serial.println("=== MODO LECTOR DE UIDS ===");
  Serial.println("Acerca tarjetas NFC.");
  Serial.println("Pulsa PLAY para generar el codigo.");
  Serial.println("Pulsa NEXT para salir.");
  Serial.println();
}

/*Gestiona el modo lector de UIDs*/
void handleUIDReader() {

  static bool lastPlay = HIGH;
  static bool lastNext = HIGH;

  bool playNow = digitalRead(playButtonPin);
  bool nextNow = digitalRead(nextButtonPin);

  readUIDCard();

  /*PLAY genera el código por Serial*/
  if (lastPlay == HIGH && playNow == LOW) {
    generateUIDCode();
  }

  /*NEXT sale del modo lector*/
  if (lastNext == HIGH && nextNow == LOW) {
    allLedsOff();
    state = CAPTURE_SLOT;

    Serial.println("Saliendo del modo lector de UIDs.");
    Serial.println();

    delay(300);
  }

  lastPlay = playNow;
  lastNext = nextNow;
}

/*Lee tarjetas NFC en modo generador y las guarda en RAM*/
void readUIDCard() {

  uint8_t uid[UID_LEN];
  uint8_t uidLength;

  bool success = nfc.readPassiveTargetID(
    PN532_MIFARE_ISO14443A,
    uid,
    &uidLength,
    50
  );

  if (success && !uidReaderCardPresent) {
    uidReaderCardPresent = true;

    if (uidLength != UID_LEN) {
      Serial.println("UID no compatible");
      return;
    }

    if (uidAlreadyLoaded(uid)) {
      Serial.println("Tarjeta ya existe en UID_LIST");
      return;
    }

    if (uidAlreadyRead(uid)) {
      Serial.println("Tarjeta repetida en esta sesion");
      return;
    }

    if (readUIDCount >= MAX_READ_UIDS) {
      Serial.println("Memoria de UIDs llena");
      return;
    }

    memcpy(readUIDs[readUIDCount], uid, UID_LEN);
    readUIDCount++;

    Serial.print("Tarjeta ");
    Serial.print(readUIDCount);
    Serial.println(" guardada");
  }

  if (!success && uidReaderCardPresent) {
    uidReaderCardPresent = false;
  }
}

/*Comprueba si un UID ya se ha leído en modo generador*/
bool uidAlreadyRead(const uint8_t *uid) {
  for (uint8_t i = 0; i < readUIDCount; i++) {
    if (uidEquals(uid, readUIDs[i])) return true;
  }
  return false;
}

/*Genera nuevas líneas para añadir a UID_LIST y actualiza el contador de pista libre*/
void generateUIDCode() {

  if (readUIDCount == 0) {
    Serial.println();
    Serial.println("No hay UIDs nuevos para generar.");
    Serial.println();
    return;
  }

  Serial.println();
  Serial.println("/* ===== NUEVAS LINEAS PARA PEGAR DENTRO DE UID_LIST ===== */");
  Serial.println();

  for (uint8_t i = 0; i < readUIDCount; i++) {

    uint8_t currentIndex = uidStartIndex + i;

    Serial.print("  {");

    for (uint8_t j = 0; j < UID_LEN; j++) {
      Serial.print("0x");

      if (readUIDs[i][j] < 0x10) {
        Serial.print("0");
      }

      Serial.print(readUIDs[i][j], HEX);

      if (j < UID_LEN - 1) {
        Serial.print(",");
      }
    }

    Serial.print("},/*Pista ");
    Serial.print(currentIndex);
    Serial.println("*/");
  }

  Serial.println();
  Serial.println("/* ===== SUSTITUIR TAMBIEN ESTA LINEA ===== */");
  Serial.println();

  Serial.print("uint8_t uidStartIndex = ");
  Serial.print(uidStartIndex + readUIDCount);
  Serial.println(";/*Siguiente número de pista libre*/");

  Serial.println();
}