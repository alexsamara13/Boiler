
#define SENS6_PIN 6       // BUTTON +
#define SENS7_PIN 7       // BUTTON -
//#define SENS5_PIN 8      // DATCHIK PLAMENI
#define SENS1_PIN 9       // VACUUM RELE
#define SENS2_PIN 10      // DATCHIK DAVLENIA
//#define SENS3_PIN 11			// DATCHIK PEREGREVA
//#define SENS4_PIN 12      // DATCHIK PROTOKA
#define _LCD_TYPE 1  // для работы с I2C дисплеями

#include <LiquidCrystal_I2C.h>        //  Подключаем библиотеку для работы с LCD дисплеем по шине I2C
#include <math.h> // библиотека для выполнения простых математических операций
//#include <LCD_1602_RUS_ALL.h>
//#include <GyverNTC.h>
#include "GyverButton.h"
#include "GyverWDT.h"
#include "TimerOne.h"
#include <microDS18B20.h>
MicroDS18B20<17> sensor;  // датчик температуры на D17(A3)
GButton butt1(SENS1_PIN); // VACUUM RELE
GButton butt2(SENS2_PIN); // DATCHIK DAVLENIA
//GButton butt3(SENS3_PIN); // DATCHIK PEREGREVA
//GButton butt4(SENS4_PIN); // DATCHIK PROTOKA
//GButton butt5(SENS5_PIN); // DATCHIK PLAMENI
GButton butt6(SENS6_PIN); // BUTTON +
GButton butt7(SENS7_PIN); // BUTTON -
//GyverNTC therm(0, 9930, 3950);
LiquidCrystal_I2C lcd(0x27, 16, 2);
//LCD_1602_RUS lcd(0x27, 16, 2);
int value = 0 , SETTEMP = 39 , TEMPON = 40 , TEMPOFF = 69;
int val = 545 , valuegaz = 0, val1;
int valuepiezo = 0  , t , cursorhour, cursormin ;
int delerr = 700 , n = 0 , SEPAR = 32;
bool SENSVAC, SENSPRESS, SENSHEAT, SENSFLAME;
bool flagvent = 1, flaggaz = HIGH, flagpump = HIGH, flagvalve = HIGH ;
bool flagpiezo = 1, flagplamja = LOW, LEDflag = LOW, FLAGon = 0, flagsepar = 0;
bool BUTTinc , BUTTdec ;
uint32_t tmr, tmr1, tmr2 , tmr3, myTimer, myTimer1, myTimer2 , MINUT , HOUR;
float GRADHEAT, OVERHEAT;

void setup() {
  // Serial.begin(9600);
  pinMode(2, OUTPUT);  //RELE VENT
  pinMode(3, OUTPUT);  //RELE PUMP
  pinMode(4, OUTPUT);  //RELE GAZ
  pinMode(5, OUTPUT);  //OPTO PIEZO
  pinMode(13, OUTPUT);  // ERROR
  butt1.setDebounce(80);
  butt2.setDebounce(50);
  //butt3.setDebounce(300);
  //butt5.setDebounce(300);
  butt6.setDebounce(150);
  butt7.setDebounce(150);
  butt6.setTimeout(500);
  butt7.setTimeout(500);
  digitalWrite(2, HIGH);
  digitalWrite(3, HIGH);
  digitalWrite(4, HIGH);
  digitalWrite(5, HIGH);
  Timer1.initialize(100);           // установка таймера на каждые 100 микросекунд (== 10 мс)
  Timer1.attachInterrupt(timerIsr);
  Watchdog.enable(RESET_MODE, WDT_PRESCALER_1024);// запуск таймера
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 1);
}
void timerIsr() {   // прерывание таймера
  butt1.tick();     // отработка теперь находится здесь
  butt2.tick();
  // butt3.tick();
  // butt4.tick();
  //butt5.tick();
  butt6.tick();
  butt7.tick();
}

void loop() {
  Watchdog.reset();
  clockwise();
  readsens();
  readtemp_ds();
  //readtemp_ntc();
  lcdprint();


  // индикация работы
  if (millis() - myTimer >= 2000) {
    myTimer = millis(); // сбросить таймер
    digitalWrite(13, LEDflag); // вкл/выкл
    LEDflag = !LEDflag; // инвертировать флаг
  }
  // инициализация экрана каждые 5 мин
  if (millis() - myTimer1 >= 300000) {
    myTimer1 = millis(); // сбросить таймер
    lcd.init();

  }

  //  запуск с нуля
  if (SENSVAC == 1 && SENSPRESS == 1 && SENSHEAT == 1 && FLAGon == 0 )  {
    value = 0;  ventstart();
  }
  if (SENSVAC == 0 && SENSPRESS == 1 && SENSHEAT == 1 && GRADHEAT <= TEMPON  && FLAGon == 0 )  {
    FLAGon = 1;
    value = 0;  ventstart();
    Watchdog.reset();
    pumpstart();
    Watchdog.reset();
    gazstart();
    n++;
    Watchdog.reset();

  }
  SENSVAC = butt1.state();

  // контроль пламени
  if (SENSFLAME == 1) {
    flagpiezo = HIGH; // датчик пламени замкнут
    piezo();
    valuepiezo = 0;
  }
  if (SENSFLAME == 0 && flaggaz == LOW) {
    flagpiezo = LOW;   // датчик пламени разомкнут)
    piezo();
    valuepiezo++;
  }
  if (valuepiezo == 1000) {
    flaggaz = HIGH; gaz();
    flagpiezo = HIGH;  piezo();
    delerr = 500;
    error();

  }

  // отключение при работающей газ горелке и отсутствии вакуума
  if (SENSVAC == 0 && SENSPRESS == 1 && SENSHEAT == 1 && FLAGon == 1 )  {
    flaggaz = HIGH; gaz();
    flagpiezo = HIGH;  piezo();
    flagvent = HIGH; vent();
    value = 0;
    FLAGon = 0;
  }
  // отлючение при перегреве
  if (SENSVAC == 1 && SENSPRESS == 1 && SENSHEAT == 0 && FLAGon == 1 )  {
    offovertemp();

  }
  if (SENSVAC == 1 && SENSPRESS == 1 && GRADHEAT >= TEMPOFF && SENSHEAT == 1 && FLAGon == 1 )  {
    offovertemp();

  }
  // контроль давления теплоносителя
  SENSPRESS = butt2.state();
  // if (SENSPRESS == 1)  flagpump = LOW; pump();      // датчик давления замкнут
  if (SENSPRESS == 0)  {
    flagpump = HIGH; pump();  // датчик давления разомкнут)
    flaggaz = HIGH; gaz();
    flagpiezo = HIGH; piezo();
    delerr = 300;
    error();
  }

}

void TEMPERON() {

  if (butt6.isPress())  BUTTinc =  1;
  if (butt6.isRelease())  BUTTinc =  0;
  if (butt7.isPress())  BUTTdec = 1;
  if (butt7.isRelease())  BUTTdec = 0;
  if ( BUTTinc )  {
    TEMPON++;
    myTimer2 = millis();
  }
  if ( BUTTdec ) {
    TEMPON--;
    myTimer2 = millis();
  }
  if (millis() - myTimer2 >= 6000) {
    myTimer = millis();
    //  if ( BUTTinc == 0 && BUTTdec == 0 )
    lcd.setCursor(8, 1);
    lcd.print("    ");
    lcdprint();
    Watchdog.reset();
    return;
  }
  // TEMPON = SETTEMP;
  lcd.setCursor(8, 1);
  lcd.print("T on");
  lcdprint();
  Watchdog.reset();
  TEMPERON();
}

void TEMPEROFF() {
  if (butt6.isPress())  BUTTinc =  1;
  if (butt6.isRelease())  BUTTinc =  0;
  if (butt7.isPress())  BUTTdec = 1;
  if (butt7.isRelease())  BUTTdec = 0;
  if ( BUTTinc )  {
    TEMPOFF++;
    myTimer2 = millis();
  }
  if ( BUTTdec ) {
    TEMPOFF--;
    myTimer2 = millis();
  }
  if (millis() - myTimer2 >= 6000) {
    myTimer = millis();
    //if ( BUTTinc == 0 && BUTTdec == 0 )
    lcd.setCursor(5, 1);
    lcd.print("    ");
    lcdprint();
    Watchdog.reset();
    return;
  }
  //TEMPOFF = SETTEMP;
  lcd.setCursor(5, 1);
  lcd.print("Toff");
  lcdprint();
  Watchdog.reset();
  TEMPEROFF();
}
void offovertemp() {
  flaggaz = HIGH; gaz();
  flagpiezo = HIGH; piezo();
  Watchdog.reset();
  delay(3500);
  Watchdog.reset();
  flagvent = HIGH; vent();
  Watchdog.reset();
  delay(3500);
  Watchdog.reset();
  flagpump = HIGH; pump();
  Watchdog.reset();
  FLAGon = 0;
}
void ventstart() {

  if (value > 2) {
    flagvent = HIGH;  //value- количество попыток+1)
    vent();
    error();
  }

  if (!digitalRead(SENS1_PIN) == 1) {
    value = 3;
    flagvent = LOW;
    vent();
    delay(1000); // первоначальный опрос реле вакуума
    flagvent = HIGH;
    vent();
    Watchdog.reset();
    delay(2000);
    delerr = 100;
    ventstart();
  }

  flagvent = LOW; vent();           //LOW -уровень включения реле
  Watchdog.reset();
  // if (millis() - tmr2 >= 3000) {
  //  tmr2 = millis();
  //delay(3000);
  // for (int i = 0; i < 100; i++) {
  // readsens();
  // lcdprint();
  // Watchdog.reset();
  //}
  delay(3000);
  if (!digitalRead(SENS1_PIN) == 0) {
    flagvent = HIGH; //HIGH -уровень отключения реле
    vent();
    value++;
    delay(2000);
    delerr = 400;
    Watchdog.reset();
    ventstart();
    //}
  }
}

/*
  void tmrdelay (int t) {
  for (int i = 0; i < t ; i++) {
    readsens();
    lcdprint();
  }
  Watchdog.reset();
  }
*/

void pumpstart() {
  if (!digitalRead(SENS2_PIN) == 0) {
    delerr = 600;
    error();
  }
  flagpump = LOW; pump();
  delay(2000);

}

void gazstart() {

  if (valuegaz > 2) {
    flaggaz = HIGH;  //value- количество попыток+1)
    gaz();
    error();
  }
  flaggaz = LOW; gaz();
  flagpiezo = LOW;  piezo();


  delay(2000);
  readsens();
  lcdprint();
  if (SENSFLAME == 0) {
    flaggaz = HIGH; gaz();
    flagpiezo = HIGH;  piezo();
    delay(2000);
    Watchdog.reset();
    valuegaz++;
    delerr = 200; gazstart();
  }
}
void clockwise()
{
  if (millis() - tmr3 >= 60000) {
    MINUT++;
    tmr3 = millis();
  }
  if (MINUT == 60) {
    HOUR++;
    MINUT = 0;
  }
  if (HOUR == 96) HOUR = 0;
  if (HOUR < 10) {
    lcd.setCursor(0, 0);
    lcd.print("0");
    cursorhour = 1;
  }
  else cursorhour = 0;
  if (MINUT < 10) {
    lcd.setCursor(3, 0);
    lcd.print("0");
    cursormin = 4;
  }
  else cursormin = 3;
  lcdprint();

}

void lcdprint() {

  lcd.setCursor(cursorhour, 0);
  lcd.print(HOUR, DEC);

  lcd.setCursor(cursormin, 0);
  lcd.print(MINUT, DEC);
  lcd.setCursor(0, 1);
  // lcd.print( FLAGon, BIN );
  // lcd.setCursor(1, 1);
  lcd.print( SENSVAC, BIN );
  // lcd.setCursor(2, 1);
  lcd.print( SENSPRESS, BIN );
  // lcd.setCursor(3, 1);
  lcd.print( SENSHEAT, BIN );
  // lcd.setCursor(4, 1);
  lcd.print( SENSFLAME, BIN );

  if (millis() - tmr1 >= 1000) {

    lcd.setCursor(2, 0);
    lcd.print(char(SEPAR));
    tmr1 = millis();
if (SEPAR == 58)  SEPAR = 32;
else SEPAR = 58;
    
   // flagsepar = !flagsepar;
   // if (flagsepar == true)  SEPAR = 32;
   // if (flagsepar == false)  SEPAR = 58;
    lcd.setCursor(6, 0);
    lcd.print("        ");
    lcd.setCursor(6, 0);
    lcd.print( GRADHEAT , 1);
    lcd.print(" ");
    lcd.print( OVERHEAT , 0);
  }
  //lcd.print( analogRead(0) ,DEC);
  lcd.setCursor(5, 1);
  lcd.print( TEMPON , DEC);
  lcd.setCursor(10, 1);
  lcd.print( TEMPOFF , DEC);
  lcd.setCursor(13, 1);
  lcd.print( n , DEC);
}

void vent() {
  digitalWrite(2, flagvent);

}

void pump() {
  digitalWrite(3, flagpump);

}

void gaz() {
  digitalWrite(4, flaggaz);

}

void piezo() {
  if (flaggaz == HIGH )  flagpiezo = HIGH;

  digitalWrite(5, flagpiezo);
}
//Опрос датчиков..
void readsens() {
  val = (analogRead(0));   // получаем  данные c датчика перегрева NTC
  // OVERHEAT = 132.0702756 * exp(-0.0030085 * val); //расчет темп-ры по данным порта
  OVERHEAT = 139.672194 * exp(-0.0031137 * val);
  //readtemp_ntc();

  val1 = (analogRead(1));   // Читаем температуру  с датчика пламени аналог..
  // Serial.println( val1);

  if (butt1.isPress())  SENSVAC = 1;
  if (butt1.isRelease())  SENSVAC = 0;
  if (butt2.isPress())  SENSPRESS = 1;
  if (butt2.isRelease())  SENSPRESS = 0;
  if (OVERHEAT < 90)  SENSHEAT = 1;
  if (OVERHEAT >= 90)  SENSHEAT = 0;
  if (val1 <= 970)  SENSFLAME = 1;
  if (val1 > 970)  SENSFLAME = 0;
  if (butt6.isHolded()) {
    lcd.setCursor(8, 1);
    lcd.print("T");
    delay(1000);
    TEMPEROFF();
  }
  if (butt7.isHolded()) {
    lcd.setCursor(8, 1);
    lcd.print("T");
    delay(1000);
    TEMPERON();
  }
}

// Читаем температуру  с датчика DS18B20
void readtemp_ds() {

  if (millis() - tmr >= 800) {
    tmr = millis();
    // читаем прошлое значение
    if (sensor.readTemp()) {
      GRADHEAT = (sensor.getTemp());
      lcdprint();
      // запрашиваем новое измерение
      sensor.requestTemp();
    }
  }
}

// Читаем температуру  с датчика NTC
void readtemp_ntc() {
  val = (analogRead(0));   // получаем  данные c датчика перегрева NTC
  // OVERHEAT = 132.0702756 * exp(-0.0030085 * val); //расчет темп-ры по данным порта
  OVERHEAT = 139.672194 * exp(-0.0031137 * val);
}

void error() {

  for (;;) {
    // выполняется вечно...

    digitalWrite(13, HIGH);// ERROR PO RELE VENT 600 NO PRESSURE WATER
    delay(delerr / 3);
    lcd.setCursor(14, 0);
    lcd.print(" ");
    delay(300);
    digitalWrite(13, LOW);// ERROR PO RELE VENT 100 NO VACUUM PRESSURE
    delay(delerr);
    lcd.setCursor(14, 0);
    lcd.print("E");
    lcd.print((delerr / 100), DEC);
    delay(1000);
    clockwise();
    readsens();
    readtemp_ds();
    lcdprint();

    Watchdog.reset();

  }
}
