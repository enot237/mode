#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define OLED_RESET 4
#include <EEPROM.h>
Adafruit_SSD1306 display(OLED_RESET);

#define NUMFLAKES 10
#define XPOS 0
#define YPOS 1
#define DELTAY 2
#define LOGO16_GLCD_HEIGHT 16
#define LOGO16_GLCD_WIDTH  16

#if (SSD1306_LCDHEIGHT != 32)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

float voltage_supply;                       //Напряжение на батарее в разные моменты
float voltage_coil;                         //Напряжение между спиралью и шунтом во время измерения
float current;                              //Изначально, ток для измерения сопротивления спирали, затем - максимально достижимый ток в системе
float res_coil;                             //Сопротивление спирали
float res_shunt = 14.8;                     //Сопротивление измерительного шунта
float eds;                                  //ЭДС батареи
float current_watts;                        //Текущая выбранная мощность
float max_watts = 0;                        //Максимальная мощность в системе
short menu_enter = 0;                       //Параметр для операций с меню
short i = 0;                                //Параметр для операций с меню
boolean reset_menu = false;                 //Параметр для операций с меню
short menu_set = 1;                         //Выбор страницы меню
boolean change_set = false;                 //Флаг редактирования параметров в меню
float max_current;                          //Выбор максимального допустимого тока через батарею
float break_out;                            //Время отсечки (в секундах)
float count_break = 0;                      //Счётчик для отсечки
boolean locker = false;                     //Флаг блокировки работы при удерживании после отсечки
boolean calibration_mode = false;           //Флаг калибровки, запрещающий работу (разрешаются только измерения, режим необходим для настройки сопротивления шунта)
short error = 0;                            //Код ошибки/защиты

void setup()   {
  Serial.begin(115200);
  pinMode(3, OUTPUT);
  pinMode(9, OUTPUT);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
  max_current = EEPROM.read (0);
  break_out = EEPROM.read (4);
  //res_shunt = EEPROM.read (8)/100;
}

void loop() {
  voltage_supply = analogRead(A2) * 0.01515;

  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.clearDisplay();
  display.setCursor(0, 12);
  display.print("R: ");
  display.println(res_coil);
  display.setCursor(0, 24);
  display.print("I: ");
  display.println(current);
  display.setCursor(50, 0);
  display.print("maxW: ");
  display.println(max_watts);
  display.setCursor(50, 12);
  display.print("curW: ");
  display.println(current_watts);
  eds = voltage_supply;
  display.setCursor(0, 0);
  display.print("E: ");
  display.println(eds);
  display.display();

  if (digitalRead(5) == 0) {                                                         //Работа с отсечкой и запуск силовой схемы
    locker = false;
  }
  if ((digitalRead(7) == 1) and (locker == false)) {
    fire();
  }

  if ((digitalRead(6) == 1) and (current_watts < 40) and (digitalRead(5) == 0)) {    //Установка текущей мощности
    current_watts = current_watts + 1;
  }
  if ((digitalRead(5) == 1) and (current_watts > 0) and (digitalRead(6) == 0)) {
    current_watts = current_watts - 1;
  }


  if (reset_menu = true) {                                                          //Условия для входа в меню
    i = i + 1;
    if (i == 5) {
      menu_enter = 0;
      i = 0;
      reset_menu = false;
    }
  }
  if ((digitalRead(6) == 1) and (digitalRead(5) == 1)) {
    delay(200);
    if  ((digitalRead(6) == 0) and (digitalRead(5) == 0)) {
      reset_menu = true;
      i = 0;
      menu_enter = menu_enter + 1;
    }
  }
  if (menu_enter == 2) {
    menu_enter = 0;
    change_set = false;
    reset_menu = false;
    i = 0;
    menu();
  }

  delay(100);
}                                                                            //end of loop void

void fire() {
  digitalWrite(3, 1);
  voltage_supply = analogRead(A2) * 0.01515;                                    //Измерение сопротивления спирали
  voltage_coil = analogRead(A0) * 0.01515;
  digitalWrite(3, 0);

  current = voltage_coil / res_shunt;
  res_coil = (voltage_supply - voltage_coil) / current;
  current = 0;

  digitalWrite(9, 1);
  voltage_supply = analogRead(A2) * 0.01515;
  digitalWrite(9, 0);

  current =  voltage_supply / res_coil;                                       //Расчёт максимального тока

  error = 0;
  if (eds < 3.7) {
    error = 3;
  }
  if (eds > 6) {
    error = 4;
  }
  if (current > max_current) {
    error = 1;
  }
  if (res_coil < 0.01) {
    error = 1;
  }
  if (res_coil > 4) {
    error = 2;
  }

  // float battary_voltage = eds - voltage_supply;                                                        //Внутреннее напряжение батареи
  //float battary_resistance = battary_voltage / current;                                                 //Внутреннее сопротивление батареи
  // max_watts = ((pow(eds, 2)) * res_coil) / (pow((res_coil + battary_resistance), 2));                  //Расчёт максимальной мощности (через батарею)
  max_watts = pow(current, 2) * res_coil;                                                              //Расчёт максимальной мощности (без батареи)


  if ((calibration_mode == false) and (error == 0)) {
    float milisec = millis();
    while ((digitalRead(7) == 1) and (millis() - milisec < break_out * 1000)) {

      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(32, 0);
      display.println("VAPING");
      display.setCursor(22, 18);
      display.print((millis() - milisec) / 1000);
      display.print("/");
      display.println(break_out);
      display.display();
      if (max_watts > current_watts) {
        analogWrite(9, current_watts * 255 / max_watts);
      }
      else
      {
        analogWrite(9, 255);
      }
      delay(20);
    }                                                                                                         //отсечка сработала или вы отпустили кнопку

    display.clearDisplay();
    display.setCursor(32, 0);
    display.println("VAPING");
    display.setCursor(32, 18);
    display.println("LOCKED");
    display.display();
    digitalWrite(9, 0);
    locker = true;

    while (locker == true) {
      if (digitalRead(7) == 0) {
        locker = false;
      }
    }
  }
  else {                                                                                                     //Если был установлен режим калибровки или возникла ошибка
    display.clearDisplay();
    display.setCursor(32, 0);                                                                                //Индикация V
    switch (error) {
      case 1:
        display.println("Very low");
        display.setCursor(32, 18);
        display.println("resistance");
        display.display();
        break;
      case 2:
        display.println("Very high");
        display.setCursor(32, 18);
        display.println("resistance");
        display.display();
        break;
      case 3:
        display.println("Very low");
        display.setCursor(32, 18);
        display.println("charge");
        display.display();
        break;
      case 4:
        display.println("Very high");
        display.setCursor(32, 18);
        display.println("charge");
        display.display();
        break;
    }
    delay(1000);
  }
}

void menu() {                                                                              //Меню
  while (menu_enter != 2) {                                                                //Условия выхода
    if (reset_menu = true) {
      i = i + 1;
      if (i == 5) {
        menu_enter = 0;
        i = 0;
        reset_menu = false;
      }
    }
    if ((digitalRead(6) == 1) and (digitalRead(5) == 1)) {
      delay(200);
      if  ((digitalRead(6) == 0) and (digitalRead(5) == 0)) {
        reset_menu = true;
        i = 0;
        menu_enter = menu_enter + 1;
      }
    }

    display.display();                                                                //Отображение страниц меню
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.println("Menu:");
    switch (menu_set) {
      case 1: display.setCursor(40, 0);
        display.setTextSize(1);
        display.println("max current");
        display.setCursor(40, 12);
        display.setTextSize(2);
        display.println(max_current);
        break;
      case 2: display.setCursor(40, 0);
        display.setTextSize(1);
        display.println("break out");
        display.setCursor(40, 12);
        display.setTextSize(2);
        display.println(break_out);
        break;
      case 3: display.setCursor(40, 0);
        display.setTextSize(1);
        display.println("Shunt res");
        display.setCursor(40, 12);
        display.setTextSize(2);
        display.println(res_shunt);
        break;
      case 4: display.setCursor(40, 0);
        display.setTextSize(1);
        display.println("Calibration");
        display.setCursor(40, 12);
        display.setTextSize(2);
        if (calibration_mode == true) {
          display.println("ON");
        } else {
          display.println("OFF");
        }
        break;
    }

    if ((change_set == true) and (menu_set != 0)) {                                            //Редактирование параметров
      display.setCursor(10, 20);
      display.setTextSize(1);
      display.println("X");

      switch (menu_set) {
        case 1: if ((digitalRead(6) == 1) and (max_current < 30)) {
            max_current = max_current + 1;
          }
          if ((digitalRead(5) == 1) and (max_current > 1)) {
            max_current = max_current - 1;
          }
          break;
        case 2: if ((digitalRead(6) == 1) and (break_out < 10)) {
            break_out = break_out + 0.2;
          }
          if ((digitalRead(5) == 1) and (break_out > 0.6)) {
            break_out = break_out - 0.2;
          }
          break;
        /*     case 3: if ((digitalRead(6) == 1) and (res_shunt < 30)) {
              res_shunt = res_shunt + 0.1;
            }
            if ((digitalRead(5) == 1) and (res_shunt > 1)) {
              res_shunt = res_shunt - 0.1;
            }
            break;  */
        case 4: if (digitalRead(6) == 1) {
            calibration_mode = !calibration_mode;
          }
          if (digitalRead(5) == 1) {
            calibration_mode = !calibration_mode;
          }
          break;
      }
    }

    if ((change_set == false) and (i == 0)) {                                               //Пролистывание страниц
      if ((digitalRead(6) == 1) and (menu_set < 4)) {
        menu_set = menu_set + 1;
      }
      if ((digitalRead(5) == 1) and (menu_set > 1)) {
        menu_set = menu_set - 1;
      }
    }


    if ((menu_enter == 1) and (i == 4)) {                                                  //Сохраниение некоторых результатов в энергонезависимую память
      if (change_set == 1) {
        switch (menu_set) {
          case 1: EEPROM.write(0, max_current);
            break;
          case 2: EEPROM.write(4, break_out);
            break;
            //  case 3: EEPROM.write(8, res_shunt*100);
            //   break;
        }
      }
      change_set = !change_set;
    }


    delay(100);
  }
  if (menu_enter == 2) {                                                                     //Условия выхода
    menu_enter = 0;
    reset_menu = false;
    i = 0;
  }
}



