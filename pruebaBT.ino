
String inputByte = "t";


void setup()
{
  Serial.begin(9600);
  pinMode(13,OUTPUT);
}


void loop() 
{
  while(Serial.available()>0)
  {
    inputByte = Serial.read();
    int n = inputByte.length();
    String variable = inputByte.substring(1,1);
    String val = inputByte.substring(2,n);
    float valor = val.toFloat();

    Serial.print(inputByte);
    
    if (inputByte == 'T')
    {
      digitalWrite(13,HIGH);
    }
    else if (inputByte=='t')
    {
      digitalWrite(13,LOW);
    } 
  }

  long masa = random(0,950);
  String masa_txt = String(masa);
  
  long liquido = random(0,950);
  String liq_txt = String(liquido);
 
  long temperatura = random(0,100);
  String temp_txt = String(temperatura);
 
  long motor = random(0,1);
  String motor_txt = String(motor);

  Serial.print("#t1");
  Serial.print(",");
  Serial.print(masa_txt);

  Serial.print("#t2");
  Serial.print(",");
  Serial.print(liq_txt);
  
  Serial.print("#t3");
  Serial.print(",");
  Serial.print(temp_txt);
  
  Serial.print("#t4");
  Serial.print(",");
  Serial.print(motor_txt);
}
