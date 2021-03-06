#include <errno.h>
#include <Servo.h> 

#define ENGINE_SPEED      6
#define ENGINE_DIR        7
#define VOLTAGE           5
 
#define COMMAND_EXPIRE 10000

#define DEBUG false

Servo steering;

class CommandParser {
  private:
    static const int BUFLEN = 30;
    static const int COMMAND_COUNT = 8;
    static const int KEY_COUNT = 6;
    char* COMMANDS[COMMAND_COUNT] = {"r", "e", "s", "bm", "set", "info", "disable", "version"};
    int CMD_SIZES[COMMAND_COUNT] = {1, 1, 1, 2, 3, 4, 7, 7};

    char* KEYS[KEY_COUNT] = {"steering_angle", "timeout", "angle", "min_speed", "top_front_speed", "top_rear_speed"};
    int KEY_SIZES[KEY_COUNT] = {14, 7, 5, 9, 15, 14};


    char buffer[BUFLEN];
    int position;
    int argpos;
    
    int getArgEnd() {
      for(int i = argpos; i< BUFLEN; i++) {
        if (buffer[i] == ' ' || buffer[i] == '\n') {
          return i;
        }
      }
      return -1;
    }
  public:

    void clearBuffer() {
      position = 0;
      buffer[position] = '\0';
    }
  
    static const int COMMAND_NOT_READY = -2;
    static const int COMMAND_UNKNOWN = -1;
    
    static const int CMD_STEERING = 0;
    static const int CMD_ENGINE = 1;
    static const int CMD_STOP = 2;
    static const int CMD_BINARY_MODE = 3;
    static const int CMD_SET = 4;
    static const int CMD_INFO = 5;
    static const int CMD_DISABLE = 6;
    static const int CMD_VERSION = 7;
    
    CommandParser() {
      position = 0;
    };
    
    int processSerial() {
      int result = COMMAND_NOT_READY;
      if (DEBUG) {
        delay(1000);
        Serial.print("processSerial, ");
        Serial.println(result);
      }
      while(Serial.available()) {
        char input = (char)Serial.read();
        if (DEBUG) {
          Serial.print("pos=");
          Serial.print(position);
          Serial.print("input=");
          Serial.println(input);
        }
        buffer[position++] = input;
        if (input == '\n') {
          result = parseCommand(buffer);
          if (result >= 0) {
            argpos = CMD_SIZES[result] + 1;
            clearBuffer();
          } else {
            argpos = 0;
          }
          return result;
        }
        if (this->position >= BUFLEN) {
          // невалидная команда
          clearBuffer();
        }
      }
      return result;
    }
    
    int getInt() {
      int argend = getArgEnd();
      if (argend < 0){
        errno = 1;
        return -1;
      }
      buffer[argend] = '\0';
      int result = atoi(&(buffer[argpos]));
      argpos = argend + 1;
      return result;
    }
    
    char getChar() {
      int argend = getArgEnd();
      if (argend < 0){
        errno = 1;
        return -1;
      }
      char result = (char)buffer[argpos];
      argpos = argend + 1;
      return result;
    }
    
    int getKey() {
      int argend = getArgEnd();
      if (argend < 0){
        errno = 1;
        return -1;
      }
      buffer[argend] = '\0';
      for(int i=0; i< KEY_COUNT; i++) {
        if (strncmp(buffer, KEYS[i], KEY_SIZES[i]) == 0)
          return i;
      }
      return COMMAND_UNKNOWN;
      
    }
    
    int parseCommand(char* buf) {
      // ищем в начале строки известные команды.
      // возвращаем номер команды от 0 до 7.
      if (DEBUG) {
        Serial.print("parseCommand: ");
        Serial.println(buf);
      }
      for(int i=0; i< COMMAND_COUNT; i++) {
        if (strncmp(buf, COMMANDS[i], CMD_SIZES[i]) == 0)
          return i;
      }
      clearBuffer();
      return COMMAND_UNKNOWN;
    }
    
    void writeInfo(int voltage, int rotation, boolean front, int engine_speed, boolean disabled) {
      Serial.print(voltage);
      Serial.print(' ');
      Serial.print(rotation);
      Serial.print(front?" F ":" R ");
      Serial.print(engine_speed);
      Serial.println(disabled?" D":" E");
    }
    
    void writeRotation(int angle) {
      Serial.println(angle);
    }
    
    void writeEngineState(boolean front, int motor) {
      Serial.print(front?"F ":"R ");
      Serial.println(motor);
    }
    
    void writeDisabledState(boolean disabled) {
      Serial.println(disabled?"D":"E");
    }
    
    void writeVersion(int version) {
      Serial.println(version);
    }
};

CommandParser text_parser;

class RobotController {
  // параметры движения
  int rotation;
  int voltage;
  int engine_speed;
  int version;
  boolean front;
  boolean disabled;
  
  unsigned int last_command;
  boolean command_applied;
  
  // настройки
  int timeout = 10000;
  int angle = -2;
  int steering_scale = 100;
  int min_speed = 10;
  int top_front_speed = 200;
  int top_rear_speed = 100;
  
    
  CommandParser *parser;

  /**
  * Осуществляет синхронизацию с мотором и сервой
  */
  void sync() {
    command_applied = true;
    if (disabled)
      return;
    steering.write(rotation);
    digitalWrite(ENGINE_DIR, front);
    analogWrite(ENGINE_SPEED, engine_speed);
  };
  void switchBinaryMode() {};
  void stopCar() {
    engine_speed = 0;
    front = true;
    rotation = 90 + angle;
    sync();
  };
  void disableCar() {
    disabled = !disabled;
    parser->writeDisabledState(disabled);  
  };
  void processSteeringCommand(){
    if (DEBUG) Serial.println("processSteeringCommand");
    int value = parser->getInt();
    // проверяем что удалось разобрать число.
    if (errno!=0)
      return; 
    // поворот - основной угол 90 + коррекционный угол + ограничитель в % * value / 5
    // при ограничителе в 100% получим диапазон угла поворота в 255/5 градусов.
    rotation = 90 + angle + steering_scale * value / 500;
    parser->writeRotation(rotation);
    sync();
  };
  void processEngineCommand(){
    if (DEBUG) Serial.println("processEngineCommand");
    int value = parser->getInt();
    if (DEBUG) Serial.print(value);
    if (errno!=0) {
      if (DEBUG) {
        Serial.print("getInt errno=");
        Serial.println(errno);
      }
      return; 
    }
    
    // направление    
    front = value>=0;
    
    // скорость
    
    // диапазон от -127 до 127 переводим в 0..255 + направление
    engine_speed = abs(value);
    
    if (engine_speed > 0) {
      // масштабируем входное значение относительно максимальной скорости
      if (front)
        engine_speed = min(min_speed + ((unsigned long)(top_front_speed - min_speed) * engine_speed) / 128, 127);
      else
        engine_speed = min(min_speed + ((unsigned long)(top_rear_speed - min_speed) * engine_speed) / 128, 127);
    }
    parser->writeEngineState(front, engine_speed);
    sync();
    
  };
  void processSetCommand(){
    int key = parser->getKey();
    if (key == CommandParser::COMMAND_UNKNOWN) 
      return;
    int value = parser->getInt();
    switch(key) {
      default:
        break;
    }
  };
  void showInfo(){
    parser->writeInfo(voltage, rotation, front, engine_speed, disabled);
  };
  void showVersion(){
    parser->writeVersion(version);
  };
  
  public:
    RobotController() {
      parser = &text_parser;
      disabled = true;
      engine_speed = 0;
      front = true;
      rotation = 90 + angle;
      version = 0;
      last_command = 0;
    }
    
    void processCommand(int cmd) {
          
      switch(cmd) {
        case CommandParser::CMD_STEERING:
          processSteeringCommand();
          break;
        case CommandParser::CMD_ENGINE:
          processEngineCommand();
          break;
        case CommandParser::CMD_STOP:
          stopCar();
          break;
        case CommandParser::CMD_BINARY_MODE:
          switchBinaryMode();
          break;
        case CommandParser::CMD_SET:
          processSetCommand();
          break;
        case CommandParser::CMD_INFO:
          showInfo();
          break;
        case CommandParser::CMD_DISABLE:
          disableCar();
          break;
        case CommandParser::CMD_VERSION:
          showVersion();
          break;
        default:
          break;
      }
      command_applied = false;
    }
    
  void loop() {
    // если долго не было команд - останавливаемся
    if (millis() - last_command > timeout) {
      if (command_applied) {
        stopCar();
        command_applied = false;
      }
      return; 
    }
    float v = map(analogRead(VOLTAGE), 0, 1023, 0, 300);
    voltage = (int)v;
  }
  
  int processSerial() {
    return parser->processSerial();
  }
};

RobotController controller;
  

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println("HELLO");
  // Настраивает выводы платы 6, 7 на вывод сигналов 
  for(char i = 6; i < 8; i++)     
      pinMode(i, OUTPUT);  
  // Рулевое управление
  steering.attach(9);
}

void loop() {
  controller.loop();  
}

void serialEvent() {
  // что-то есть на чтение в COM-порте
  int result = CommandParser::COMMAND_UNKNOWN;
  int cmd = result;
  // пока есть данные в порте, читаем и разбираем команды
  while (result != CommandParser::COMMAND_NOT_READY) {
    result = controller.processSerial();
    if (DEBUG) {
      Serial.print("result=");
      Serial.println(result);
    }
    // если нашли в строке команду - запоминаем ее и разбираем следующую строку,
    // пока не кончатся данные в порте.
    if (result != CommandParser::COMMAND_NOT_READY && result != CommandParser::COMMAND_UNKNOWN)
      cmd = result;
  }
  // если ничего не распознали, больше ничего не делаем.
  if (cmd == CommandParser::COMMAND_UNKNOWN)
    return;
  controller.processCommand(cmd);
}
