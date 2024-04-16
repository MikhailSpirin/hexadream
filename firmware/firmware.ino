#include <FastLED.h>
#include <MozziGuts.h>
#include <Oscil.h>
#include <tables/brownnoise8192_int8.h> 
#include <tables/saw8192_int8.h> 
#include <tables/sin8192_int8.h> 
#include <tables/cos2048_int8.h>
#include <tables/triangle_warm8192_int8.h>
#include <ResonantFilter.h>
#include <Portamento.h>
#include <mozzi_midi.h>
#include <mozzi_fixmath.h>
#include <mozzi_rand.h>
#define ENCODER_DO_NOT_USE_INTERRUPTS
#include <Encoder.h>

// FastLED, wave colors
#define BRIGHTNESS 50
#define NUM_LEDS 64
#define SIN_HUE 100
#define SAW_HUE 170
#define TRI_HUE 60
#define NOI_HUE 40
#define FILTER_HUE 30

// define controls
Encoder controlEncoder(32, 33);
#define ACTION_BTN 27
#define TRANSPOSE_UP_BTN 39
#define TRANSPOSE_DOWN_BTN 34

// other consts
#define CONTROL_RATE 64
#define CIRCLE_OF_FIFTH_LENGTH 12
#define LONG_PRESS_TIME 200
#define HARMONICS 6
#define CHORDSCOUNT 4
#define DEBUG 0


CRGB leds[NUM_LEDS];
byte channelColors[8];

Oscil<SAW8192_NUM_CELLS, AUDIO_RATE> osc[HARMONICS];
Oscil<COS2048_NUM_CELLS, AUDIO_RATE> vibratos[HARMONICS];
Portamento<CONTROL_RATE> portamentos[HARMONICS];
LowPassFilter lpf;

byte currentChord = 0;

byte chords[CHORDSCOUNT][HARMONICS] = {
   {0, 5, 7, 12, 24, 36},
   {0, 4, 7, 12, 24, 36},
   {0, 2, 7, 12, 24, 36},
   {0, 3, 7, 12, 24, 36},
 };

float vibratosFrequencies[6] = { 0, 1.5f, 3.f, 1.2f, 1.4f, 1.7f };
byte levels[8] = { 127, 127, 127, 64, 64, 64, 127, 127 };

enum OscType { SAW, SIN, TRI, NOI };
OscType oscTypes[6] = { SAW, SAW, SAW, TRI, TRI, TRI };

// C G D A E B Gb Db Ab Eb Bb F
// 48 55 50 57 52 59 54 49 56 51 58 53

byte circleOfFifth[CIRCLE_OF_FIFTH_LENGTH] = { 48, 55, 50, 57, 52, 59, 54, 49, 56, 51, 58, 53 };
byte currentNote = 0;

byte currentOsc = 6;
long oldEncoderPosition = -10, newEncoderPosition;
byte controlMode = 0;
bool action = LOW, prevAction = HIGH;
bool transposeUpPressed = LOW, prevTransposeUpPressed = HIGH;
bool transposeDownPressed = LOW, prevTransposeDownPressed = HIGH;
byte indicatorUp = 0, indicatorDown = 0;
unsigned long pressedTime = 0, releasedTime = 0;
long nextwave, lastwave;
byte gainscale;

////////////////////////////////
// User functions
////////////////////////////////

// drawing current parameters on LED panel  
void updateLed() {
  if (indicatorUp > 0) {
    	  fadeToBlackBy( leds, NUM_LEDS, 20);
        uint8_t dothue = 0;
        for( int i = 0; i < 8; i++) {
            leds[beatsin16( i+7, 0, NUM_LEDS-1 )] |= CHSV(dothue, 50, 127);
            dothue += 32;
        }
    indicatorUp--;
  } else if (indicatorDown > 0) {
        fadeToBlackBy( leds, NUM_LEDS, 20);
        uint8_t dothue = 0;
        for( int i = 0; i < 8; i++) {
            leds[beatsin16( i+7, 0, NUM_LEDS-1 )] |= CHSV(dothue, 100, 127);
            dothue += 32;
        }
    indicatorDown--;
  } else {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    leds[currentOsc] = CRGB::LightSeaGreen;

    for  (byte channel = 0; channel < 6; channel++)
      switch (oscTypes[channel]) {
        case SIN:
        channelColors[channel] = SIN_HUE;
        break;

        case SAW:
        channelColors[channel] = SAW_HUE;
        break;

        case TRI:
        channelColors[channel] = TRI_HUE;
        break;

        case NOI:
        channelColors[channel] = NOI_HUE;
        break;
      }
    channelColors[6] = FILTER_HUE;
    channelColors[7] = FILTER_HUE;

    for  (byte channel = 0; channel < 8; channel++) {
      byte n = levels[channel] >> 5;
      for (byte i = 1; i <= n; i++) {
        leds[i * 8 + channel] = CHSV(channelColors[channel], map(i, 0, 8, 50, 230), 120);
      }
    }
  }

  FastLED.show();
}

// set frequencies for all 6 channels
void changeFrequencies() {
    for (byte channel = 0; channel < 6; channel++)
      portamentos[channel].start((uint8_t)(circleOfFifth[currentNote] + chords[currentChord % CHORDSCOUNT][channel]));
}

// update frequencies for all 6 channels
void updateFrequencies() {  
    for (byte channel = 0; channel < 6; channel++)
      osc[channel].setFreq_Q16n16(portamentos[channel].next());
}


// changing waveform on current osc
void nextWaveformOnOsc(byte currentOsc) {
  switch (oscTypes[currentOsc]) {
    case SAW:
    osc[currentOsc].setTable(SIN8192_DATA);
    oscTypes[currentOsc] = SIN;
    break;

    case SIN:
    osc[currentOsc].setTable(TRIANGLE_WARM8192_DATA);
    oscTypes[currentOsc] = TRI;
    break;

    case TRI:
    oscTypes[currentOsc] = NOI;
    osc[currentOsc].setTable(BROWNNOISE8192_DATA);
    break;

    case NOI:
    oscTypes[currentOsc] = SAW;
    osc[currentOsc].setTable(SAW8192_DATA);
    break;
  }
}

// a bit of evolving for randomly selected channel
void slowlyRandomizeLevels() {
    if ((int)rand(0, 15) == 0) {
      byte channel = (int)rand(0, 6);
      byte direction = (int)rand(0, 2);
      Serial.println(direction);
      if (direction == 0 && levels[channel] > 5)
          levels[channel] -= 5;
      if (direction == 1 && levels[channel] < 250)
          levels[channel] += 5;
    }
  }

// needed to adjust overall gain based on all channels levels
void adjustCurrentGain() {
  gainscale = 0;
  long gainval = 255 * (long)(levels[0] + levels[1] + levels[2] + levels[3] + levels[4] + levels[5]);
  while (gainval > 255) {
    gainscale++;
    gainval >>= 1;
  }
}

////////////////////////////////
// Main flow
////////////////////////////////

void setup () {
  if (DEBUG == 1) {
    Serial.begin(115200);
  }
  
  pinMode(ACTION_BTN, INPUT_PULLUP);
  pinMode(TRANSPOSE_UP_BTN, INPUT_PULLUP);
  pinMode(TRANSPOSE_DOWN_BTN, INPUT_PULLUP);

  FastLED.addLeds<WS2812B, 2, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear(); 
  
  for (byte channel = 0; channel < 6; channel++) {
    osc[channel].setTable(SAW8192_DATA);
    vibratos[channel].setTable(COS2048_DATA);
    vibratos[channel].setFreq(vibratosFrequencies[channel]);
    portamentos[channel].setTime(200u);
  }
  changeFrequencies();
  startMozzi(CONTROL_RATE);                        
}  

void updateControl(){
  if (DEBUG == 1) {
    Serial.print(int(transposeUpPressed));
    Serial.print(";");
    Serial.print(currentNote);
    Serial.print(";");
    Serial.println(circleOfFifth[currentNote]);
  }
  action = digitalRead(ACTION_BTN);
  if (action == LOW && prevAction == HIGH)
    pressedTime = millis();
  else if (action == HIGH && prevAction == LOW) {
    releasedTime = millis();
    if (releasedTime - pressedTime < LONG_PRESS_TIME)
      controlMode ^= 1;
    else if (currentOsc == 7) {
        currentChord++;
        changeFrequencies();
    }
    else
      nextWaveformOnOsc(currentOsc); 
  }
  prevAction = action;

  transposeUpPressed = digitalRead(TRANSPOSE_UP_BTN);
  if (transposeUpPressed == LOW && prevTransposeUpPressed == HIGH) {
    currentNote++;
    if (currentNote >= CIRCLE_OF_FIFTH_LENGTH) 
      currentNote = 0;
    indicatorUp = 32;
    changeFrequencies();
  }
  prevTransposeUpPressed = transposeUpPressed;

  transposeDownPressed = digitalRead(TRANSPOSE_DOWN_BTN);
  if (transposeDownPressed == LOW && prevTransposeDownPressed == HIGH) {
    currentNote--;
    if (currentNote == 255)
      currentNote = CIRCLE_OF_FIFTH_LENGTH - 1;
    indicatorDown = 32;
    changeFrequencies();

  }
 prevTransposeDownPressed = transposeDownPressed;


  newEncoderPosition = controlEncoder.read();
  if (newEncoderPosition != oldEncoderPosition && abs(newEncoderPosition - oldEncoderPosition) > 1) {
      if (controlMode == 0 && newEncoderPosition > oldEncoderPosition && currentOsc < 7){
        currentOsc++;
      }
      if (controlMode == 0 && newEncoderPosition < oldEncoderPosition && currentOsc > 0){
        currentOsc--;
      }
      if (controlMode == 1 && newEncoderPosition > oldEncoderPosition && levels[currentOsc] < 224){
        levels[currentOsc] += 32;
      }
      if (controlMode == 1 && newEncoderPosition < oldEncoderPosition && levels[currentOsc] > 32){
        levels[currentOsc] -= 32;
      }    
      oldEncoderPosition = newEncoderPosition; 
      lpf.setCutoffFreqAndResonance(levels[6] >> 1, levels[7]);
  }

  updateFrequencies();
  slowlyRandomizeLevels();
  adjustCurrentGain();
  updateLed();
}

AudioOutput_t updateAudio() {
  lastwave = (levels[0] * osc[0].next() +
              levels[1] * osc[1].next() +
              levels[2] * osc[2].next() +
              levels[3] * osc[3].next() +
              levels[4] * osc[4].next() +
              levels[5] * osc[5].next());          
  nextwave  = lpf.next(lastwave) >> gainscale;
  return (int)((nextwave) >> 1);
}


  // lastwave = ((long)levels[0] * osc[0].phMod((Q15n16) vibratos[0].next()) +
  //             levels[1] * osc[1].phMod((Q15n16) vibratos[1].next()) +
  //             levels[2] * osc[2].phMod((Q15n16) vibratos[2].next()) +
  //             levels[3] * osc[3].phMod((Q15n16) vibratos[3].next()) +
  //             levels[4] * osc[4].phMod((Q15n16) vibratos[4].next()) +
  //             levels[5] * osc[5].phMod((Q15n16) vibratos[5].next()));          
  // nextwave  = lpf.next(lastwave) >> gainscale;
  // return (int)((nextwave) >> 1);

void loop(){
  audioHook();
}