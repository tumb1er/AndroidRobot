#include <errno.h>

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
      buffer[BUFLEN] = '\0';
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
      while(Serial.available()) {
        char input = (char)Serial.read();
        buffer[position++] = input;
        if (input == '\n') {
          result = parseCommand(buffer);
          if (result >= 0) {
            argpos = CMD_SIZES[result] + 1;
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
    
    int parseCommand(char* buffer) {
      // ищем в начале строки известные команды.
      // возвращаем номер команды от 0 до 7.
      for(int i=0; i< COMMAND_COUNT; i++) {
        if (strncmp(buffer, COMMANDS[i], CMD_SIZES[i]) == 0)
          return i;
      }
      return COMMAND_UNKNOWN;
    }
};


CommandParser parser;

void switchBinaryMode() {};
void stopCar() {};
void disableCar() {};
void processSteeringCommand(){
  byte value = parser.getInt();
  // проверяем что удалось разобрать число.
  if (errno!=0)
    return; 
  // поворот
    
};
void processEngineCommand(){};
void processSetCommand(){
  int key = parser.getKey();
  if (key == CommandParser::COMMAND_UNKNOWN) 
    return;
  int value = parser.getInt();
  switch(key) {
    default:
      break;
  }
};
void showInfo(){};
void showVersion(){};

void setup() {
  // put your setup code here, to run once:
  
}

void loop() {
  // put your main code here, to run repeatedly:

}

void serialEvent() {
  // что-то есть на чтение в COM-порте
  int result = CommandParser::COMMAND_UNKNOWN;
  int cmd = result;
  // пока есть данные в порте, читаем и разбираем команды
  while (result != CommandParser::COMMAND_NOT_READY) {
    result = parser.processSerial();
    // если нашли в строке команду - запоминаем ее и разбираем следующую строку,
    // пока не кончатся данные в порте.
    if (result != CommandParser::COMMAND_NOT_READY && result != CommandParser::COMMAND_UNKNOWN)
      cmd = result;
  }
  // если ничего не распознали, больше ничего не делаем.
  if (cmd == CommandParser::COMMAND_UNKNOWN)
    return;
    
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
}
