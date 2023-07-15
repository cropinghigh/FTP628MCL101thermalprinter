//BASED ON OTHER DEVELOPERS CODE(https://kkmspb.ru/development/arduino/Fujitsu-FTP-628MCL101.php, https://www.jameco.com/Jameco/workshop/TechTip/temperature-measurement-ntc-thermistors.html)

#include <EEPROM.h>

#define PIN_MOTOR_DIR 2 //PD2
#define PIN_MOTOR_STEP 3 //PD3
#define PIN_MOTOR_SLEEP 4 //PD4
#define PIN_MOTOR_RESET 5 //PD5

#define PIN_PRN_DATA_IN 7 //PD7
#define PIN_PRN_CLK 8 //PB0
#define PIN_PRN_LATCH 9 //PB1

#define PORT_PRN_DATA_IN PORTD
#define NUM_PRN_DATA_IN 7
#define PORT_PRN_CLK PORTB
#define NUM_PRN_CLK 0
#define PORT_PRN_LATCH PORTB
#define NUM_PRN_LATCH 1

#define PIN_PRN_STROBE1 10 //PB2
#define PIN_PRN_STROBE2 11 //PB3
#define PIN_PRN_STROBE3 12 //PB4
#define PIN_PRN_STROBE4 A0 //PC0
#define PIN_PRN_STROBE5 A1 //PC1
#define PIN_PRN_STROBE6 A2 //PC2

//AVref=3.3v
#define PIN_THERMISTOR A3 //B=3950, R25=30k
#define THERMISTOR_RGND 20000.0f
#define MAX_TEMP 50.0f
#define PIN_HEAD_POWER_SENSE A4 //div = 1/2(vmax=2.5v)
#define HEAD_POWER_SENSE_MIN 7.8f //minimum VCC to work

//delay 0.1875us, fr~=5.3MHz
#define PRN_LOAD_DELAY asm("nop\n\tnop\n\tnop\n\t");

bool motor_step_val = false;

uint16_t PrnBurnDelayMin = 0;
uint16_t PrnBurnDelayMax = 0;

bool printVals[384];
uint8_t vals[384];

float getTempThermistor() {
  float readValue = analogRead(PIN_THERMISTOR);
  float thermistorR = THERMISTOR_RGND * ((1024.0f / readValue) - 1.0f);
  float tempC = (1.0f/(1.0f/(25.0f+273.0f) - 1.0f/3950.0f * log(30000.0f / thermistorR))) - 273.0f;
  return tempC;
}

float getVCC() {
  float readValue = analogRead(PIN_HEAD_POWER_SENSE);
  float voltage = (readValue / 1024.0) * 3.3f;
  return voltage * 11.1f;
}

void prnClear() {
  digitalWrite(PIN_PRN_STROBE1, LOW);
  digitalWrite(PIN_PRN_STROBE2, LOW);
  digitalWrite(PIN_PRN_STROBE3, LOW);
  digitalWrite(PIN_PRN_STROBE4, LOW);
  digitalWrite(PIN_PRN_STROBE5, LOW);
  digitalWrite(PIN_PRN_STROBE6, LOW);
  digitalWrite(PIN_PRN_CLK, LOW);
  digitalWrite(PIN_PRN_DATA_IN, LOW);
  digitalWrite(PIN_PRN_LATCH, HIGH);
  for(int i = 0; i < 384; i++) {
    digitalWrite(PIN_PRN_CLK, HIGH);
    digitalWrite(PIN_PRN_CLK, LOW);
  }
  digitalWrite(PIN_PRN_LATCH, LOW);
  digitalWrite(PIN_PRN_CLK, LOW);
  digitalWrite(PIN_PRN_DATA_IN, LOW);
  digitalWrite(PIN_PRN_LATCH, HIGH);
  delayMicroseconds(10);
}

void prnLoadBuffer() {
  PORT_PRN_CLK &= ~(1 << NUM_PRN_CLK);
  PORT_PRN_DATA_IN &= ~(1 << NUM_PRN_DATA_IN); 
  PORT_PRN_LATCH |= (1 << NUM_PRN_LATCH);
  PRN_LOAD_DELAY
  for(int i = 0; i < 384; i++) {
    if(printVals[i]) {
      PORT_PRN_DATA_IN |= (1 << NUM_PRN_DATA_IN); //hi
    } else {
      PORT_PRN_DATA_IN &= ~(1 << NUM_PRN_DATA_IN); //low
    }
    PRN_LOAD_DELAY
    PORT_PRN_CLK |= (1 << NUM_PRN_CLK);
    PRN_LOAD_DELAY
    PORT_PRN_CLK &= ~(1 << NUM_PRN_CLK);
    PRN_LOAD_DELAY
  }
  PORT_PRN_LATCH &= ~(1 << NUM_PRN_LATCH);
  PRN_LOAD_DELAY
  PORT_PRN_CLK &= ~(1 << NUM_PRN_CLK);
  PORT_PRN_DATA_IN &= ~(1 << NUM_PRN_DATA_IN);
  PORT_PRN_LATCH |= (1 << NUM_PRN_LATCH);
}

void waitForVCCNormalize() {
  while(getVCC() < HEAD_POWER_SENSE_MIN) {}
}

void prnStrobe(int segment, int Time) {
  digitalWrite(PIN_PRN_STROBE1, LOW);
  digitalWrite(PIN_PRN_STROBE2, LOW);
  digitalWrite(PIN_PRN_STROBE3, LOW);
  digitalWrite(PIN_PRN_STROBE4, LOW);
  digitalWrite(PIN_PRN_STROBE5, LOW);
  digitalWrite(PIN_PRN_STROBE6, LOW);
  waitForVCCNormalize();
  delayMicroseconds(5);
  switch(segment) {
    case 1:
      digitalWrite(PIN_PRN_STROBE1, HIGH);
      break;
    case 2:
      digitalWrite(PIN_PRN_STROBE2, HIGH);
      break;
    case 3:
      digitalWrite(PIN_PRN_STROBE3, HIGH);
      break;
    case 4:
      digitalWrite(PIN_PRN_STROBE4, HIGH);
      break;
    case 5:
      digitalWrite(PIN_PRN_STROBE5, HIGH);
      break;
    default:
      digitalWrite(PIN_PRN_STROBE6, HIGH);
      break;
  }
  delayMicroseconds(Time);
  digitalWrite(PIN_PRN_STROBE1, LOW);
  digitalWrite(PIN_PRN_STROBE2, LOW);
  digitalWrite(PIN_PRN_STROBE3, LOW);
  digitalWrite(PIN_PRN_STROBE4, LOW);
  digitalWrite(PIN_PRN_STROBE5, LOW);
  digitalWrite(PIN_PRN_STROBE6, LOW);
}

void motorClear() {
  digitalWrite(PIN_MOTOR_DIR, 1); //DIR
  digitalWrite(PIN_MOTOR_SLEEP, 1); //SLEEP
  digitalWrite(PIN_MOTOR_RESET, 0); //RESET
}

void shiftOneLine() {
  waitForVCCNormalize();
  digitalWrite(PIN_MOTOR_RESET, 1); //NO RESET
  digitalWrite(PIN_MOTOR_STEP, motor_step_val);
  delayMicroseconds(500);
  motor_step_val = !motor_step_val;
}

void printOneLine() {
  bool active_colors[15] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  for(int i = 0; i < 384; i++) {
    if(vals[i] != 0) {
      active_colors[vals[i]-1] = true;
    }
  }
  for(int l = 0; l < 4; l++) { //Write each line N times to keep correct V/H ratio
    for(int j = 1; j < 16; j+=1) { //Go through every brightness value
        if(!active_colors[j-1]) {
          continue;
        }
        bool en[6] = {0, 0, 0, 0, 0};
        for(int k = 0; k < 384; k++) { //Go through every pixel in row
          int val = vals[k];
          if(val == j) {
            printVals[k] = 1; //If at least one pixel set, enable needed stb
            int stbid = (k/64);
            if(!en[stbid]) en[stbid] = true;
          } else {
            printVals[k] = 0;
          }
        }
        if(en[0] || en[1] || en[2] || en[3] || en[4] || en[5]) {
          prnLoadBuffer();
          long Time = map(j, 1L, 15L, PrnBurnDelayMin, PrnBurnDelayMax);
          for(int i = 1; i < 7; i++) {
            if(en[i-1]) {
              prnStrobe(i, Time);
            }
          }
        }
    }
    shiftOneLine();
  }
  if(getTempThermistor() >= MAX_TEMP) {
    Serial.println("OVERHEAT!");
  }
  prnClear();
}

unsigned int strToHex(const char* str) {
 return (unsigned int) strtoul(str, 0, 16);
}

String readInput() {
  String input = "";
  input = Serial.readStringUntil('\n');
  input.trim();
  return input;
}

bool readPartToVals(int num, int begin_index, unsigned int size) {
  Serial.print("r");Serial.println(num);
  String input = readInput();
  if(input.length() != size) {
    Serial.print("length error(");Serial.print(input.length());Serial.print(") ; (");Serial.print(input);Serial.println(" )");
    return false;
  }
  for(unsigned int i = 0; i < size; i++) {
    char sb = input.charAt(i);
    long val = sb - '0';
    vals[begin_index + i] = val;
  }
  return true;
}

void setup() {
  Serial.begin(500000);
  Serial.setTimeout(60000);
  analogReference(EXTERNAL); //3.3v
  
  pinMode(PIN_MOTOR_DIR, OUTPUT);
  pinMode(PIN_MOTOR_STEP, OUTPUT);
  pinMode(PIN_MOTOR_SLEEP, OUTPUT);
  pinMode(PIN_MOTOR_RESET, OUTPUT);
  pinMode(PIN_PRN_DATA_IN, OUTPUT);
  pinMode(PIN_PRN_CLK, OUTPUT);
  pinMode(PIN_PRN_LATCH, OUTPUT);
  digitalWrite(PIN_PRN_LATCH, HIGH);
  pinMode(PIN_PRN_STROBE1, OUTPUT);
  pinMode(PIN_PRN_STROBE2, OUTPUT);
  pinMode(PIN_PRN_STROBE3, OUTPUT);
  pinMode(PIN_PRN_STROBE4, OUTPUT);
  pinMode(PIN_PRN_STROBE5, OUTPUT);
  pinMode(PIN_PRN_STROBE6, OUTPUT);

  prnClear();
  motorClear();

  EEPROM.get(0, PrnBurnDelayMin);
  EEPROM.get(2, PrnBurnDelayMax);
  if(PrnBurnDelayMin < 1) {
    PrnBurnDelayMin = 1;
    EEPROM.put(0, PrnBurnDelayMin);
  }
  if(PrnBurnDelayMax <= PrnBurnDelayMin) {
    PrnBurnDelayMax = PrnBurnDelayMin + 1;
    EEPROM.put(2, PrnBurnDelayMax);
  }

  waitForVCCNormalize();
  
  Serial.println("setup");
  while(readInput() != "ok");
  Serial.println("ok");
}

void loop() {
  if(Serial.available() > 0) {
    String input = readInput();
    if(input == "getTemp") {
      Serial.println(getTempThermistor());
    } else if(input == "getVCC") {
      Serial.println(getVCC());
    } else if(input == "print") {
      const int single_transmission = 64;
      for(int i = 0; i < 384; i+=single_transmission) {
        if(!readPartToVals(i/single_transmission, i, single_transmission)) {
          return;
        }
      }
      printOneLine();
    } else if(input == "shift") {
        shiftOneLine();
    } else if(input == "msd") {
        motorClear();
    } else if(input == "setMinTime") {
      Serial.println("r");
      input = readInput();
      uint16_t newTime = input.toInt();
      if(newTime < 1) {
        Serial.println("error");
        return;
      }
      PrnBurnDelayMin = newTime;
      EEPROM.put(0, PrnBurnDelayMin);
    } else if(input == "setMaxTime") {
      Serial.println("r");
      input = readInput();
      uint16_t newTime = input.toInt();
      if(newTime < 1 || newTime < PrnBurnDelayMin) {
        Serial.println("error");
        return;
      }
      PrnBurnDelayMax = newTime;
      EEPROM.put(2, PrnBurnDelayMax);
    } else if(input == "getMinTime") {
      Serial.println(PrnBurnDelayMin);
    } else if(input == "getMaxTime") {
      Serial.println(PrnBurnDelayMax);
    } else {
      Serial.println("unknown");
      return;
    }
    Serial.println("f");
  }
}

/* 
print
0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?
0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?
0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?
0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?
0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?
0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?0?
shift
*/
