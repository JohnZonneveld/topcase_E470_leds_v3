#include <FastLED.h>

// --- CONFIG ---
#define NUM_STRIPS 6
#define BRIGHTNESS 255
#define RUNNING_BRIGHTNESS 100
#define TURN_BRIGHTNESS 255

// Strips 0,1,2 = left side. 3,4,5 = right side
// Left: pins 2,3,4 = 9,8,6 LEDs. Right: pins 5,6,7 = 9,8,6 LEDs
const uint8_t STRIP_PINS[NUM_STRIPS] = {2, 3, 4, 5, 6, 7}; // <-- Updated
const uint8_t STRIP_LENGTHS[NUM_STRIPS] = {9, 8, 6, 9, 8, 6};
const uint8_t MAX_LEDS = 9;

const uint8_t BRAKE_PIN = 8; // <-- Updated
const uint8_t LEFT_PIN = 9; // <-- Updated
const uint8_t RIGHT_PIN = 10; // <-- Updated

const uint16_t DEFAULT_PERIOD_MS = 800;
const uint16_t RUNNING_UPDATE_MS = 50;
const uint8_t RUNNING_LEDS = 3;
const uint8_t SWEEP_PERCENT = 70;
const uint8_t TAIL_LENGTH = 4;

CRGB strips[NUM_STRIPS][9];

// --- STATE ---
struct TurnState {
  bool lastRaw = false;
  uint32_t lastRise = 0;
  uint32_t lastPeriod = DEFAULT_PERIOD_MS;
  uint8_t headPos = 0;
  uint32_t lastStepTime = 0;
  uint32_t stepInterval = DEFAULT_PERIOD_MS * SWEEP_PERCENT / 100 / MAX_LEDS;
  bool isActive = false;
  bool sweepDone = false;
};

TurnState leftTurn, rightTurn;
uint32_t lastRunningUpdate = 0;

void setup() {
  FastLED.addLeds<WS2812B, 2, GRB>(strips[0], STRIP_LENGTHS[0]); // <-- Updated
  FastLED.addLeds<WS2812B, 3, GRB>(strips[1], STRIP_LENGTHS[1]); // <-- Updated
  FastLED.addLeds<WS2812B, 4, GRB>(strips[2], STRIP_LENGTHS[2]); // <-- Updated
  FastLED.addLeds<WS2812B, 5, GRB>(strips[3], STRIP_LENGTHS[3]); // <-- Updated
  FastLED.addLeds<WS2812B, 6, GRB>(strips[4], STRIP_LENGTHS[4]); // <-- Updated
  FastLED.addLeds<WS2812B, 7, GRB>(strips[5], STRIP_LENGTHS[5]); // <-- Updated
  FastLED.setBrightness(BRIGHTNESS);

  pinMode(BRAKE_PIN, INPUT);
  pinMode(LEFT_PIN, INPUT);
  pinMode(RIGHT_PIN, INPUT);

  FastLED.clear(true);
}

void updateTurnTiming(TurnState &state, bool currentRaw, uint32_t now) {
  if (currentRaw &&!state.lastRaw) {
    if (state.lastRise!= 0) {
      uint32_t period = now - state.lastRise;
      if (period > 200 && period < 2000) {
        state.lastPeriod = period;
        state.stepInterval = (period * SWEEP_PERCENT / 100) / MAX_LEDS;
      }
    }
    state.lastRise = now;
    state.headPos = 0;
    state.lastStepTime = now;
    state.isActive = true;
    state.sweepDone = false;
  }

  if (state.isActive && now - state.lastRise > state.lastPeriod * 3 / 2) {
    state.isActive = false;
    state.headPos = 0;
    state.sweepDone = false;
  }

  state.lastRaw = currentRaw;
}

void animateTurn(TurnState &state, bool brakeActive, uint8_t startStrip, uint8_t endStrip, uint32_t now) {
  CRGB fullRed = CRGB(TURN_BRIGHTNESS, 0, 0);

  if (!state.isActive) {
    if (brakeActive) {
      for (uint8_t s = startStrip; s <= endStrip; s++) {
        fill_solid(strips[s], STRIP_LENGTHS[s], fullRed);
      }
    }
    return;
  }

  if (!state.sweepDone && state.headPos < MAX_LEDS && now - state.lastStepTime >= state.stepInterval) {
    state.headPos++;
    state.lastStepTime = now;
    if (state.headPos >= MAX_LEDS) {
      state.sweepDone = true;
    }
  }

  for (uint8_t s = startStrip; s <= endStrip; s++) {
    uint8_t len = STRIP_LENGTHS[s];
    fill_solid(strips[s], len, CRGB::Black);

    if (state.sweepDone) continue;

    uint8_t scaledHead = (state.headPos * len) / MAX_LEDS;

    for (int8_t i = 0; i < TAIL_LENGTH; i++) {
      int8_t pos = scaledHead - 1 - i;
      if (pos >= 0 && pos < len) {
        uint8_t brightness = TURN_BRIGHTNESS * (TAIL_LENGTH - i) / TAIL_LENGTH;
        strips[s][pos] = CRGB(brightness, 0, 0);
      }
    }
  }
}

void updateRunningLights() {
  CRGB dimRed = CRGB(RUNNING_BRIGHTNESS, 0, 0);

  for (uint8_t s = 0; s < NUM_STRIPS; s++) {
    uint8_t len = STRIP_LENGTHS[s];
    uint8_t runCount = (RUNNING_LEDS < len)? RUNNING_LEDS : len;
    for (uint8_t i = 0; i < runCount; i++) {
      strips[s][i] = dimRed;
    }
  }
}

void loop() {
  uint32_t now = millis();

  bool brake = digitalRead(BRAKE_PIN);
  bool leftRaw = digitalRead(LEFT_PIN);
  bool rightRaw = digitalRead(RIGHT_PIN);

  for (uint8_t s = 0; s < NUM_STRIPS; s++) {
    fill_solid(strips[s], STRIP_LENGTHS[s], CRGB::Black);
  }

  updateTurnTiming(leftTurn, leftRaw, now);
  updateTurnTiming(rightTurn, rightRaw, now);

  bool anyTurnActive = leftTurn.isActive || rightTurn.isActive;

  if (brake && leftTurn.isActive &&!rightTurn.isActive) {
    animateTurn(leftTurn, brake, 0, 2, now);
    animateTurn(rightTurn, brake, 3, 5, now);
  }
  else if (brake && rightTurn.isActive &&!leftTurn.isActive) {
    animateTurn(rightTurn, brake, 3, 5, now);
    animateTurn(leftTurn, brake, 0, 2, now);
  }
  else if (brake && leftTurn.isActive && rightTurn.isActive) {
    animateTurn(leftTurn, brake, 0, 2, now);
    animateTurn(rightTurn, brake, 3, 5, now);
  }
  else if (brake &&!anyTurnActive) {
    CRGB brightRed = CRGB(TURN_BRIGHTNESS, 0, 0);
    for (uint8_t s = 0; s < NUM_STRIPS; s++) {
      fill_solid(strips[s], STRIP_LENGTHS[s], brightRed);
    }
  }
  else if (leftTurn.isActive &&!rightTurn.isActive) {
    animateTurn(leftTurn, brake, 0, 2, now);
  }
  else if (rightTurn.isActive &&!leftTurn.isActive) {
    animateTurn(rightTurn, brake, 3, 5, now);
  }
  else if (leftTurn.isActive && rightTurn.isActive) {
    animateTurn(leftTurn, brake, 0, 2, now);
    animateTurn(rightTurn, brake, 3, 5, now);
  }
  else {
    if (now - lastRunningUpdate > RUNNING_UPDATE_MS) {
      updateRunningLights();
      lastRunningUpdate = now;
    }
  }

  FastLED.show();
}
