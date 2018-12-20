/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////                                                           HISTORY.H                                                                    ////
/////      Sound_Cam V.1.0 ALPHA                                                                                                             ////
/////      Historial de cambios desde la version 1.0 ALPHA.                                                                                  ////
/////      FLATBOX, Guatemala 11 de Diciembre de 2018.                                                                                       ////
/////                                                                                                                                        ////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*

  Guatemala 19 de diciembre de 2018
  Clase Bug:
  El SIM800 despues de estar funcionando mucho tiempo se empaza a comportar de una manera extraña, y ya no realizaba llamadas, por lo que se procedio
  a agregarle una funcion que lo reinicia junto con el microcontrolador para garantizar el correcto funcionamiento.

  Funcion agregada:

  void SIM_RESET()
  {
  digitalWrite(SIM_RST, LOW);                         //Reiniciamos colocando el pin RST del SIM800 en 0
  delay(555);                                         //esperamos para un buen reinicio XD
  digitalWrite(SIM_RST, HIGH);                        //Volvemos a activar el modulo.
  }

  code modified:
  from:
  mySerial.println(F("10LTS"));                        //Mostramos la version de firmware (por motivos de desarrollo)
  mySerial.println(F("1"));                            //Mensajes para determinar correcto funcionamiento en el PC
  delay(555);                                          //Esperamos para que se vuelva a activar.
  sendData("AT", 111, 1);                              //Enviamos en comando "AT" para verificar el correcto funcionamiento del SIM800 (o para salir del SLEEP automatico si estaba activado)
  sendData(CSCLK0, 1111, 1);                           //Desactivamos el SLEEP automatico y continuamos con el programa.
  sendData("ATH", 1111, 1);                            //Por motivos de seguridad (Pasa cuando hay poca carga en la bateria) se corta la llamada en proceso (si existiera).

  -----------------------------------------------------------------
  to:
  mySerial.println(F("10LTS"));                        //Mostramos la version de firmware (por motivos de desarrollo)
  mySerial.println(F("1"));                            //Mensajes para determinar correcto funcionamiento en el PC
  SIM_RESET();                                     //Reiniciamos el SIM800L
  delay(555);                                          //Esperamos para que se vuelva a activar.
  sendData("AT", 111, 1);                              //Enviamos en comando "AT" para verificar el correcto funcionamiento del SIM800 (o para salir del SLEEP automatico si estaba activado)
  sendData(CSCLK0, 1111, 1);                           //Desactivamos el SLEEP automatico y continuamos con el programa.
  sendData("ATH", 1111, 1);                            //Por motivos de seguridad (Pasa cuando hay poca carga en la bateria) se corta la llamada en proceso (si existiera).
