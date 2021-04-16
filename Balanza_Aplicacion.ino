#include <HX711.h>

// Variables para conexión de celda de carga
#define DOUT  A1
#define CLK  A0

// Pin de conexión PWM para motor de minibomba
int motor_pin = 9;

// Definición de constantes
HX711 balanza;

// Definición de variables
float toma1 = 0;
float liquido = 0;
float escala = 0; // variable para calcular la escala que se va a calibrar

char caracter; // la información recibida del usuario empieza con 'U', los setpoints empiezan con L, T, R, S 
String palabra; // variables para recibir información del Bluetooth

float densidad_liquido = 1; // MODIFICAR de acuerdo a la densidad del líquido 1g/ml
float volumen_liquido_deseado = 0; // MODIFICAR de acuerdo a lo que se quiera 

float masa_calibracion = 0; // MODIFICAR de acuerdo a la masa con la que está calibrando
float masa_muestra = 0; // MODIFICAR de acuerdo a lo que se pida de peso de muestra sólida que se va a tener en la balanza
float masa_actual = 0; // variable para guardar peso actual medido en el sistema

int vel_motobomba = 0; // variable para la velocidad de la moto bomba


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

  calibracion();

  Serial.print("Escala calibrada: ");
  Serial.println(escala);
  delay(1500);
  
  Serial.println("SISTEMA LISTO PARA OPERAR");
  delay(1000);
}


void loop()
{
  //Serial.println("Si desea dosificar líquido escriba 1 en el campo de texto");
  Serial.println("Ahora, ingrese los valores en la casilla correspondiente al setpoint requerido");
  masa_actual = medir_masa_actual(); // Esta función mide el peso con la escala obtenida directamente en este código

  if(Serial.available()) // Revisa si el usuario introdujo un comando en la interfaz para dosificar
  {
    float valor = Serial.parseInt();
//    palabra = Serial.read();
//    char ID = palabra[0];
//    String val = palabra.substring(1,palabra.length()-1);
//    int valor = val.toInt();

//    if (ID == 'T')
//    {
//      Serial.println("Quiere poner setpoint de temperatura");
//    }
    
//    else if (ID == 'L')
//    {
        masa_muestra = masa_actual;
        Serial.print("Para al dosificación, se tiene actualmente una muestra medida de: ");
        Serial.print(masa_muestra);
        Serial.println(" g");
        
        delay(2000);
        
//        Serial.println("¿cuánto volumen de líquido quiere dosificar (ml)?: ");
//        while (!(Serial.available() > 0))
//        {
//          // Espera hasta que ingresen un valor en la interfaz  
//        }
//        volumen_liquido_deseado = Serial.parseInt();
        
        volumen_liquido_deseado = (valor - 1000);
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
    
//    else if (valor == 2)
//    {
//      Serial.println("Inicio de tara"); // Esta tara la hace sobre el peso inicial de la balanza y el peso del recipiente, eso se puede cambiar
//      delay(1000);
//      balanza.set_scale(escala); 
//      balanza.tare(50); // Se toman cincuenta muestras, para calcular el proceso de tara (ajusta el cero de acuerdo al peso del recipiente)
//      Serial.println("Fin de tara");
//      delay(1000);
//    }
//  }
  
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

  while (error > 5) // Maneja una velocidad de la motobomba proporcional al error
  {
    vel_motobomba = map(abs(error), 0, volumen_liquido_deseado, 50, 255);
    analogWrite(motor_pin, vel_motobomba);
    
    masa_actual = medir_masa_actual(); // medir_peso_actual();
    error = referencia - masa_actual;

    liquido = masa_actual - masa_Inicial;
    
    Serial.print("#t2");
    Serial.print(",");
    Serial.println(liquido);
    
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
  Serial.print("El error inicial es: ");
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
//  else if (toma1 < 0) // su hay medidas negativas se ajusta la tara para corregirlas
//  {
//    Serial.println("Ajustando tara...");
//    balanza.tare(10);
//    delay(5);
//  }
  
//  Serial.print("Masa actual es: ");
//  Serial.print(toma1); // Imprime la medicion en consola
//  Serial.println("g");

  Serial.print("#t1");
  Serial.print(",");
  Serial.println(toma1);
  
  return toma1;
}


void calibracion(void) // Función para calibrar con 1 peso de referencia
{
  Serial.println("Por favor ingresar la masa con la que va a calibrar entre 1g y 950g");
  delay(500);
  
  while (!(Serial.available()))
  {
    // Espera hasta que ingresen una masa de calibracion  
  }
  
  masa_calibracion = Serial.parseInt();
  
  Serial.print("La masa de calibración ingresada es: ");
  Serial.print(masa_calibracion);
  Serial.println("g");
  
  Serial.println("Colocar sobre la balanza la masa con la que se desea calibrar");
  delay(6000);
  masa_actual = balanza.get_value(50); // Toma 50 medidas de la muestra
  Serial.print("El valor medido sin escala es: ");
  Serial.println(masa_actual);
  delay(2000);

  escala = masa_actual/masa_calibracion;      /*Asi se calcula la escala de la balanza para gramos, si quisieramos que la escala estuviera en kilogramos, se le ingresa el peso en kilogramos por la terminal*/  
}


