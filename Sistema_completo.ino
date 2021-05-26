#include <Arduino.h>
// libreria para lectura de la celda de carga
#include <HX711.h>
// libreria para manejo de motor paso a paso
#include <DRV8825.h>
// libreria para el sensor de temperatura
#include <DS18B20.h>


/* DATOS ENVIADOS A LA APP
 * #t1,VALOR = masa enviada a aplicacion
 * #t2,VALOR = cantidad de líquido
 * #t3,VALOR = temperatura
 * #t4,VALOR = RPMs del motor paso a paso
 */

/* SETPOINTS RECIBIDOS DE LA APP
 * L = setpoint de liquido
 * T = setpoint de temperatura
 * R = setpoint de RPMs
 * S = setpoint de tiempo de mezclado
 */


// Variables para conexión de celda de carga
#define DOUT  A1
#define CLK  A0

// Motor steps per revolution. Most steppers are 200 steps or 1.8 degrees/step
#define MOTOR_STEPS 200
// 1=full step, 2=half step etc.
#define MICROSTEPS 1
// configure the pins connected
#define DIR 8
#define STEP 9
#define MS1 10
#define MS2 11
#define MS3 12
#define ENABLE 7
// define el objeto del motor Stepper
#DRV8825 stepper(MOTOR_STEPS, DIR, STEP, ENABLE, MS1, MS2, MS3);
DRV8825 stepper(MOTOR_STEPS, DIR, STEP)
// Pines de conexión de los perifericos
#define motor_pin 3   // pin pwm de la motobomba de líquido
#define relay_pin 8   // pin de conexión del relé que controla el calentador         
#define DS18B20_pin 6 // pin de conexion del sensor de temperatura    
#define stopbutton_pin 2 // pin para interrupción por hardware


// Definición de constantes de periféricos
HX711 balanza;
DS18B20 ds(DS18B20_pin);
uint8_t selected;


// timers para el control de temperatura
unsigned long Timer0 = 0;                                 // time counter 1 for delay measurements in millisecs
unsigned long TimerRelay = 0;                             // time counter for delay activation of Relay in millisecs
int TimeRelay = 2;                                        // number N of second for relay activation


//// Variables para calibración y medición de la balanza
float toma1 = 0;
float escala = 0; // variable para calcular la escala que se va a calibrar
float densidad_liquido = 1; // MODIFICAR de acuerdo a la densidad del líquido 1g/ml
float volumen_liquido_deseado = 0; // MODIFICAR de acuerdo a lo que se quiera 
float masa_calibracion = 0; // MODIFICAR de acuerdo a la masa con la que está calibrando
float masa_muestra = 0; // MODIFICAR de acuerdo a lo que se pida de peso de muestra sólida que se va a tener en la balanza
float masa_actual = 0; // variable para guardar peso actual medido en el sistema

//// Variables para motobomba y liquido
int vel_motobomba = 0; // variable para la velocidad de la moto bomba
float liquido = 0;

//// Variables para el sensor de temperatura
float temperature = 0;                                 // Variable for storing temperature measurements
float SetUpTemp = 0;                                   // Variable for storing the Setup Temperature of the system
int waitRelay = 0;                                     // Variable for storing status of waiting for the relay operation
float temperatura_actual;

//// Variables para motor paso a pas
int RPM = 0; // RPMs por defecto para el motor stepper
int tiempo_mezcla; // tiempo de mezcla ingresado
unsigned long tiempo; // tiempo medido para la mezcla

//// Variables para lectura de instrucciones desde la APP
char caracter; // la información recibida de la aplicación 
char ID; // id para setpoints enviados desde el celular
String val; // string para guardar el valor del setpoint enviado
int valor; // valor del setpoint ingresado por  
String palabra; // variables para recibir información del Bluetooth


void setup() 
{ 
  // Inicializacion 
  Serial.begin(9600);

  // inicializa la balanza
  balanza.begin(DOUT, CLK);

  // inicializa el sensor de temperatura
  selected = ds.selectNext();

  // Configuración de interrupción por hardware
  pinMode(stopbutton_pin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(stopbutton_pin), parado_emergencia, LOW);

  // inicializa el motor stepper con parametros iniciales
  stepper.begin(RPM, MICROSTEPS); 

  if (selected) 
  {
    Serial.println("Device DS18B20 was found!");
  } 
  else 
  {
    Serial.println("Device DS18B20 not found!");
  }

  // inicializa la motobomba y el relé
  pinMode(motor_pin, OUTPUT);
  digitalWrite(motor_pin, LOW);
  pinMode(relay_pin,OUTPUT);
  digitalWrite(relay_pin,HIGH);    // Turn off Relay in Setup
  
  Serial.println("Inicializando balanza");
  delay(1000);

  Serial.println("Por favor no poner nada sobre la balanza");
  delay(2000);
  
  Serial.println("Por favor poner recipiente sobre la balanza");
  delay(5000); 
  
  Serial.println("Inicio de tara"); // Esta tara la hace sobre el peso inicial de la balanza y el peso del recipiente, eso se puede cambiar
  delay(1000);
  balanza.set_scale(); 
  balanza.tare(50); // proceso de tara (ajusta el cero de acuerdo al peso del recipiente)
  Serial.println("Fin de tara");
  delay(1000);

  Serial.println("Inicializando la calibración de la balanza");
  delay(1000);

  calibracion();

  Serial.print("Escala calibrada: ");
  Serial.println(escala);
  delay(1000);
  
  Serial.println("SISTEMA LISTO PARA OPERAR");
  delay(2000);
}


void loop()
{
  Serial.println("Ahora, ingrese los setpoints requeridos");
  masa_actual = medir_masa_actual(); // mide la masa actual de la balanza
  temperatura_actual = medir_temperatura_actual(); // mide la temperatura actual del liquido
  control_onoff_temperatura(temperatura_actual); // realiza el control de temperatura con el relé
  
  ID = 'X'; // resetea el valor del ID para que no se crucen los setpoints
  val = ""; // resetea valor string recibido de setpoints

  while(Serial.available()) // Lee los mensajes enviados por la aplicación
  {
    caracter = Serial.read();
    
    if(caracter == '*') 
    {    
      ID = palabra[0];
      val = palabra.substring(1,palabra.length());
      valor = val.toInt();
      palabra = "";
    }
    else
    {
      palabra = palabra + caracter;
    }
  }

  if (ID == 'Q') // cuando se recibe parado de emergencia desde la aplicación
  {
    parado_emergencia();
  }

  else if (ID == 'L') // si se envía setpoint de líquido
  {
    masa_muestra = masa_actual;
    Serial.print("Para la dosificación, se tiene actualmente una muestra medida de: ");
    Serial.print(masa_muestra);
    Serial.println(" g");
        
    delay(2000);
    
    volumen_liquido_deseado = valor;
    Serial.print("Volumen de liquido deseado: ");
    Serial.print(volumen_liquido_deseado);
    Serial.println(" ml");
    
    delay(2000);
    
    Serial.println("Dosificando...");
    
    control_p_motobomba(masa_actual);
    
    Serial.println("Dosificación terminada");
  }

  else if (ID == 'T') // si se envía setpoint de temperatura
  {
    SetUpTemp = valor;
    Serial.println("Se estableció setpoint de temperatura en: ");
    Serial.println(valor);
  }

  else if (ID == 'R') // si se envía setpoint de RPMs
  {
    stepper.setRPM(valor);
    RPM = valor;
    Serial.print("Se fijó el setpoint de RPMs en: ");
    Serial.println(valor);
    delay(50);
  }

  else if (ID == 'S') // si se envía setpoint de tiempo de mezclado
  {
    Serial.print("Se fijó tiempo de mezcla en: ");
    Serial.println(valor);
    
    tiempo_mezcla = valor;
    tiempo = millis();
    
    stepper.enable();
    Serial.print("#t4");
    Serial.print(",");
    Serial.println("ON");
    
    while ((millis() - tiempo)*1000 < tiempo_mezcla)
    {
      stepper.move(5);
    }
    
    stepper.disable();
    Serial.print("#t4");
    Serial.print(",");
    Serial.println("OFF");
  }

  delay(20);
  
}


void parado_emergencia()
{
  // para apagar motobomba, stepper y calentador relay
  Serial.println("PARADO DE EMERGENCIA ACTIVADO");
  analogWrite(motor_pin, 0); // apaga motoboma
  stepper.disable(); // apaga el stepper
  digitalWrite(relay_pin, HIGH); // desactiva el calentador
  delay(60000); // espera para desconectar o reiniciar el sistema
}


void control_onoff_temperatura(float temp_actual)
{
   if((temp_actual < SetUpTemp) && (waitRelay == 0) && SetUpTemp != 0)  // If the temp is less than the setup activate relay and story counting time
   {
      digitalWrite(relay_pin, LOW);               // Activate Relay
      // Activate timer
      Timer0 = millis(); 
      waitRelay = 1;
   }   
   
   TimerRelay = millis();
   if ((TimerRelay >= Timer0+(1000*TimeRelay)) && (waitRelay==1))  // if the time has concluded for the relay to be active
   {
      digitalWrite(relay_pin,HIGH);               // Deactivate Relay
      waitRelay = 0;
   }        
}


float medir_temperatura_actual()
{
  if (selected) // DS18B20 is responding and active
  {
      temperature = ds.getTempC();
      Serial.print("#t3");
      Serial.print(",");
      Serial.println(temperature);
  } 
  else // The DS18B20 is not responding
  {
    Serial.println("Error con sensor de temperatura");
    delay(100);                         // wait a bit to see it working, remove after testing
    digitalWrite(relay_pin, HIGH);        // Turn off temperature as sensor is not responding
    temperature = 0;                      // Determine temperature as Zero to notify an error
    waitRelay = 0;                        // inactivate the status for the relay to wait
  }

  return temperature;
}


float medir_masa_actual() // Función para medir masa con una sola escala calibrada en este mismo código
{
  balanza.set_scale(escala); // Establece la escala calibrada
  toma1 = balanza.get_units(5);  // Pesa y entrega el valor en gramos

  if (toma1 > 1000) // revisa que se esté pesando máximo 1000g = 1kg
  {
    Serial.println("Fuera de rango de medición de masa");
  }
//  else if (toma1 < 0) // su hay medidas negativas se ajusta la tara para corregirlas
//  {
//    Serial.println("Ajustando tara...");
//    balanza.tare(10);
//    delay(5);
//  }

  Serial.print("#t1");
  Serial.print(",");
  Serial.println(toma1); // manda la masa actual
  
  return toma1;
}


void control_p_motobomba(float masa_Inicial) // Para control proporcional
{
  float referencia = (volumen_liquido_deseado*densidad_liquido) + masa_muestra;
  float error = referencia - masa_Inicial; // Error = referencia - actual
  Serial.print("El error actual del setpoint de líquido es: ");
  Serial.print(error);
  Serial.println(" g");

  while (error > 5) // Maneja una velocidad de la motobomba proporcional al error
  {
    vel_motobomba = map(abs(error), 0, volumen_liquido_deseado, 50, 255);
    analogWrite(motor_pin, vel_motobomba);
    
    masa_actual = medir_masa_actual();
    error = referencia - masa_actual;

    liquido = masa_actual - masa_Inicial;
    
    Serial.print("#t2");
    Serial.print(",");
    Serial.println(liquido);
    
    Serial.print("El error actual del setpoint de líquido es: ");
    Serial.print(error);
    Serial.println(" g");
  }
  
  analogWrite(motor_pin, 0);
}


void calibracion(void) // Función para calibrar con 1 peso de referencia
{
  Serial.println("Por favor ingresar la masa con la que va a calibrar entre 1g y 900g");
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


