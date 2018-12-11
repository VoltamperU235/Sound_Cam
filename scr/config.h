/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////                                                           CONFIG.H                                                                     ////
/////      Sound_Cam V.1.0 ALPHA                                                                                                             ////
/////      Dispositivo IoT que sirve para monitorear lugares remotos con acceso a red GSM, a traves de audio analogo.                        ////
/////                                                                                                                                        ////
/////      Materiales:                                                                                                                       ////
/////      SIM800l                                                                                                                           ////
/////      Arduino Ultra Mini                                                                                                                ////
/////      Microfono Ambiental                                                                                                               ////
/////      Elevador de Tension con pin ENABLE                                                                                                ////
/////      Modulo Microfono con Digital Output                                                                                               ////                                   ////
/////      Bateria 18650                                                                                                                     ////
/////      Modulo de carga de baterias de lition                                                                                             ////
/////      Panel Solar                                                                                                                       ////
/////                                                                                                                                        ////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define MIC   12                                      //Definimos el pin que estara conectado al ENABLE de elevador de tension para el microfono ambiental.
#define NO_CONECTION_LED 7                            //Definimos el pin que nos mostrara en una LED la ausencia de conexion.
#define ACTIVE_LED 6                                  //Definimos el pin que nos mostrara en una LED que el sistema esta busy.
#define LOW_POWER_LED 5                               //Definimos el pin que nos mostrara en una LED que el sistema entro en modo power-save.
#define MIC_INTERRUPT 2                               //Definimos el pin que estara conectado a la salida digital del microfono.

String TELEPHONE = "50257276772";                     //Definimos el numero de telefono auxiliar al cual se estaran enviando los datos
