/************************************************************************************
*************************************************************************************
* He aquí el código fuente implementado para el Trabajo Fin de Grado con título:
*                 "Sistemas fotovoltaico Orientado a Docencia"
* Realizado por:
*                           Óscar Martínez Cano
*
* Este programa genera un par de señales PWM, una con el timer1, que controlará la
* conmutación de un convertidor elevador en función de medidas de tensión y corriente,
* con el fin de que el sistema opere a máxima potencia, y otra con el timer2, que 
* controlará la conmutación de un inversor de puente completo. Además el sistema 
* contará con * comunicación serie con un PC.
* 
* Este software es de código abierto y puede ser consultado libremente.
*************************************************************************************
************************************************************************************/

/*************************Variables y constantes************************************/

int cnt20u = 0;         //Contador de 20 us
int cnt1m = 0;          //Contador de 1ms
int cambio = 0;         //Activa la segunda función de la rutina de overflow
//Valores de los ciclos de trabajo para obtener un seno al filtrar.
const int d1[11]=  {40, 35, 30, 25, 20, 20, 20, 25, 30, 35, 35}; //Señal en alta
const int d2[9] =      {10, 20, 25, 30, 35, 35, 30, 25, 15};     //Señal en baja
//Para obtener un valor exacto de V e I tomo p muestras en cada duty y promedio
const int p = 1000;     //Número de muestras a promediar
float Ip=0;             //Almacena la corriente media
float Vp=0;             //Almacena la tensión media
//El ciclo de trabajo varia de 0 a 33, dónde 33 equivale a D = 100%
const int medidas = 34; //Una medida más para la corriente en abierto 
float Iin = 0;          //Corriente de entrada
float Vin = 0;          //Tensión de entrada
float Pin = 0;          //Potencia de entrada
float Pmaxima = 0;      //Para calcular la potencia máxima
int DpMax=0;            //Almacena el duty que da la potencia maxima

int cnt = 0; //contador de medidas
int dato=0; //dato recibido por el puerto serie
/***********************************************************************************/

void setup(){
  cli();               // Detiene las interrupciones
  Serial.begin(9600);  //Inicia la comunicación serie (9600 bits/seg)
  DDRB =  1 << DDB0 | 1 << DDB1 | 1<<DDB4; // 8 salida,9 salida(PWM, DC-DC),12 salida
  DDRD =  1 << DDD3 | 0 << DDD2;//3 salida(PWM, inv),2 entrada (botón)
  PORTD = 1 << PORTD2; // pin 2 a 1 (pullup), para detectar circuito cerrado (botón)
  PORTB = 1 << PORTB4; //12 a 1, pone el sistema en cortocircuito para que funcione.
   
  /*Timer 2. Genera la PWM del inversor y la señal de 50 Hz
    COM Clear OC2B on compare Match, set OC2B at BOTTOM (inverting) '1'
    WGM Fast PWM from bottom to OCRA, CS No prescaling */
  TCCR2A = 1<<COM2B1 | 1<<COM2B0 | 1<<WGM21 | 1<<WGM20;
  TCCR2B = 1<<WGM22 | 0<<CS22 | 1<<CS21 | 0<<CS20;            //P = 8
  OCR2A = 40;
  
  /*Timer 1. Genera la PWM del convertidor elevador.
    Phase & frequency correct PWM”,con final de cuenta (TOP)= ICR1
    Modo (“Clear OC1A/OC1B on Compare Match when upcounting
    Set OC1A/OC1B on Compare Match when downcounting*/
  TCCR1A = 1<<COM1A1 | 0<<COM1A0 | 1<<COM1B1 | 0<<COM1B0 | 0<<WGM11 | 0<<WGM10;
  TCCR1B = 1<<WGM13 | 0<<WGM12 | 0<<CS12 | 1<<CS11 | 0<<CS10; //P = 8;
  ICR1 = 33; // periodo = 0.033 ms -> 30kHz
  OCR1A = 16; //indica el duty (15 Por ejemplo ~50%)
  
  /* Activar interrupciones */
  TIMSK2 = 1 << TOIE0; //activar overflow interrupt on timer 2
  asm("SEI");          //enable global interrupt sei();
}


void loop(){
  /*Si pulso e botón o si recibo dato calibramos*/  
  if((PIND & 1<<DDD2) == 0x00 || dato ==1) calibrar();
  //if (dato==1) calibrar();
}

/*Esta función se ejecuta cuando hay un evento por el puerto serie.
Mientras haya datos disponibles lee, si el dato recibido en un dígito lo
convierto a entero*/
void serialEvent() {
  while (Serial.available()) {
    // Conseguir un nuevo byte
    char inChar = (char)Serial.read(); 
    //Si es digito lo convierto a entero
    if( isDigit(inChar) ){
      dato = (inChar - '0');
      delay(50);
      //Envío el número de medidas que voy a tomar.
      Serial.println(medidas);
    }
  }
}

/* Esta función mide la tensión y la corriente, tomando p muestras para cada
   medida del ciclo de trabajo y calcula la potencia de entrada */
void medir(){
  int corriente;
  int tension;
  //Tomo p muestras de una misma medida y hago el promedio
  for(int i=0; i<p; i++){
    corriente = analogRead(A2);
    tension = analogRead(A1);
    //Almaceno la suma de las muestras
    Ip = Ip + corriente; 
    Vp = Vp + tension;
  }
  //Divido por el total de muestras para hacer el promedio
  corriente = Ip/p; 
  tension = Vp/p;
  // Pongo a cero para la siguiente calibración
  Ip=0; 
  Vp=0;
  //Aplico una conversión y  factor de escala
  Iin = corriente * (5.0 / 1023.0) * 21.7; //En mA 
  Vin = tension   * (5.0 / 1023.0) * 2.45; //En V
  Pin = Iin * Vin;                         //En mW
}

/* Esta función envía las datos de medidos por el puerto serie al PC */
void printar(){   
  delay(50);
  Serial.println(Iin);
  delay(50);
  Serial.println(Vin);
  delay(50);
  Serial.println(Pin);
}

/* Esta función se encarga de automatizar el proceso de calibración.
   Para ello vario el ciclo de trabajo, realizo la medida, calculo
   la potencia máxima y almaceno el Duty que proporciona la potencia
   máxima. Y envío todos los datos                                  */
void calibrar(){
  OCR1A = 0;
  //La primera medida la tomo en circuito abierto.
  PORTB = 0 << PORTB4;
  delay(500);
  medir();
  //Cierro el circuito, envío los datos y empiezo a medir.
  PORTB = 1 << PORTB4;
  printar();
  
  for (int i=0; i<medidas; i++){
    OCR1A = i;
    delay(500);
    medir();
    if (Pin>Pmaxima){
      Pmaxima= Pin;
      DpMax = i;
    }
  printar();
  }
  //Selecciono el Duty que proporciona la potencia máxima.
  OCR1A = DpMax;
  //Pongo las variables a cero para la siguiente calibración.
  dato = 0; 
  Pmaxima = 0;
}

/* Esta función se ejecuta cada vez que se desborda el timer 2 (20us).
   La rutina tiene dos estados; señal de 50Hz en baja y en alta y genera una señal
   PWM que controla el inversor de puente completo. */
ISR(TIMER2_OVF_vect){
  
  if (cambio == 0){ //Señal en baja
    // Cada 20us actualizo el valor del ciclo de trabajo
    if (cnt20u == 0) OCR2B = d2[cnt1m];
    //Cada vez que desborda incremento el contador de 20us
    cnt20u++;
    //Cada 50*20us es 1ms
    if (cnt1m != 8 && cnt20u == 49){ 
      cnt1m++; 
      cnt20u = 0;
    }
    //El micro segundo de antes pongo la PWM a 0 porque tengo un retardo acumulado.
    else if (cnt1m == 8 && cnt20u == 48 ){
      OCR2B = 40; 
    }
    //Cuando llego a 9ms tengo que esperar que terminen los 50us
    else if (cnt1m == 8 && cnt20u == 49 ){
      cnt1m=0;
      cnt20u = 0;
      cambio = 1;
      PINB = 1<<PORTB0;//Niega el valor del puerto B0 para generar los 50Hz
    }
  }
  else{ //Señal en alta. El funcionamiento es similar
    //Cada 20us actualizo el valor del ciclo de trabajo
    if (cnt20u == 0) OCR2B = d1[cnt1m];
    //Incremento el contador de 20us en cada overflow
    cnt20u++;
    //Cada 50*20us es 1ms
    if (cnt1m != 10 && cnt20u == 49){ 
      cnt1m++; 
      cnt20u = 0;
    }
    //Cuando llego a 10ms tengo que esperar que terminen los 50us
    else if (cnt1m == 10 && cnt20u == 49){ 
      cnt1m=0;
      cnt20u = 0;
      cambio = 0;
      PINB = 1<<PORTB0;//Niega el valor del puerto B0
      OCR2B = d2[cnt1m];
    }
  }
}
