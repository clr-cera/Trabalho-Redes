//RECEPTOR
#define PINO_RX 13
#define PINO_TX 13
#define BAUD_RATE 1
#define HALF_BAUD 1000/(2*BAUD_RATE)

#define PINO_RTX 12
#define PINO_CTS 11
#define PINO_CLOCK 10

//1 se for paridade par
#define paridade 1

#include "Temporizador.h"

// Calcula bit de paridade - Par ou impar
bool bitParidade(char dado){
  int ones = 0;
  for(int i = 0; i < 8; i++) {
    if ((dado >> i) & 1) {
      ones++;
    }
  }
  return ones % 2 == paridade;
}

// Rotina de interrupcao do timer1
// O que fazer toda vez que 1s passou?
ISR(TIMER1_COMPA_vect){
  //>>>> Codigo Aqui <<<<
}

// Executada uma vez quando o Arduino reseta
void setup(){
  //desabilita interrupcoes
  noInterrupts();
  // Configura porta serial (Serial Monitor - Ctrl + Shift + M)
  Serial.begin(9600);
  // Inicializa TX ou RX
  pinMode(PINO_RTX, INPUT);
  pinMode(PINO_CTS, OUTPUT);
  pinMode(PINO_CLOCK, INPUT);
  pinMode(PINO_TX, INPUT);

  digitalWrite(PINO_CTS, LOW);

  // Configura timer
  configuraTemporizador(BAUD_RATE);
  // habilita interrupcoes
  interrupts();
}

// O loop() eh executado continuamente (como um while(true))
void loop ( ) {
  if (digitalRead(PINO_RTX) == HIGH){
    Serial.println("Subida do RTX detectada");
    digitalWrite(PINO_CTS, HIGH);
    char incomingByte = 0;
    //FLAG DE ERRO
    bool ERRO = false;

    Serial.println("Recebimento iniciado");
    for(int i = 0; i < 11; i++) {

      

      //Espera uma borda de subida
      while(digitalRead(PINO_CLOCK) == HIGH) {;}
      while(digitalRead(PINO_CLOCK) == LOW) {;}
      Serial.println("Próximo bit");

      bool bit = digitalRead(PINO_TX) == HIGH;
      Serial.println(bit);

      if (i == 0){
        //Verifica o bit alto para inciar recebimento
        if(bit != true)
          i--;
      } else if (i >= 1 && i <= 8) {
        // Escreve no byte
        if (bit) {
          bitSet(incomingByte, i - 1);
        } else {
          bitClear(incomingByte, i - 1);
        }
      } else if (i == 9){
        //Verifica o bit de paridade
        if(bitParidade(incomingByte) != bit){
          Serial.println("Falha de paridade");
          ERRO = true;
        }
      } else if (i == 10){
        if(bit == true && ERRO == false){
          Serial.println("Caracter lido:");
          Serial.println(incomingByte);
        } else {
          Serial.println("Falha de sincronismo");
        }
      }
      if(digitalRead(PINO_RTX) == LOW) break;
    }
    while(digitalRead(PINO_RTX) == HIGH) {;}
    digitalWrite(PINO_CTS, LOW);
  }
}
