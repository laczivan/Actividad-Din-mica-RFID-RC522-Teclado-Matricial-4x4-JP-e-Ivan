#include <SPI.h>
#include <MFRC522.h>
#include <Keypad.h>

// ==============================
// CONFIGURACIÓN MÓDULO RFID RC522
// ==============================
#define RST_PIN 9
#define SS_PIN 10
MFRC522 mfrc522(SS_PIN, RST_PIN);

// ==============================
// CONFIGURACIÓN TECLADO MATRICIAL
// ==============================
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1','2','3','A'},  // A = Responder
  {'4','5','6','B'},  // B = Siguiente pregunta
  {'7','8','9','C'},  // C = Reiniciar trivia
  {'*','0','#','D'}   // D = Mostrar puntaje
};

byte rowPins[ROWS] = {8, 7, 4, 3};     // Filas conectadas a pines 8,7,4,3
byte colPins[COLS] = {2, 1, 0, A0};    // Columnas a pines 2,1,0,A0

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ==============================
// VARIABLES DEL JUEGO DE TRIVIA
// ==============================
struct Pregunta {
  String texto;
  char respuestaCorrecta;
  String opciones[4];
};

Pregunta trivia[] = {
  {"Capital de Italia?", 'B', {"A) Madrid", "B) Roma", "C) Paris", "D) Berlin"}},
  {"2+2*2=?", 'C', {"A) 6", "B) 8", "C) 6", "D) 4"}},
  {"Color del cielo?", 'A', {"A) Azul", "B) Verde", "C) Rojo", "D) Amarillo"}},
  {"Planeta rojo?", 'B', {"A) Tierra", "B) Marte", "C) Jupiter", "D) Saturno"}}
};

int preguntaActual = 0;
int puntaje = 0;
bool juegoActivo = false;
char respuestaUsuario = ' ';
String usuarioActual = "";

// ==============================
// BASE DE DATOS DE USUARIOS RFID
// ==============================
struct Usuario {
  String uid;
  String nombre;
  int highScore;
};

Usuario usuarios[] = {
  {"A1B2C3D4", "Jugador1", 0},
  {"E5F6G7H8", "Jugador2", 0},
  {"I9J0K1L2", "Jugador3", 0}
};

// ==============================
// CONFIGURACIÓN INICIAL
// ==============================
void setup() {
  Serial.begin(9600);
  SPI.begin();
  mfrc522.PCD_Init();
  
  Serial.println("=== SISTEMA DE TRIVIA INTERACTIVO ===");
  Serial.println("Acerca tu tarjeta RFID para comenzar...");
  Serial.println();
}

// ==============================
// BUCLE PRINCIPAL
// ==============================
void loop() {
  // Verificar si se acerca una tarjeta RFID
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    procesarRFID();
    delay(1000);
  }
  
  // Si el juego está activo, procesar teclado
  if (juegoActivo) {
    procesarTeclado();
  }
  
  delay(100);
}

// ==============================
// PROCESAMIENTO RFID
// ==============================
void procesarRFID() {
  String uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    uid.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : ""));
    uid.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  uid.toUpperCase();
  
  Serial.print("Tarjeta detectada: ");
  Serial.println(uid);
  
  // Buscar usuario en la base de datos
  bool usuarioEncontrado = false;
  for (int i = 0; i < 3; i++) {
    if (usuarios[i].uid == uid) {
      usuarioActual = usuarios[i].nombre;
      Serial.println("¡Bienvenido " + usuarioActual + "!");
      Serial.println("High Score: " + String(usuarios[i].highScore));
      usuarioEncontrado = true;
      break;
    }
  }
  
  if (!usuarioEncontrado) {
    usuarioActual = "Invitado";
    Serial.println("¡Bienvenido Invitado!");
  }
  
  iniciarTrivia();
}

// ==============================
// INICIAR NUEVA TRIVIA
// ==============================
void iniciarTrivia() {
  juegoActivo = true;
  preguntaActual = 0;
  puntaje = 0;
  respuestaUsuario = ' ';
  
  Serial.println();
  Serial.println("=== TRIVIA INICIADA ===");
  Serial.println("Presiona A para responder");
  Serial.println("Presiona B para siguiente pregunta");
  Serial.println("Presiona C para reiniciar");
  Serial.println("Presiona D para ver puntaje");
  Serial.println("=========================");
  
  mostrarPregunta();
}

// ==============================
// MOSTRAR PREGUNTA ACTUAL
// ==============================
void mostrarPregunta() {
  if (preguntaActual < 4) {
    Serial.println();
    Serial.println("Pregunta " + String(preguntaActual + 1) + "/4:");
    Serial.println(trivia[preguntaActual].texto);
    
    for (int i = 0; i < 4; i++) {
      Serial.println(trivia[preguntaActual].opciones[i]);
    }
    
    Serial.println("Selecciona tu respuesta (1-4):");
  } else {
    finalizarTrivia();
  }
}

// ==============================
// PROCESAMIENTO DEL TECLADO
// ==============================
void procesarTeclado() {
  char key = keypad.getKey();
  
  if (key) {
    Serial.println("Tecla: " + String(key));
    
    switch(key) {
      case '1': case '2': case '3': case '4':
        if (respuestaUsuario == ' ') {
          respuestaUsuario = key;
          Serial.println("Respuesta registrada: Opcion " + String(key));
        }
        break;
        
      case 'A': // Responder
        if (respuestaUsuario != ' ') {
          verificarRespuesta();
        } else {
          Serial.println("Primero selecciona una opcion (1-4)");
        }
        break;
        
      case 'B': // Siguiente pregunta
        siguientePregunta();
        break;
        
      case 'C': // Reiniciar trivia
        Serial.println("Reiniciando trivia...");
        iniciarTrivia();
        break;
        
      case 'D': // Mostrar puntaje
        Serial.println("Puntaje actual: " + String(puntaje) + "/4");
        break;
        
      case '*': case '#': case '0':
        // Teclas no utilizadas
        break;
    }
  }
}

// ==============================
// VERIFICAR RESPUESTA
// ==============================
void verificarRespuesta() {
  char respuestaCorrecta = trivia[preguntaActual].respuestaCorrecta;
  
  Serial.println();
  Serial.println("Tu respuesta: " + String(respuestaUsuario));
  Serial.println("Respuesta correcta: " + String(respuestaCorrecta));
  
  if (respuestaUsuario == respuestaCorrecta) {
    puntaje++;
    Serial.println("¡CORRECTO! 🎉");
  } else {
    Serial.println("❌ Incorrecto");
  }
  
  Serial.println("Puntaje: " + String(puntaje) + "/" + String(preguntaActual + 1));
  respuestaUsuario = ' ';
}

// ==============================
// SIGUIENTE PREGUNTA
// ==============================
void siguientePregunta() {
  preguntaActual++;
  respuestaUsuario = ' ';
  
  if (preguntaActual < 4) {
    mostrarPregunta();
  } else {
    finalizarTrivia();
  }
}

// ==============================
// FINALIZAR TRIVIA
// ==============================
void finalizarTrivia() {
  juegoActivo = false;
  
  Serial.println();
  Serial.println("=== TRIVIA FINALIZADA ===");
  Serial.println("Jugador: " + usuarioActual);
  Serial.println("PUNTUACION FINAL: " + String(puntaje) + "/4");
  
  if (puntaje == 4) {
    Serial.println("¡PERFECTO! 🌟");
  } else if (puntaje >= 3) {
    Serial.println("¡EXCELENTE! 👍");
  } else if (puntaje >= 2) {
    Serial.println("¡BUEN TRABAJO! 😊");
  } else {
    Serial.println("Sigue practicando 💪");
  }
  
  Serial.println("Acerca otra tarjeta para jugar nuevamente");
  Serial.println("==============================");
}