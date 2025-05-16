//EMISSOR
#define PINO_RX 13
#define PINO_TX 13
#define BAUD_RATE 1
#define HALF_BAUD 1000/(2*BAUD_RATE)

#define PINO_RTX 12
#define PINO_CTS 11
#define PINO_CLOCK 10

#include "Temporizador.h"

bool clock_state = false;

// Calcula bit de paridade - Par ou impar
bool bitParidade(char dado){
  int ones = 0;
  for(int i = 0; i < 8; i++) {
    if ((dado >> i) & 1) {
      ones++;
    }
  }
  return ones % 2 == 1;
}

// Switch clock
ISR(TIMER1_COMPA_vect){
  Serial.println("Clock switch");
  if (clock_state) {
    Serial.println("To low");
    digitalWrite(PINO_CLOCK, LOW);
    clock_state = false;
  }
  else {
    Serial.println("To High");
    digitalWrite(PINO_CLOCK, HIGH);
    clock_state = true;
  }
}

// Executada uma vez quando o Arduino reseta
void setup(){

  // Inicializa pinos
  pinMode(PINO_RTX, OUTPUT);
  pinMode(PINO_CTS, INPUT);
  pinMode(PINO_CLOCK, OUTPUT);
  pinMode(PINO_TX, OUTPUT);
  digitalWrite(PINO_RTX, LOW);
  digitalWrite(PINO_CLOCK, LOW);
  digitalWrite(PINO_TX, LOW);
  clock_state = false;

  //desabilita interrupcoes
  noInterrupts();
  // Configura porta serial (Serial Monitor - Ctrl + Shift + M)
  Serial.begin(9600);

  // Configura timer
  configuraTemporizador(BAUD_RATE);
  // habilita interrupcoes
  interrupts();
}

void loop ( ) {
  // Início do envio do pacote
  if (Serial.available() > 0) {
    // Define os dados
    char incomingByte = Serial.read();
    Serial.print(incomingByte);
    bool parity = bitParidade(incomingByte);

    // Inicia o RTX
    digitalWrite(PINO_RTX, HIGH);

    // Espera o CTS
    while(digitalRead(PINO_CTS) == LOW) {;}
    
    // Inicia o Clock
    iniciaTemporizador();
    

    for(int i = -1; i < 11; i++){
      // Checa por descidas no CLOCK
      Serial.println("Inicio espera clock");
      while(!clock_state) {;}
      while(clock_state) {;}
      Serial.println("Descida clock");

      // Start bit
      if (i == -1) {
        digitalWrite(PINO_TX, HIGH);
        Serial.println("Start bit");
      }

      // Write data
      else if (i <= 7) {
        bool bit = (incomingByte >> i) & 1;
        if (bit) {
          digitalWrite(PINO_TX, HIGH);
        }
        else  {
          digitalWrite(PINO_TX, LOW);
        }
        Serial.println("Sent bit");
      }

      // Write parity
      else if (i == 8) {
        if (parity) {
          digitalWrite(PINO_TX, HIGH);
        }
        else  {
          digitalWrite(PINO_TX, LOW);
        }
        Serial.println("Wrote Parity");
      }

      // Write end bit
      else if (i == 9){
        digitalWrite(PINO_TX, HIGH);
        Serial.println("End bit");
      }
    }
    Serial.println("End for");

    // Fecha o TX, RTX e para o CLOCK
    
    digitalWrite(PINO_TX, LOW);
    digitalWrite(PINO_RTX, LOW);
    paraTemporizador();
    digitalWrite(PINO_RTX, LOW);
    Serial.println("End transmission");
  }  
}
