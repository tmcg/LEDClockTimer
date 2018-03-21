
/*
  Driving a 74HC595 shift register IC for a 7-segment display, 
 with a TPIC6B595 shift register to sink the LED 7 segment 
 display output current. 
 
 Current for each digit: ~40mA  (8 segments incl decimal * ~5.0mA per segment)
 */

#include <TimeLib.h>

#define SREG_CLOCK_PIN 2
#define SREG_LATCH_PIN 3
#define SREG_DATA_PIN 4
#define ALARM_ENABLE_PIN 8
#define ALARM_TONE_PIN 12
#define ONBOARD_LED_PIN 13

#define SWITCH_COUNT 3
byte SWITCH_PINS[SWITCH_COUNT] = { 7, 6, 5 };
byte SWITCH_LED_PINS[SWITCH_COUNT] = { 11, 10, 9 };

#define SELECT_DD (0x01)
#define SELECT_D1 (0x01<<1)
#define SELECT_D2 (0x01<<3)
#define SELECT_D3 (0x01<<5)
#define SELECT_D4 (0x01<<7)

#define SEG_A (0x01)
#define SEG_B (0x01<<1)
#define SEG_C (0x01<<2)
#define SEG_D (0x01<<3)
#define SEG_E (0x01<<4)
#define SEG_F (0x01<<5)
#define SEG_G (0x01<<6)
#define SEG_P (0x01<<7)

byte DIGIT_DATA[] = { 
      /* 0 */ SEG_A|SEG_B|SEG_C|SEG_D|SEG_E|SEG_F,
      /* 1 */ SEG_B|SEG_C,
      /* 2 */ SEG_A|SEG_B|SEG_E|SEG_D|SEG_G,
      /* 3 */ SEG_A|SEG_B|SEG_C|SEG_D|SEG_G,
      /* 4 */ SEG_B|SEG_C|SEG_F|SEG_G,
      /* 5 */ SEG_A|SEG_C|SEG_D|SEG_F|SEG_G,
      /* 6 */ SEG_A|SEG_C|SEG_D|SEG_E|SEG_F|SEG_G,
      /* 7 */ SEG_A|SEG_B|SEG_C,
      /* 8 */ SEG_A|SEG_B|SEG_C|SEG_D|SEG_E|SEG_F|SEG_G,
      /* 9 */ SEG_A|SEG_B|SEG_C|SEG_D|SEG_F|SEG_G
};
byte DIGIT_DATA_LEN = sizeof(DIGIT_DATA)/sizeof(byte);
byte DIGIT_OFF = DIGIT_DATA_LEN+1;

#define SWITCH_R 0
#define SWITCH_A 1
#define SWITCH_B 2
#define TIMER_MAX 3600
#define TIMER_PLUS_A 300
#define TIMER_PLUS_B 30
#define ALARM_PERIOD 20
#define CLOCK_PLUS_A 3600
#define CLOCK_PLUS_B 60

#define MODE_CLOCK 0
#define MODE_TIMER 1
#define FREQ_SEPARATOR 200
#define FREQ_ALARM 200
#define ALARM_BUZZER true

struct clock_t {
  byte cx_CurrentMode;
  byte cx_SwitchState[SWITCH_COUNT];
  byte cx_MomentState[SWITCH_COUNT];
  time_t cx_CountdownStart;
  short cx_CountdownLength;
  time_t cx_CurrentTime;
  unsigned long cx_ClockMillis;
};

struct clock_t deviceData;


/****************** Melody Encoding ******************/
/*****************************************************/
// Melody Encoding
int noteShift = 0, bankShift = 4, lengthShift = 8;
int noteMask = (0x0F << noteShift);
int bankMask = (0x0F << bankShift);
int lengthMask = (0xFF << lengthShift);

// Note Banks
int noteBankC2[]  = { 0, 65, 73, 82, 87, 98, 110, 124 }; // C2 Octave
int noteBankC2S[]  = { 0, 69, 78, 85, 93, 104, 117, 127 }; // One semitone above C2
int noteBankC3[]  = { 0, 131, 147, 165, 175, 196, 220, 247 }; // C3 Octave
int noteBankC3S[] = { 0, 139, 156, 170, 185, 208, 233, 254 }; // One semitone above C3
int noteBankC4[]  = { 0, 262, 294, 329, 349, 392, 440, 494 }; // C4 Octave
int noteBankC4S[] = { 0, 277, 311, 339, 370, 415, 466, 509 }; // One semitone above C4
int noteBankC5[]  = { 0, 523, 587, 659, 698, 783, 880, 988 }; // C5 Octave
int noteBankC5S[]  = { 0, 555, 622, 679, 740, 831, 932, 1018 }; // One semitone above C5

int* noteBanks[] = { noteBankC2, noteBankC2S, noteBankC3, noteBankC3S, noteBankC4, noteBankC4S, noteBankC5, noteBankC5S };
int k2 = (0x00<<bankShift), k2s = (0x01<<bankShift),
    k3 = (0x02<<bankShift), k3s = (0x03<<bankShift),
    k4 = (0x04<<bankShift), k4s = (0x05<<bankShift),
    k5 = (0x06<<bankShift), k5s = (0x07<<bankShift);

// Note constants to help with notation
int x = 0, _C = 1, _D = 2, _E = 3, _F = 4, _G = 5, _A = 6, _B = 7;
int _C2 = _C+k2, _D2 = _D+k2, _E2 = _E+k2, _F2 = _F+k2, _G2 = _G+k2, _A2 = _A+k2, _B2 = _B+k2;
int _C2s = _C+k2s, _D2s = _D+k2s, _E2s = _E+k2s, _F2s = _F+k2s, _G2s = _G+k2s, _A2s = _A+k2s, _B2s = _B+k2s;
int _C3 = _C+k3, _D3 = _D+k3, _E3 = _E+k3, _F3 = _F+k3, _G3 = _G+k3, _A3 = _A+k3, _B3 = _B+k3;
int _C3s = _C+k3s, _D3s = _D+k3s, _E3s = _E+k3s, _F3s = _F+k3s, _G3s = _G+k3s, _A3s = _A+k3s, _B3s = _B+k3s;
int _C4 = _C+k4, _D4 = _D+k4, _E4 = _E+k4, _F4 = _F+k4, _G4 = _G+k4, _A4 = _A+k4, _B4 = _B+k4;
int _C4s = _C+k4s, _D4s = _D+k4s, _E4s = _E+k4s, _F4s = _F+k4s, _G4s = _G+k4s, _A4s = _A+k4s, _B4s = _B+k4s;
int _C5 = _C+k5, _D5 = _D+k5, _E5 = _E+k5, _F5 = _F+k5, _G5 = _G+k5, _A5 = _A+k5, _B5 = _B+k5;
int _C5s = _C+k5s, _D5s = _D+k5s, _E5s = _E+k5s, _F5s = _F+k5s, _G5s = _G+k5s, _A5s = _A+k5s, _B5s = _B+k5s;

// Note Length
int t1 = (1<<lengthShift),
    t20 = (20<<lengthShift), t40 = (40<<lengthShift), 
    t60 = (60<<lengthShift), t80 = (80<<lengthShift);

int twinkleTwinkle[] = { x
      , _C4 + t40, _C4 + t40, _G4 + t40, _G4 + t40, _A4 + t40, _A4 + t40, _G4 + t80 , x + t40
      , _F4 + t40, _F4 + t40, _E4 + t40, _E4 + t40, _D4 + t40, _D4 + t40, _C4 + t80 , x + t40
      , _G4 + t40, _G4 + t40, _F4 + t40, _F4 + t40, _E4 + t40, _E4 + t40, _D4 + t80 , x + t40
      , _G4 + t40, _G4 + t40, _F4 + t40, _F4 + t40, _E4 + t40, _E4 + t40, _D4 + t80 , x + t40
      , _C4 + t40, _C4 + t40, _G4 + t40, _G4 + t40, _A4 + t40, _A4 + t40, _G4 + t80 , x + t40
      , _F4 + t40, _F4 + t40, _E4 + t40, _E4 + t40, _D4 + t40, _D4 + t40, _C4 + t80 , x + t40
};
int twinkleTwinkleLen = sizeof(twinkleTwinkle)/sizeof(int);
int twinkleTwinkleTempo = 300;

int letItGo[] = { x
    , _F4  + t20, _G4  + t20, _G4s + t80, x + t20, _D4s + t20, _C5 + t20, _A4s + t80, x + t20
    , _G4s + t40, _F4  + t20, _F4  + t20, _F4  + t40, _F4 + t40, _G4  + t20, _G4s + t80, x + t20
    , _F4  + t20, _G4  + t20, _G4s + t80, x + t20, _D4s + t20, _C5 + t20, _A4s + t80, x + t20
    , _G4s + t20, _A4s + t20, _C5  + t20, x + t1, _C5  + t20, x + t1,  _C5  + t20, x + t1
    , _C5s + t40, _C5 + t20, _A4s + t20, _G4s + t20, _A4s + t20, _G4s + t80, x + t20
    , _D5s + t60, _C5  + t60, _A4s + t60, x + t1, _C5  + t40, x + t1, _C5 + t40, x + t1
    , _G4s + t40, _D5s + t60, _C5  + t60, _G4s + t80, x + t20
    , _G4s + t20, _G4s + t20, _G4  + t60, _D4s + t60, x + t1, _C4s + t80, x + t20
    , _D4s + t20, _C4s + t40, _C4s + t20, _C4  + t20, _C4s + t20, _C4s + t20, _C4 + t20
    , _C4s + t20, _C4 + t20, _G3s + t80, x
};
int letItGoLen = sizeof(letItGo)/sizeof(int);
int letItGoTempo = 300;

/*****************************************************/
/*****************************************************/

void playTune(int outputPin, int abortPin, int* tune, int tuneLen, int defaultNoteLength) {
  int noteIndex = 0;
  int noteBank = 0;
  int noteLength = defaultNoteLength;
  int toneLength = defaultNoteLength;
  int toneFrequency = noteBankC4[_C]; // Middle C
  
  for (int i = 0; i < tuneLen; i++) {
    // Note frequency is a lookup into multiple banks of up to 16 notes
    noteBank = (tune[i] & bankMask) >> bankShift;
    noteIndex = (tune[i] & noteMask) >> noteShift;
    toneFrequency = noteBanks[noteBank][noteIndex];

    noteLength = ((tune[i] & lengthMask) >> lengthShift)*10;
    toneLength = noteLength > 0 ? noteLength : defaultNoteLength;

    if(toneFrequency > 0) {
      tone(outputPin, toneFrequency, toneLength);
    }

    delay(toneLength);
    if(digitalRead(abortPin)) { break; }
  }
}

boolean enablePeriod(unsigned long msec, unsigned long freq) {
  return ((msec/freq)%10) > 5;
}

void shiftOutSegmentDisplay(byte shift1, byte shift2) {
  digitalWrite(SREG_LATCH_PIN,LOW);
  shiftOut(SREG_DATA_PIN,SREG_CLOCK_PIN,MSBFIRST,shift1);
  shiftOut(SREG_DATA_PIN,SREG_CLOCK_PIN,MSBFIRST,shift2);
  digitalWrite(SREG_LATCH_PIN,HIGH);
}

void clearSegmentDisplay() {
  shiftOutSegmentDisplay(0x00,0x00);
}

void writeSegmentDigit(byte digit, boolean decimal, byte options) {
  byte decimalData = (decimal) ? SEG_P : 0x00;
  byte digitData = (digit < DIGIT_OFF) ? DIGIT_DATA[digit%DIGIT_DATA_LEN] : 0x00;
  
  clearSegmentDisplay();
  shiftOutSegmentDisplay(options, digitData | decimalData);
}

void writeSegmentDisplay(byte digit1, byte digit2, byte digit3, byte digit4, boolean separator, boolean alarm) {
  byte doubleDots = separator ? SELECT_DD : 0x00;
  writeSegmentDigit(digit1,false,SELECT_D1|doubleDots);
  writeSegmentDigit(digit2,false,SELECT_D2|doubleDots);
  writeSegmentDigit(digit3,false,SELECT_D3|doubleDots);
  writeSegmentDigit(digit4,alarm,SELECT_D4|doubleDots);
}


/*
//Alternate way to drive the LED display - ended up with more flicker due to varying current

byte getSegmentDigitData(byte digit, boolean decimal) {
  byte decimalData = (decimal) ? SEG_P : 0x00;
  return (digit < DIGIT_OFF) ? DIGIT_DATA[digit%DIGIT_DATA_LEN] : 0x00;
}

void writeSegmentDisplay(byte digit1, byte digit2, byte digit3, byte digit4, boolean separator, boolean alarm) {
  byte doubleDots = separator ? SELECT_DD : 0x00;
  byte digitData1 = getSegmentDigitData(digit1,false);
  byte digitData2 = getSegmentDigitData(digit2,false);
  byte digitData3 = getSegmentDigitData(digit3,false);
  byte digitData4 = getSegmentDigitData(digit4,alarm);

  clearSegmentDisplay();
  for(int i=0; i <= 7; i++) {
    byte d1a = digitData1&(SEG_A<<i)?SELECT_D1:0x0;
    byte d2a = digitData2&(SEG_A<<i)?SELECT_D2:0x0;
    byte d3a = digitData3&(SEG_A<<i)?SELECT_D3:0x0;
    byte d4a = digitData4&(SEG_A<<i)?SELECT_D4:0x0;
    shiftOutSegmentDisplay(d1a|d2a|d3a|d4a|doubleDots,(SEG_A<<i));
  }
}
*/

void writeClockTime(struct clock_t* device) {
  short clockHour = hour(device->cx_CurrentTime);
  short clockMinute = minute(device->cx_CurrentTime);
  boolean showSeparator = device->cx_SwitchState[SWITCH_R] || enablePeriod(device->cx_ClockMillis,FREQ_SEPARATOR);
  writeSegmentDisplay((clockHour >= 10 ? clockHour/10 : DIGIT_OFF), clockHour%10, clockMinute/10, clockMinute%10, showSeparator, false);
}

void writeTimerTime(struct clock_t* device) {
  short secondsLeft = getTimerSecondsLeft(device);
  boolean alarmActive = getDeviceAlarmActive(device);
  boolean showAlarm = alarmActive && enablePeriod(device->cx_ClockMillis,FREQ_ALARM);

  if(alarmActive && ALARM_BUZZER) {
    clearSegmentDisplay();
    digitalWrite(ALARM_ENABLE_PIN,HIGH);
    playTune(ALARM_TONE_PIN, SWITCH_PINS[SWITCH_R], letItGo, letItGoLen, letItGoTempo);
    digitalWrite(ALARM_ENABLE_PIN,LOW);
    updateDeviceMode(device,MODE_CLOCK);
  } else {
    int mins = ( secondsLeft / 60 ) % 60;
    int secs = secondsLeft % 60;
    byte mins1 = mins >= 10 ? mins/10 : DIGIT_OFF;
    byte mins2 = mins >= 0 ? mins%10 : DIGIT_OFF;
    byte secs1 = secs >= 0 ? secs/10 : 0;
    byte secs2 = secs >= 0 ? secs%10 : 0;
    writeSegmentDisplay(mins1, mins2, secs1, secs2, true, alarmActive && showAlarm);
  }
}

void updateDeviceTime(struct clock_t* device) {
  if(device->cx_CurrentMode == MODE_TIMER) {
    writeTimerTime(device);
  }
  if(device->cx_CurrentMode == MODE_CLOCK) {
    writeClockTime(device);
  }
}

boolean readSwitch(byte switchIndex) {
  return digitalRead(SWITCH_PINS[switchIndex]);
}

boolean switchPressed(struct clock_t* device, byte switchIndex) {
  return !device->cx_SwitchState[switchIndex] && device->cx_MomentState[switchIndex];
}

boolean switchReleased(struct clock_t* device, byte switchIndex) {
  return device->cx_SwitchState[switchIndex] && !device->cx_MomentState[switchIndex];
}

void updateTime(byte hh, byte mm, byte ss) {
  setTime(hh,mm,ss,1,1,2014); // Don't care about the date
}

int getTimerSecondsLeft(struct clock_t* device) {
  return device->cx_CountdownStart + device->cx_CountdownLength - device->cx_CurrentTime;
}

boolean getDeviceAlarmActive(struct clock_t* device) {
  int secondsLeft = getTimerSecondsLeft(device);
  return device->cx_CurrentMode == MODE_TIMER && secondsLeft > -ALARM_PERIOD && secondsLeft <= 0;
}

void resetTimer(struct clock_t* device, int countdownLength) {
  device->cx_CountdownStart = device->cx_CurrentTime;
  device->cx_CountdownLength = countdownLength;
}

void incrementTimer(struct clock_t* device, short value) {
  int secondsLeft = getTimerSecondsLeft(device);
  secondsLeft = (secondsLeft < 0) ? value : secondsLeft + value;
  secondsLeft = (secondsLeft >= TIMER_MAX) ? TIMER_MAX - 1 : secondsLeft;
  resetTimer(device,secondsLeft);
}

void incrementClock(short value) {
  time_t t = now() + value;
  updateTime(hour(t),minute(t),0);
}

void updateDeviceMode(struct clock_t* device, byte newMode) {
  boolean modeChange = device->cx_CurrentMode != newMode;
  device->cx_CurrentMode = newMode;

  if(modeChange) {
    resetTimer(device,0);
  }
}

void updateSwitchLED(byte switchIndex, boolean enableLED)
{
  digitalWrite(SWITCH_LED_PINS[switchIndex], enableLED ? HIGH : LOW);
}

void updateSwitchStatus(struct clock_t* device) {
  for(int i=0; i < SWITCH_COUNT; i++) {
    device->cx_MomentState[i] = readSwitch(i);
  }

  boolean switchReleased_R = switchReleased(device,SWITCH_R);
  boolean switchReleased_A = switchReleased(device,SWITCH_A);
  boolean switchReleased_B = switchReleased(device,SWITCH_B);
  boolean switchPressed_R = switchPressed(device,SWITCH_R);
  boolean switchPressed_A = switchPressed(device,SWITCH_A);
  boolean switchPressed_B = switchPressed(device,SWITCH_B);

  if(switchPressed_R || switchReleased_R)
  {
    updateSwitchLED(SWITCH_R,switchPressed_R);
  }
  if(switchPressed_A || switchReleased_A)
  {
    updateSwitchLED(SWITCH_A,switchPressed_A);
  }
  if(switchPressed_B || switchReleased_B)
  {
    updateSwitchLED(SWITCH_B,switchPressed_B);
  }
  
  if(device->cx_MomentState[SWITCH_R]) {
    updateDeviceMode(device,MODE_CLOCK);

    // Are we setting the clock?
    if(switchPressed_A) {
      incrementClock(CLOCK_PLUS_A);
    }
    if(switchPressed_B) {
      incrementClock(CLOCK_PLUS_B);
    }
  }
  else {
    if(switchPressed_A) {
      updateDeviceMode(device, MODE_TIMER);
      incrementTimer(device, TIMER_PLUS_A);
    }
    if(switchPressed_B) {
      updateDeviceMode(device, MODE_TIMER);
      incrementTimer(device, TIMER_PLUS_B);
    }
  }

  for(int i=0; i < SWITCH_COUNT; i++) {
    device->cx_SwitchState[i] = device->cx_MomentState[i];
  }
}

void setup() {
  clearSegmentDisplay();
  
  // Wait for the switch debounce capacitors to charge
  delay(200);

  deviceData.cx_CurrentMode = MODE_CLOCK;
  deviceData.cx_CurrentTime = now();
  deviceData.cx_ClockMillis = millis();
  deviceData.cx_CountdownStart = deviceData.cx_CurrentTime;
  deviceData.cx_CountdownLength = 0;
  
  pinMode(SREG_CLOCK_PIN,OUTPUT);
  pinMode(SREG_DATA_PIN,OUTPUT);
  pinMode(SREG_LATCH_PIN,OUTPUT);
  digitalWrite(SREG_CLOCK_PIN,LOW);
  digitalWrite(SREG_DATA_PIN,LOW);
  digitalWrite(SREG_LATCH_PIN,LOW);
  
  pinMode(ALARM_TONE_PIN,OUTPUT);
  pinMode(ALARM_ENABLE_PIN,OUTPUT);
  digitalWrite(ALARM_TONE_PIN,LOW);
  digitalWrite(ALARM_ENABLE_PIN,LOW);
  
  // Disable the onboard LED to hide floating voltage on that pin
  pinMode(ONBOARD_LED_PIN,OUTPUT);
  digitalWrite(ONBOARD_LED_PIN,LOW);
  
  for(int i=0; i < SWITCH_COUNT; i++) {
    pinMode(SWITCH_PINS[i],INPUT_PULLUP);
    pinMode(SWITCH_LED_PINS[i],OUTPUT);
    digitalWrite(SWITCH_LED_PINS[i],LOW);
    deviceData.cx_SwitchState[i] = 0x0;
  }
  
  clearSegmentDisplay();
  updateTime(0,0,0);
}

void loop() {
  deviceData.cx_CurrentTime = now();
  deviceData.cx_ClockMillis = millis();

  updateSwitchStatus(&deviceData);
  updateDeviceTime(&deviceData);
}




