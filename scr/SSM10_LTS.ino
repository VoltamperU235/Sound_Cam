/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////7///
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* Sistema de Transmisión de audio utilizando el SIM800.
   Desarrollado en FlatBox.
   Se utilizo una Arduino Ultra_mini, siendo compatible el sketch con una Arduino Pro mini.
   Initial Version: August 2018
   Alpha Version: December 2018
   Developed and designed by Dennis Revolorio (Kestler Disciple)
*/

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "config.h"
#include "DHT.h"
#include <EEPROM.h>                                   //Utilizamos la memoria EEPROM interna para reiniciar el sistema y determinar cual era el mensaje de inicio. (Management o Sound_Recorded)
#include "LowPower.h"                                 //Fue nesesaria para Reudcir el consumo de energia mientras la arduino no se esta utilizando (que es aproximadamente un 95% del tiempo :v )
#include <SoftwareSerial.h>                           //Se utilizo SoftwareSerial para el Debuging (En el terminal) debido a que el SIM utiliza el UART0 .
#include <ArduinoJson.h>                              //Se utilizo para generar una String Json para transmision de los datos a Internet.
#define TINY_GSM_MODEM_SIM800                         //Como la libreria tinyGSM es generica hay que especificar que Chip se utiliza para la conexion a la red GSM.
#include <TinyGsmCommon.h>                            //Librerias nesesarias para el correcto funcionamiento del SIM800.
#include <TinyGsmClient.h>
SoftwareSerial mySerial(8, 9);                        //Declaramos los pines por los cuales se comunicara el Arduino con el PC (utilizar conversor USB - TTL)
TinyGsm modem(Serial);                                //Creamos un objeto en el cual indicaremos el UART que se comunicara con el SIM800 en este caso UART0 (Serial) si fuera otra arduino mas poderosa podria ser Serial1, Serial2, etc. :v

#define DHTPIN 15                                     //Definimos el pin digital al cual estara conectado el DO del DTH en este caso D15 (A1).
#define DHTTYPE DHT11                                 //Definimos el modelo de DHT que estamos utilizando.

DHT dht(DHTPIN, DHTTYPE);                             //Creamos un objeto especifcando el pin digital del DTH11 y el modelo (DTH11).

#define QUICK_SLEEP 100                               //Estado de la maquina de estados finitos en el cual se entra en modo sleep de manera mas rapida a nivel de spftware        
#define SLEEP_LONG 0                                  //Estado de la maquina de estados finitos en el cual se entra en modo sleep.       
#define LLAMADA 1                                     //Estado de la maquina de estados finitos en el cual se ejecutara la llamada.
#define TEXT 8                                        //Estado de la maquina de estados finitos en el cual se enviaran los mensajes de estado.
#define ALARMS 21                                     //Estado de la maquina de estados finitos en el cual se programaran las alarmas.
#define NOISE 24                                      //Estado de la maquina de estados finitos en el cual se ejecutara el analisis de pulsos.
#define READ_TEXT 55

String DETECCION_ALARMA = "";                         //String en la cual ingresaran caracteres provenientes del SIM800 en busca del texto que indica activacion de alarma.
String STRING_HORA_SIM800;                            //String que almacenara la String de hora actual proveniente del SIM800.
char C;                                               //Caracter que almacenara el dato recibido por el SIM800 y lo concatenara al ARRAY_DE_ANALISIS.

unsigned long MILLIS_NOW;                             //Entero que contendra el valor de millis() en ese momento.
bool LOCK_MILLIS = HIGH;                              //Booleano que nos bloqueara que la variable MILLIS_NOW este tomando valores nuevos cada vez, sino solamente una vez al inicio de la funcion.
bool INDICADOR_DE_ENVIO;                              //Booleano que nos indicara si nuestro comando tinyGMS fue realizado exitosamente
volatile int ESTADO = 0;                              //Entero que almacenara el estado de la maquina de estados finitos.
bool SENDER = LOW;                                    //Booleano para determinar dentro del estado TEXT el destinatario del mensaje.
uint16_t BATT_PERCENT;                                //Servira para guardar el valor del porcentaje de la bateria.
String BATTERY_TO_SMS;                                //String para guardar el BATT_PERCENT concatenado con %.
char JSON_SMS[115];                                   //Array de Caracteres donde se guardara la String JSON para enviar;
String JSON_STRING;                                   //String donde se concaternara nustro array de JSON y que este listo para enviar por GSM.
volatile int PULSES;                                  //Servira para llevar control del numero de interrupciones generadas en un momento dado de tiempo.
unsigned long MILLLIS_NOW;                            //Entero que almacenara el valor de millis() para calcular diferencias de tiempo.
bool ENABLE_ANALISIS = HIGH;                          //Bool que nos permitira el acceso a una rutina de analisis de pulsos para determinar si es un sonido fuerte o sos falsos positivos.
long PULSES_BEFORE;                                   //Enetero que almacenara la ultima cantidad de pulsos registrada para determinar la diferencia de pulsos entre lecturas.
unsigned long PULSES_DIFERENCE;                       //Entero en el cual se almacena la diferencia de pulsos entre lecturas.

//Para establecer una alarma en el SIM800 se utiliza el siguiente comando AT: AT+CALA=<Parametros> (verificarlos en el Datasheet) :v
//La idea es que Arduino cree la instruccion de manera automatica en relacion a la hora, por lo que se utilizaran porciones constantes de la instruccion y se le contatenara la informacion extra.

const String ALARM1_RESET = "AT+CALA=\"00:00:00\",1,0";
const String ALARM1_RESET_DELETE = "AT+CALD=1";
const String ALARM2_RESET = "AT+CALA=\"06:00:00\",2,0";
const String ALARM2_RESET_DELETE = "AT+CALD=2";
const String ALARM3_RESET = "AT+CALA=\"12:00:00\",3,0";
const String ALARM3_RESET_DELETE = "AT+CALD=3";
const String ALARM4_RESET = "AT+CALA=\"18:00:00\",4,0";
const String ALARM4_RESET_DELETE = "AT+CALD=4";

int HUMITY;                                           //Entero que almacenara la humedad registrada po el DTH11
int TEMP;                                             //Entero que almacenara la temperatura ambiente registrada por el DTH11

int SMS_ESTATE = 0;                                   //Entero que determinara el tipo de mensaje que se enviara Management o Sound_Recorded
const String CSCLK2 = "AT+CSCLK=2";                   //Comando utilizado por el SIM800 para entrar en modo SLEEP automatico.
const String CSCLK0 = "AT+CSCLK=0";                   //Comando utilizado por el SIM800 para salir del modo SLEEP automatico

String IMEI = "";                                     //String que almacenara el IMEI del SIM800
bool ENABLE_SCAN;                                     //Booleano que servira como bandera para indicar el momento en que debe escanear el UART en busca de alarmas
char ALARM_FLAG[4] = "0123";                          //Array de caracteres que servira para introducir caracter por caracter (maximo 4) en busca de la combinacion "CALV" (Señal de Alarma)
String UART_SCAN = "";                                //String donde se volcara el ALARM_FLAG para analizar sin contiene "CALV" (se realizo de esta manera para utilizar de manera eficiente la memoria ram del microcontrolador).




void setup() {
  Serial.begin(9600);                                  //Establecemos la conexion con el SIM800 utilizando el UART0 (Lo utilizamos porque existe la Interupcion Serial nesesaria para sacar a la arduino del modo sleep en el momento que el SIM800 reciba la indicacion de alarma);
  mySerial.begin(115200);                              //Lo utilizamos para el debuging y visualizar los datos en el PC (es mas rapida para que no retrase mucho la programacion).
  //pinMode(4, OUTPUT);
  //digitalWrite(4, LOW);
  dht.begin();                                         //Comando para inicializar el sensor DTH11
  pinMode(MIC_INTERRUPT, INPUT);                       //Inicializamos el pin DO del modulo microfono como entrada digital
  pinMode(MIC, OUTPUT);                                //Inicializamos el pin elevador de tension para el microfono ambiental como salida digital
  digitalWrite(MIC, LOW);                              //Colocamos el pin del elevador de tension para el microfono ambiental en estado bajo (APAGADO)
  pinMode(NO_CONECTION_LED, OUTPUT);                   //El LED permanecera encendido mientras carga el setup y detecta conexion;
  digitalWrite(NO_CONECTION_LED, HIGH);
  mySerial.println(F("10LTS"));                        //Mostramos la version de firmware (por motivos de desarrollo)
  mySerial.println(F("1"));                            //Mensajes para determinar correcto funcionamiento en el PC
  sendData("AT", 111, 1);                              //Enviamos en comando "AT" para verificar el correcto funcionamiento del SIM800 (o para salir del SLEEP automatico si estaba activado)
  sendData(CSCLK0, 1111, 1);                           //Desactivamos el SLEEP automatico y continuamos con el programa.
  sendData("ATH", 1111, 1);                            //Por motivos de seguridad (Pasa cuando hay poca carga en la bateria) se corta la llamada en proceso (si existiera).

  pinMode(ACTIVE_LED, OUTPUT);
  pinMode(LOW_POWER_LED, OUTPUT);                      //El LED rojo permanecera encendido mientras no se conecte a la red gsm.

  while (!modem.waitForNetwork()) {                    //Esperamos en un bucle infinito la conexion de red.
    mySerial.println(F("2"));                          //El sistema no continuara si no hay conexion de red.
  }
  if (modem.isNetworkConnected()) {
    mySerial.println(F("3"));                          //Mostrara un mensaje cuando de haya conectado a la red.
  }
  while (IMEI == "") {
    IMEI = modem.getIMEI();                            //Esperamos en un bucle hasta obtener el IMEI del dispositivo (Es importante porque asi se puede distiguir de los otros dispositivos)
  }
  sendData("AT+CLTS=1", 555, 1);                       //Comando utilizado para activar la hora local de red.
  delay(111);                                          //Esperamos a que obtenga la hora.
  sendData("AT&W", 555, 1);                            //Comando utilizado para guardar los cambios realizados.
  digitalWrite(NO_CONECTION_LED, LOW);
  digitalWrite(ACTIVE_LED, HIGH);                      //Cuando se conecte a la red se pondra el LED en verde.
  //SET ALARM TO RESET
  sendData(ALARM1_RESET_DELETE, 550, 1);               //Enviamos el comando que borra cualquier alarma en la posicion 2 (Yo urilize esa arbitrariamente :v ).
  sendData(ALARM2_RESET_DELETE, 550, 1);               //Enviamos el comando que borra cualquier alarma en la posicion 2 (Yo urilize esa arbitrariamente :v ).
  sendData(ALARM3_RESET_DELETE, 550, 1);               //Enviamos el comando que borra cualquier alarma en la posicion 2 (Yo urilize esa arbitrariamente :v ).
  sendData(ALARM4_RESET_DELETE, 550, 1);               //Enviamos el comando que borra cualquier alarma en la posicion 2 (Yo urilize esa arbitrariamente :v ).
  mySerial.println(F("SETTING ALARM"));
  delay(555);
  sendData(ALARM1_RESET, 550, 1);                      //Enviamos en comando para situar la alarma en la posicion uno.
  sendData(ALARM2_RESET, 550, 1);                      //Enviamos en comando para situar la alarma en la posicion dos.
  sendData(ALARM3_RESET, 550, 1);                      //Enviamos en comando para situar la alarma en la posicion tres.
  sendData(ALARM4_RESET, 550, 1);                      //Enviamos en comando para situar la alarma en la posicion cuatro.

  //Verificamos el valor que hay en la posicion 0 de la memoria EEPROM interna.

  if (EEPROM.read(0) == 0)                             //Si el valor es 0 significa que el ultimo reinicio fue debido a una alarma de reinicio periodico (Cada 6 horas)
  {

    SMS_ESTATE = 0;                                    //Entonces el envio en un Management. (colocando SMS_ESTATE = 0)
    mySerial.println("ESTATE: 0");
  }
  else                                                 //Si el valor es distinto de cero significa que el reinicio fue debido a una grabacion provocada por algun sonido fuerte
  {
    SMS_ESTATE = 1;                                    //Entonces el envio es un Sound_Recorded. (Colocando SMS_ESTATE = 1)
    EEPROM.write(0, 0);                                //Escribimos en la posicion 0 de la EEPROM <0> para volver al estado normal (Management).
    mySerial.println("ESTATE: 1");
    delay(15);                                         //Esperamos a que termine de escribir en la memoria EEPROM.
  }

  ESTADO = TEXT;                                       //Colocamos la maquina de estados finitos en TEXT para el respectivo envio del mensaje Management o Sound_Recorded.
}

void loop() {
  switch (ESTADO) {

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////// BIENVENIDOS AL ESTADO "SLEEPS MODES" ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    case QUICK_SLEEP:
      PULSES_DIFERENCE = 0; PULSES_BEFORE = 0; PULSES = 0;                                                              //Borramos nuestra variables de analisis porque se sobrepaso el tiempo maximo de muestreo estando PULSES_FIFERENCE en 0 (Ver Case: Noise para comprender mejor.)
      attachInterrupt(0, DETECTION, FALLING);                                                                           //Activamos la interrupcion INT0.
      LowPower.idle(SLEEP_FOREVER, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_OFF, USART0_ON, TWI_OFF);           //Se coloca la Arduino en modo LowPower IDLE (Ver Datasheet.) podemos desactivar todo lo que no estemos utilizando, apagar los clock internos y permanecer atentos al UART0 (Se hace porque se programar alarmas periodicas al SIM y las envia por UART )
      break;
    case SLEEP_LONG:
      SMS_ESTATE = 0;
      digitalWrite(ACTIVE_LED, LOW);                                                                                    //Colocamos en LOW el LED de ACTIVE (Busy).
      digitalWrite(LOW_POWER_LED, HIGH);                                                                                //Colocamos en HIGH el LED de Power Save
      attachInterrupt(0, DETECTION, FALLING);                                                                           //Activamos la interrupcion INT0.
      mySerial.println("Sleeping");
      mySerial.flush();                                                                                                 //Vaciamos la cola de bytes de nuestro serial virtual antes de mandar a dormir el SIM800
      ENABLE_SCAN = HIGH;                                                                                               //Habilitamos la lectura del UART en busca de alarmas de reinicio periodico.
      ESTADO = QUICK_SLEEP;                                                                                             //Colocamos la maquina de estados finitos en modo QUICK_SLEEP para que entre mas rapido en modo SLEEP
      digitalWrite(MIC, LOW);                                                                                           //Desactivamos el elevador de tension colocando en LOW en pin ENABLE
      sendData(CSCLK2, 3333, 1);                                                                                        //Enviamos el comando para colocar el SIM800 en modo SLEEP automatico
      LowPower.idle(SLEEP_FOREVER, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_OFF, USART0_ON, TWI_OFF);           //Se coloca la Arduino en modo LowPower IDLE (Ver Datasheet.) podemos desactivar todo lo que no estemos utilizando, apagar los clock internos y permanecer atentos al UART0 (Se hace porque se programar alarmas periodicas al SIM y las envia por UART )
      //LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_ON);
      break;

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////// BIENVENIDOS AL ESTADO "LLAMADA" ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    case LLAMADA:
      sendData("AT", 111, 1);                                         //Enviamos en comando "AT" para verificar el correcto funcionamiento del SIM800 (o para salir del SLEEP automatico si estaba activado)
      sendData(CSCLK0, 3333, 1);                                      //Desactivamos el SLEEP automatico y continuamos con el programa.
      detachInterrupt(0);                                             //Desactivamos la interrupcion (Para evitar falsos positivos generados por los picos de energia que consumen el SIM800.
      while (!modem.waitForNetwork())
      {
        mySerial.println(F("2"));
      }
      digitalWrite(MIC, HIGH);                                        //Activamos el elevador de tension colocando en HIGH en pin ENABLE
      digitalWrite(LOW_POWER_LED, LOW);
      digitalWrite(ACTIVE_LED, HIGH);                                 //Colocamos en HIGH el ACTIVE_LED y desactivamos los demas.
      digitalWrite(NO_CONECTION_LED, LOW);

      ESTADO += 1;                                                    //Pasamos al siguiente estado de la llamda. :v
      break;
    case 2:
      Smart_Delay(111);                                               //Esperamos 111ms para pasar al siguiente estado (uso numeros asi porque el numero 0 de mi teclado no funciona y tengo que copiarlo y pegarlo cada vez que lo utilizo XD ).
      break;
    case 3:
      //INDICADOR_DE_ENVIO = modem.callNumber("0017867084206");       //Este comando esta disponible en la libreria tinyGSM y sirve para ejecutar una llamada,
      INDICADOR_DE_ENVIO = modem.callNumber("50257276772");           //Este comando esta disponible en la libreria tinyGSM y sirve para ejecutar una llamada,

      mySerial.println(INDICADOR_DE_ENVIO);                           //Mostramos el resultado de la llamada (Normalmente si se tarda es porque ocurrió algún error :'v ).
      ESTADO += 1;                                                    //Pasamos al siguiente estado de la llamda. :v
      break;
    case 4:
      Smart_Delay(48000);                                             //Esperamos 48s. para pasar al siguiente estado
      break;
    case 5:
      sendData("ATH", 1111, 1);                                       //Comando utilizado para corta la llamada actual.
      digitalWrite(MIC, LOW);                                         //Desactivamos el elevador de tension colocando en LOW en pin ENABLE
      mySerial.println("OFF");                                        //Mostramos el resultado de la llamada (Normalmente si se tarda es porque ocurrió algún error :'v ).
      mySerial.flush();                                               //Vaciamos el buffer de nuestro UART virtual.
      ESTADO += 1;                                                    //Pasamos al siguiente estado de la llamda. :v
      break;
    case 6:
      Smart_Delay(5555);                                              //Esperamos 5s. para pasar al siguiente estado (Se hace porque al finalizar la llamada todavia esta enviando cosas y generando falsos positivos)
      break;
    case 7:
      EEPROM.write(0, 1);                                             //Colocamos la posicion 0 de la memoria EEPROM en 1 para que al momento de reiniciar el mensaje inicial sea de Sound_Recorded y no de Management
      mySerial.println("ESTATE: 1");
      delay(255);                                                     //Esperamos para que escriba en la EEPROM
      software_Reset();                                               //Reiniciamos el sistema.
      break;

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////// BIENVENIDOS AL ESTADO "TEXT" ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    case TEXT:
      while (!modem.waitForNetwork())
      {
        mySerial.println(F("2"));                                  //Esperamos en un bucle infinito la conexion de red.
      }
      digitalWrite(LOW_POWER_LED, LOW);
      digitalWrite(ACTIVE_LED, HIGH);                              //Activa el LED busy mientras envia un mensaje informativo al celular en formato JSON y coloca la alarmas para los mensajes periodicos.
      detachInterrupt(0);                                          //Desactivamos la interrupcion (Para evitar falsos positivos generados por los picos de energia que consumen el SIM800.
      mySerial.println(F("SMS"));
      if (SMS_ESTATE == 0 && !SENDER) {                            //Verificamos que el SMS_ESTATE sea 0 (Management) y que sender sea 0 (evitar que le pida datos al SIM800 y al DTH11 dos veces tan rapido)
        DTH();                                                     //Recolectamos los datos del DTH11.
        mySerial.println(HUMITY);
        mySerial.println(TEMP);
        BATT_PERCENT = modem.getBattPercent();                     //Solicitamos al SIM800 el valor del porcentaje de la bateria.
        BATTERY_TO_SMS = String(BATT_PERCENT) += '%';              //Lo concatenamos con % (solo para que se mire bonito :v)
        STRING_HORA_SIM800  = modem.getGSMDateTime(DATE_TIME);     //Solicitamos la hora de la red.
        JSON();                                                    //Creamos nuestro JSON y lo dejamos listo para enviar.
      }
      mySerial.println(F("P-E"));
      ESTADO += 1;                                                 //Pasamos al siguiente estado de los mensajes. :v
      break;
    case 9:
      Smart_Delay(111);                                            //Esperamos para darle tiempo al SIM800 para procesar los comandos correctamente
      break;
    case 10:
      mySerial.println(F("E"));
      //Serial.println(F("AT+CMGF=1"));                            //Los siguientes comandos hasta el case 18, son utilizados para enviar una mensaje de texto, los Smart_Delay son algo tediosos porque generan mas codigo pero hace mas aficiente la programacion para el compilador.
      sendData("AT+CMGF=1", 1111, 1);                              //Este comando es utilizado para inicializar la transmision SMS.
      ESTADO += 1;                                                 //Pasamos al siguiente estado de los mensajes. :v
      break;
    case 11:
      Smart_Delay(111);                                            //Esperamos para darle tiempo al SIM800 para procesar los comandos correctamente
      break;
    case 12:
      mySerial.print(F("Estado Sender: "));
      mySerial.println(SENDER);
      if (!SENDER)                                                 //si SENDER == false entonces que envie el mensaje al numero de Twilio
      {
        sendData("AT+CMGS=\"0017867084206\"", 1111, 1);            //Este comando es utilizado para determinar el destinatario del mensaje
      }
      if (SENDER)                                                  //Si SENDER == true entonces que envie el mensaje al numero de telefono auxiliar
      {
        Serial.println("AT+CMGS=\"" + TELEPHONE + "\"");           //Este comando es utilizado para determinar el destinatario del mensaje
        ESTADO += 1;                                               //Pasamos al siguiente estado de los mensajes. :v
      }
      break;
    case 13:
      Smart_Delay(111);                                            //Esperamos para darle tiempo al SIM800 para procesar los comandos correctamente
      break;
    case 14:
      mySerial.println(SMS_ESTATE);
      if (SMS_ESTATE == 0) {                                       //Verificamos si el SMS_ESTATE==0
        //Serial.print(JSON_STRING);
        sendData(JSON_STRING, 1111, 1);                            //De ser asi, enviamos nuestro JSON (Management)
      }
      if (SMS_ESTATE == 1) {
        Serial.print("{\"I_D\":\"" + IMEI + "\"}");                //Si no, enviamos un JSON (Creado a mano por motivos de memoria en el microcontrolador) indicando Sound_Recorded
      }
      ESTADO += 1;                                                 //Pasamos al siguiente estado de los mensajes. :v
      break;
    case 15:
      Smart_Delay(555);                                            //Esperamos para darle tiempo al SIM800 para procesar los comandos correctamente
      break;
    case 16:
      Serial.print(char(26));                                      //Enviamos un equivalente a "CTRL+Z", sirve para indicarle al SIM800 el final del mensaje
      ESTADO += 1;                                                 //Pasamos al siguiente estado de los mensajes. :v
      break;
    case 17:
      Smart_Delay(111);                                            //Esperamos para darle tiempo al SIM800 para procesar los comandos correctamente
      break;
    case 18:
      Serial.print("");
      Serial.flush();                                              //Vaciamos la cola de bytes de nuestro UART.
      mySerial.print(F(""));
      mySerial.print(F("E2"));                                     //imprimimos algunas cosas por el serial solo para verificar el correcto funcionamiento
      mySerial.print(F(""));
      ESTADO += 1;                                                 //Pasamos al siguiente estado de los mensajes. :v
      break;
    case 19:
      Smart_Delay(10000);                                          //Esperamos esta cantidad de tiempo para evitar los falsos positivos generados por los picos de corriente que exije el  SIM800.
      break;
    case 20:
      switch (SENDER) {                                            //Verificamos el estado actual de SENDER.
        case 0:                                                    //En caso de ser 0, que repita el estado TEXT y cambiamos el destinatario
          ESTADO = TEXT;                                           //Colocamos la maquina de estados finitos en modo TEXT
          SENDER = HIGH;                                           //Cambiamos el remitente del mensaje
          break;
        case 1:
          mySerial.print(F("To Sleep"));
          ESTADO = 0;                                              //Colocamos la maquina de estados finitos en modo SLEEP
          SMS_ESTATE = 0;                                          //Colocamos nuestra variable como estaba al inicio.
          SENDER = LOW;                                            //Colocamos nuestra variable como estaba al inicio.
          break;
      }
      break;

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////// BIENVENIDOS AL ESTADO "NOISE" ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    case NOISE:
      digitalWrite(ACTIVE_LED, HIGH);
      delay(11);                                                                              //Emitimos un parpadeo cada vez que escuche un sonido.
      digitalWrite(ACTIVE_LED, LOW);
      if (ENABLE_ANALISIS) {                                                                  //Verificamos si es nesesario realizar un nuevo calculo o si ya hay uno en proceso.
        MILLIS_NOW = millis();                                                                //Verificamos el estado actual de millis() para empezar a calcular la tasa de cambio de los pulsos.
        ENABLE_ANALISIS = LOW;
      }
      if (millis() > (MILLIS_NOW + 333)) {                                                    //El tiempo de muestreo es aproximadamente 333ms despues de haber detectado un pulso
        PULSES_DIFERENCE = PULSES - PULSES_BEFORE;                                            //Se calcula la difencia de pulsos entre nuestra tasa de tiempo
        mySerial.println(PULSES_DIFERENCE);
        PULSES_BEFORE = PULSES;                                                               //Se guarda nuestra ultima lectura de pulsos
        ENABLE_ANALISIS = HIGH;                                                               //Se habilita la posibilidad de nuevo analisis.

        if (PULSES_DIFERENCE == 0) {
          ESTADO = QUICK_SLEEP;                                                               //Si el resultado del analisis fue cero es porque ya no se esta produciendo ningun sonido constate ni fuerte.
        }
        if (PULSES_DIFERENCE > 70) {                                                          //Si la tasa de cambio fue demasiado alta entonces cambiara al estado llamada
          ESTADO = LLAMADA;                                                                   //Se puede modificar el 70 para tasas de cambio mas fuertes (Sonidos bastante fuertes);
          PULSES_DIFERENCE = 0;
          PULSES_BEFORE = 0;
        }

      }
      break;

  }

  if (ENABLE_SCAN) {
    if ((ESTADO == 0 || ESTADO == QUICK_SLEEP))
    {
      SIM_SCAN();
    }
  }

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////// Funciones del programa ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//*********************************************************************************************************************************************************************************************************************************************************************************************************************************************************************
//*****************************************************  Smart_Delay  ****************************************************************************************************************************************************************************************************************************************************************************************************************
//  Funcion parecida a delay() pero no para la ejecucion del sketch, siempre esta atento a datos provenientes del UART0. (Funciona unicamente con maquinas de estados finitos :'v ).
//

void Smart_Delay(long duration) {                                             
  if (LOCK_MILLIS) {                                                          //Verificamos si es momento de tomar un punto de referencia para medir el trancurso del tiempo
    MILLIS_NOW = millis();                                                    //Si es momento, entonces verificamos el momento actual
    LOCK_MILLIS = LOW;                                                        //y bloquemos la toma de un nuevo punto de referencia
    mySerial.println(F(""));
  }
  if ( millis() >= (MILLIS_NOW + duration)) {                                 //Se calcula la diferencia de tiempos para saber si ya transcurrio el tiempo nesesario y pasar al siguiente estado.
    LOCK_MILLIS = HIGH;                                                       //permitimos la toma de un nuevo punto de referencia
    ESTADO += 1;                                                              //pasamos al siguiente estado de la maquina de estados finitos.
  }
}

//*********************************************************************************************************************************************************************************************************************************************************************************************************************************************************************
//*****************************************************  DETECTION  ****************************************************************************************************************************************************************************************************************************************************************************************************************
// Funcion que permite realizar una toma de muestras de sonido y analizarlas  ************************************************************************************************************************************************************************************************************************************************************************************************
//

void DETECTION() {                                                           
  if (ESTADO == SLEEP_LONG || ESTADO == NOISE || ESTADO == QUICK_SLEEP) {     //Solo si la maquina de estados finitos se encuentra en alguno de estos estados
    ESTADO = NOISE;                                                           //Vamos al Estado Noise para analizar los pulsos (No se hace directamente aca porque al ser una interrupcion eso retrasaria cualquier proceso que se este realizando (Como recibir datos por el UART)).
    PULSES++;
  }
}

//*********************************************************************************************************************************************************************************************************************************************************************************************************************************************************************
//******************************************************  sendData  *************************************************************************************************************************************************************************************************************************************************************************************************************
//  Funcion que permite enviar comandos AT y esperar su respuesta por determinado tiempo (si Debug esta en HIGH)  *************************************************************************************************************************************************************************************************************************************************************************************************
//

String sendData(String command, const int timeout, boolean debug) {     
  String response = "";                                                 //Variable que contendra las respuestas del SIM800
  Serial.println(command);                                              //Mostramos en la PC el comando que enviamos.
  long int time = millis();                                             //Tomamos el valor acutal de millis();
  while ( (time + timeout) > millis()) {                                //Miestras no se sobrepase el timeout esperamos datos y los mostramos en el PC
    while (Serial.available()) {
      char c = Serial.read();                                           //almacenamos en una variable c cada caracter que ingrese por el UART
      response += c;                                                    //Cada caracter se suma a nuestra variable
    }
  }
  if (debug) {                                                          //Si la Respuesta esta activada entonces que imprima la lectara del SIM800
    mySerial.print(response);
  }
  return response;                                                      //Retorna la String
}

//*********************************************************************************************************************************************************************************************************************************************************************************************************************************************************************
//*******************************************************  JSON  ************************************************************************************************************************************************************************************************************************************************************************************************************
//  Funcion que permite Generar un buffer para la String Json que contrendra los datos.  ********************************************************************************************************************************************************************************************************************************************************************************************************************************
//

void JSON() {
  StaticJsonBuffer<115> jsonBuffer;                           
  JsonObject& root = jsonBuffer.createObject();               //Se crea un Objeto del tipo JsomObjet para ingresar los datos.
                                                              //Añadimos los valores de los sensores a nuestro JSON.
  root["I"] = IMEI;                                           //Ingresamos el imei del SIM800
  root["D"] = STRING_HORA_SIM800;                             //Ingresamos la hora del SIM800
  root["T"] = TEMP;                                           //Ingresamos la temperatura registrada del DTH11
  root["H"] = HUMITY;                                         //Ingresamos la humedad registrada del DTH11
  root["B"] = BATTERY_TO_SMS;                                 //Ingresamos el porcentaje de bateria restante
  root.printTo(JSON_SMS, sizeof(JSON_SMS));                   //Ingresamos en un array de Char nuestro JSON con los datos ya ingresados.
  JSON_STRING = String(JSON_SMS);                             //Convertimos a String nuestro JSON.
}

//*********************************************************************************************************************************************************************************************************************************************************************************************************************************************************************
//******************************************************  SIM_SCAN*  *****************************************************************************************************************************************************************************************************************************************************************************************************
//  Verificamos si hay nuevos datos disponibles en el UART0.  ***************************************************************************************************************************************************************************************************************************************************************************************************************************************
//

void SIM_SCAN() {
  while (Serial.available()) {                          
    C = Serial.read();                                  //Lo almacenamos en nuestra variable C.

    ALARM_FLAG[3] = C;                                  //Generamos un array de 4 caracteres que almacenara y correra los datos que ingresen

    UART_SCAN  = String(ALARM_FLAG);                    //Volacmos en una String nuestro array de caracteres.
    int A = UART_SCAN.indexOf("CALV");                  //Verificamos si la String contiene la palabra "CALV" reservada por el SIM800 para las alarmas programas

    if (A != -1)                                        //La variable A sera un -1 en caso de no contener la palabra
    {
      alarm_reset();                                    //En caso de no ser -1 es porque si contenia la palabra y procede a reiniciar el sistema
    }
    for (int x = 0; x < 3; x++) {
      ALARM_FLAG[x] =  ALARM_FLAG[x + 1];
    }
  }
}

//*********************************************************************************************************************************************************************************************************************************************************************************************************************************************************************
//********************************************************** DTH *****************************************************************************************************************************************************************************************************************************************************************************************************
//  Funcion que permite realizar la toma de mestras atmosfericas provenientes del DTH11  *********************************************************************************************************************************************************************************************************************************************************************************************************************************************************************
//

void DTH()
{
  float h = dht.readHumidity();                       //Obtenemos la humedad del DTH11
  float t = dht.readTemperature();                    //Obtenemos la humedad del DTH11
  float f = dht.readTemperature(true);                //Obtenemos la temperatura del DTH11 en farenheit
  if (isnan(h) || isnan(t) || isnan(f))               //Verificamos que los datos sean reales y no NULL
  {
    return;
  }
  float hic = dht.computeHeatIndex(t, h, false);      //Calculamos el indice de calor
  HUMITY = int(h);                                    //Asignamos a nuestra variable global HUMITY el valor leido 
  TEMP = int(t);                                      //Asignamos a nuestra variable global TEMP el valor leido
}

//*********************************************************************************************************************************************************************************************************************************************************************************************************************************************************************
//****************************************************  software_Reset  ***********************************************************************************************************************************************************************************************************************************************************************************************
// Restarts program from beginning but does not reset the peripherals and registers  *********************************************************************************************************************************************************************************************************************************************************************************************************************************************************************
//
  
void software_Reset() {
  asm volatile ("  jmp 0");                          //Funcion ensablador que sirve para "reiniciar" el microcontrolador
}

//*********************************************************************************************************************************************************************************************************************************************************************************************************************************************************************
//*****************************************************  alarm_reset  *************************************************************************************************************************************************************************************************************************************************************************************************
//  Funcion que sirve para determinar la String que activo el Software Reset y vacia la cola de bytes  *********************************************************************************************************************************************************************************************************************************************************************************************************************************************************************
//

void alarm_reset()
{
  mySerial.println(UART_SCAN);                       //Imprimimos la String que activo el software_Reset
  mySerial.flush();                                  //vaciamos la cola de bytes.
  software_Reset() ;                                 //Reiniciamos el dispositivo.
}
