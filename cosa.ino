#include "HX711.h"

// Variables para conexión de celda de carga
#define DOUT  A1
#define CLK  A0

// Pin de conexión PWM para motor de minibomba
int motor_pin = 9;

// Definición de constantes
HX711 balanza;
// Escalas precalibradas (anteriormente)
float escala5 = 1001.88;
float escala250 = 1068.39;
float escala500 = 1071.89;
float escala750 = 1079.19;

// Definición de variables
float toma1 = 0;
float toma2 = 0;
float escala = 0; // variable para calcular la escala que se va a calibrar    

float densidad_liquido = 1; // MODIFICAR de acuerdo a la densidad del líquido 1g/ml
float volumen_liquido_deseado = 0; // MODIFICAR de acuerdo a lo que se quiera 

float masa_calibracion = 0; // MODIFICAR de acuerdo a la masa con la que está calibrando
float masa_muestra = 0; // MODIFICAR de acuerdo a lo que se pida de peso de muestra sólida que se va a tener en la balanza
float masa_actual = 0; // variable para guardar peso actual medido en el sistema

int vel_motobomba = 0;


void setup() 
{ 
  // Inicializacion 
  Serial.begin(9600);
  balanza.begin(DOUT, CLK);
  pinMode(motor_pin, OUTPUT);
  
  Serial.println("Inicializando balanza");
  delay(1000);

  Serial.println("Por favor no poner nada sobre la balanza");
  delay(2000);
  
  Serial.println("Por favor poner recipiente sobre la balanza");
  delay(5000); 
  
  Serial.println("Inicio de tara"); // Esta tara la hace sobre el peso inicial de la balanza y el peso del recipiente, eso se puede cambiar
  delay(1000);
  balanza.set_scale(); 
  balanza.tare(50); // Se toman cien muestras, para calcular el proceso de tara (ajusta el cero de acuerdo al peso del recipiente)
  Serial.println("Fin de tara");
  delay(1000);

  Serial.println("Inicializando la calibración de la balanza");
  delay(1000);

  //calibracion();
  escala = 1123;
  Serial.print("Escala calibrada: ");
  Serial.println(escala);
  
  Serial.println("SISTEMA LISTO PARA MEDIR Y DOSIFICAR LIQUIDO");
  delay(1000);
}


void loop() 
{
  Serial.println("Si desea dosificar líquido escriba 1 en esta terminal, de lo contrario, no escriba nada");
  masa_actual = medir_masa_actual(); // Esta función mide el peso con la escala obtenida directamente en este código

  if(Serial.available()> 0) // Revisa si el usuario introdujo un comando en la interfaz para dosificar
  {
    int instr = Serial.parseInt();
    if (instr == 1)
    {
        masa_muestra = masa_actual;
        Serial.print("Para al dosificación, se tiene actualmente una muestra medida de: ");
        Serial.print(masa_muestra);
        Serial.println(" g");
        
        delay(2000);
        
        Serial.println("¿cuánto volumen de líquido quiere dosificar (ml)?: ");
        while (!(Serial.available() > 0))
        {
          // Espera hasta que ingresen un valor en la interfaz  
        }
  
        volumen_liquido_deseado = Serial.parseInt();
        Serial.print("Volumen de liquido deseado: ");
        Serial.print(volumen_liquido_deseado);
        Serial.println(" ml");
        delay(2000);

        Serial.println("Dosificando...");
        
        //control_onoff_motobomba(masa_actual); 
        control_p_motobomba(masa_actual);
        //control_pi_motobomba(masa_actual);

        Serial.println("Dosificación terminada");
    }
    else if (instr == 2)
    {
      Serial.println("Inicio de tara"); // Esta tara la hace sobre el peso inicial de la balanza y el peso del recipiente, eso se puede cambiar
      delay(1000);
      balanza.set_scale(escala); 
      balanza.tare(50); // Se toman cien muestras, para calcular el proceso de tara (ajusta el cero de acuerdo al peso del recipiente)
      Serial.println("Fin de tara");
      delay(1000);
    }
  }
  
}


void control_onoff_motobomba(float masa_Inicial) // Para control ON/OFF
{
  float referencia = (volumen_liquido_deseado*densidad_liquido) + masa_muestra; 
  float error = referencia - masa_Inicial; // Error = referencia - actual
  Serial.print("El error actual es: ");
  Serial.print(error);
  Serial.println(" g");

  while (error > 5) // Enciende la bomba mientras el error sea mayor al rango aceptable
  {
    analogWrite(motor_pin, 255);
    masa_actual = medir_masa_actual(); // medir_peso_actual();
    error = referencia - masa_actual;
    Serial.print("El error actual es: ");
    Serial.print(error);
    Serial.println(" g");
  }
  
  analogWrite(motor_pin, 0);
}


void control_p_motobomba(float masa_Inicial) // Para control proporcional
{
  float referencia = (volumen_liquido_deseado*densidad_liquido) + masa_muestra;
  float error = referencia - masa_Inicial; // Error = referencia - actual
  Serial.print("El error actual es: ");
  Serial.print(error);
  Serial.println(" g");

  while (error > 10) // Maneja una velocidad de la motobomba proporcional al error
  {
    vel_motobomba = map(abs(error), 0, volumen_liquido_deseado, 70, 255);
    analogWrite(motor_pin, vel_motobomba);
    
    masa_actual = medir_masa_actual(); // medir_peso_actual();
    error = referencia - masa_actual;
    Serial.print("El error actual es: ");
    Serial.print(error);
    Serial.println(" g");
  }
  
  analogWrite(motor_pin, 0);
}


void control_pi_motobomba(float masa_Inicial) // Para control proporcional
{
  float kp = 255/(volumen_liquido_deseado*densidad_liquido), ki = 0.5;
  unsigned long currentTime, previousTime;
  double elapsedTime;
  double lastError, cumError;
  float referencia = (volumen_liquido_deseado*densidad_liquido) + masa_muestra;
  float error = referencia - masa_Inicial; // Error = referencia - actual
  Serial.print("El error actual es: ");
  Serial.print(error);
  Serial.println(" g");

  while (error > 5) // Maneja una velocidad de la motobomba proporcional al error
  {
    currentTime = millis();                               // obtener el tiempo actual
    elapsedTime = (double)(currentTime - previousTime);     // calcular el tiempo transcurrido
    
    masa_actual = medir_masa_actual();
    error = referencia - masa_actual;
    cumError += error * elapsedTime;   // calcular la integral del error
    
    Serial.print("El error actual es: ");
    Serial.print(error);
    Serial.println(" g");

    vel_motobomba = kp*error + ki*cumError;     // calcular la salida del PI
 
    lastError = error;  // almacena error anterior
    previousTime = currentTime; 
    
    analogWrite(motor_pin, vel_motobomba);
  }
  
  analogWrite(motor_pin, 0);
}


float medir_masa_actual() // Función para medir masa con una sola escala calibrada en este mismo código
{
  balanza.set_scale(escala); // Establece la escala calibrada
  toma1 = balanza.get_units(5);  // Pesa y entrega el valor en gramos

  if (toma1 > 1000) // revisa que se esté pesando máximo 1000g = 1kg
  {
    Serial.println("Fuera de rango de medición");
  }
  else if (toma1 < 0) // su hay medidas negativas se ajusta la tara para corregirlas
  {
    Serial.println("Ajustando tara...");
    //balanza.tare(10);
    delay(5);
  }

//  toma1 = balanza.get_units(10);  /*Pesa y entrega el valor en gramos*/
//  delay(5);
//  toma2 = balanza.get_units(1);
//  
//  while (abs(toma1 - toma2) > 10)  /*Espera a que la medición se estabilice*/
//  {
//     toma1 = toma2;
//     toma2 = balanza.get_units(10);
//     delay(5);
//  }
  
  Serial.print("Masa actual es: ");
  Serial.print(toma1); // Imprime la medicion en consola
  Serial.println("g");
  
  return toma1;
}


void calibracion(void) // Función para calibrar con 1 peso de referencia
{
  Serial.println("Por favor ingresar la masa con la que va a calibrar entre 1g y 950g");
  Serial.println(".......");
  delay(1000);
  
  while (!(Serial.available() > 0))
  {
    // Espera hasta que ingresen una masa de calibracion  
  }
  
  masa_calibracion = Serial.parseInt();
  
  Serial.print("La masa de calibración ingresada es: ");
  Serial.print(masa_calibracion);
  Serial.println("g");

//  Serial.print("El valor actual medido sin escala es: ");
//  Serial.println(balanza.get_value(10));
//  delay(500);
  
  Serial.println("Colocar sobre la balanza la masa con la que se desea calibrar");
  delay(6000);
  masa_actual = balanza.get_value(50); // Toma 50 medidas de la muestra
  Serial.print("El valor medido sin escala es: ");
  Serial.println(masa_actual);
  delay(2000);

  escala = masa_actual/masa_calibracion;      /*Asi se calcula la escala de la balanza para gramos, si quisieramos que la escala estuviera en kilogramos, se le ingresa el peso en kilogramos por la terminal*/  
}


//float dar_volumen_deseado(float volumen_deseado) // Función extra que se hizo en la caracterización de la bomba
//{
//  float tiempo_motor = (volumen_deseado + 8.3375)/0.0467; // estos parámetros se calcularon con la caracterización de la bomba
//  Serial.print("El motor debe estar encendido por: ");
//  Serial.print(tiempo_motor);
//  Serial.println(" ms");
//  return tiempo_motor;
//}


//float medir_peso_actual() // Esta función mide el peso basado en las 5 escalas de calibración que teníamos antes
//{
//  balanza.set_scale(escala500);
//  
//  toma1 = balanza.get_units(20);  /*Pesa y entrega el valor en gramos*/
//
//  if (toma1 >= 0 && toma1 <= 30)
//  {
//    Serial.println("Entro a escala 5");
//    balanza.set_scale(escala5);
//  }
//  else if (toma1 > 30 && toma1 <= 375)
//  {
//    Serial.println("Entro a escala 250");
//    balanza.set_scale(escala250);
//  }
//  else if (toma1 > 375 && toma1 <= 615)
//  {
//    Serial.println("Entro a escala 500");
//    balanza.set_scale(escala500);
//  }
//  else if (toma1 > 615 && toma1 <= 1000)
//  {
//    Serial.println("Entro a escala 750");
//    balanza.set_scale(escala750);
//  }
//  else if (toma1 < 0)
//  {
//    Serial.println("Ajustando tara...");
//    balanza.tare(10);
//    delay(5);
//  }
//  else
//  {
//    Serial.println("Fuera de rango de medición");
//  }
//
//  toma1 = balanza.get_units(20);  /*Pesa y entrega el valor en gramos*/
//  delay(10);
//  toma2 = balanza.get_units(1);
//  
//  while (abs(toma1 - toma2) > 10)  /*Hacer una medicion precisa*/
//  {
//     toma1 = toma2;
//     toma2 = balanza.get_units(20);
//     delay(5);
//  }
//  
//  Serial.print("Peso actual medido es: ");
//  Serial.print(toma1); /*Imprime la medicion*/
//  Serial.println("g");
//  
//  return toma1; 
//}