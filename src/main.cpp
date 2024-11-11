#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// ### ACE 2024/2025 ###
// 
// ### Lab1 ###
//
// ### Description ###
// -Finite state machine to control led strip
// -Count Down of up to N=5 leds acting as a timer
// -Navigate the configurations using buttons or Serial_in
// 
// ### Authors ###
// Christian Geyer
// Maria Costa

#define MAXIMUM_NUM_NEOPIXELS 5
#define LED_PIN 6
int N = 5; // number of leds in countdown

// When setting up the NeoPixel library, we tell it how many pixels,
// and which pin to use to send signals. Note that for older NeoPixel
// strips you might need to change the third parameter -- see the
// strandtest example for more information on possible values.
Adafruit_NeoPixel pixels(MAXIMUM_NUM_NEOPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

// struct for a finite state machine
typedef struct{

  uint16_t state;
  uint16_t new_state;
  uint16_t tis; // time in state
  uint16_t tes; // time entering state
} fsm_t;

fsm_t fsm;

uint16_t prev_tis; // to save tis from COUNTDOWN going to FREEZE

enum state_fsm{

  START,
  COUNTDOWN,
  FREEZE,
  BLINK,
  CONFIG
};

void print_state(fsm_t fsm){

  switch(fsm.state){

    case START:
      Serial.print("START");
      break;
    case COUNTDOWN:
      Serial.print("COUNTDOWN");
      break;
    case FREEZE:
      Serial.print("FREEZE");
      break;
    case BLINK:
      Serial.print("BLINK");
      break;
    case CONFIG:
      Serial.print("CONFIG");
      break;
    
  }
}

// update state of a fsm
// update tes and tis in case the state changes
bool set_state(fsm_t &fsm1){

  if(fsm1.new_state != fsm1.state){

    fsm1.state = fsm1.new_state;
    fsm1.tes = millis();
    fsm1.tis = 0;
  }
}

// inputs
bool Sgo, Sesc, Smore;
bool Sgo_prev, Sesc_prev, Smore_prev;
bool Sgo_RE, Sesc_RE, Smore_RE, Smore_FE;
int Sgo_pin = 2, Sesc_pin = 3, Smore_pin = 4;

// struct for a timer 
// for measuring how long a boolean expression has been true
typedef struct{

  bool on;
  uint16_t tit; // time in timer
  uint16_t tet; // time entering timer
} tmr_t;

tmr_t timer_Smore, timer_control;

uint16_t T_control = 50; // time interval between control cycles
uint16_t T_countdown = 2000; // time between leds turning off during COUNTDOWN
uint16_t T_blink = 5000; // time to stay in BLINK state
uint16_t T_Smore = 3000;

// update on, tit and tet based on boolean expression (val)
void update_timer(tmr_t &tmr, bool val){

  if(val){

    if(!tmr.on){

      // start timer
      tmr.on = true;
      // set tet and tit
      tmr.tet = millis();
      tmr.tit = 0;
    }
    else{

      // update tit
      tmr.tit = millis()-tmr.tet;
    }
  }
  else{

    // reset on and tit
    tmr.on = false;
    tmr.tit = 0;
  }
}

#define NTYPES 3
#define NOPTIONS1 4
#define NOPTIONS2 3
#define NOPTIONS3 4

uint16_t T_countdown_v[NOPTIONS1] = {1000, 2000, 5000, 10000};

// struct for configuration
// types: {options}
// 0 -> T_countdown: {0 -> 1s | 1 -> 2s | 2 -> 5s | 3 -> 10s}
// 1 -> effect:      {0 -> SWITCHOFF | 1 -> FASTBLINK | 2 -> FADEOUT}
// 2 -> color:       {0 -> BLUE | 1 -> GREEN | 2 -> YELLOW | 3 -> WHITE}
typedef struct{

  int type = 0;
  int optionsInType[NTYPES] = {NOPTIONS1, NOPTIONS2, NOPTIONS3};
  int option[NTYPES] = {1, 0, 1}; // (2s, SWITCHOFF, GREEN) -> initial configuration
} config_t;

config_t config, config_aux;

enum led_color{

  BLUE,
  GREEN,
  YELLOW,
  WHITE,
  RED
};

// struct for output (leds)
typedef struct{

  bool on;
  int index;
  int intensity;
  int color;
  int r, g, b;
}led_t;

led_t led[MAXIMUM_NUM_NEOPIXELS];

int led_intensity = 50;

void update_rgb(led_t &led){

  switch(led.color){

    case BLUE:
      led.r = 0;
      led.g = 0;
      led.b = led.intensity;
      break;
    case GREEN:
      led.r = 0;
      led.g = led.intensity;
      led.b = 0;
      break;
    case YELLOW:
      led.r = led.intensity;
      led.g = led.intensity;
      led.b = 0;
      break;
    case WHITE:
      led.r = led.intensity;
      led.g = led.intensity;
      led.b = led.intensity;
      break;
    case RED:
      led.r = led.intensity;
      led.g = 0;
      led.b = 0;
      break;
  }
}

bool blink(double t, double f){

  double T = 1000/f;
  return int(t)%int(T) < int(T/2);
}

int f_fastblink = 3;
int f_blink = 2; // 3 Hz
int f_freeze = 1;
int f_config = 1;

char Serial_in = '-';

// update tis of a fsm
void update_tis(fsm_t &fsm1){

  fsm1.tis = millis()-fsm1.tes;
  if(fsm1.tis > (uint16_t)(-T_countdown)) fsm1.tis = 0;
}

void setup(){

  Serial.begin(9600);
  pinMode(Sgo_pin, INPUT_PULLUP);
  pinMode(Sesc_pin, INPUT_PULLUP);
  pinMode(Smore_pin, INPUT_PULLUP);

  N = min(N, MAXIMUM_NUM_NEOPIXELS); // upper limit for N: MAXIMUM_NUM_NEOPIXELS
  if(N < 1) N = 1; // assert that N >= 1

  // initialize leds
  for(int i = 0 ; i < N ; i++){

    led[i].on = false;
    led[i].index = N-i-1;
  }

  fsm.new_state = START;
  set_state(fsm);

  timer_control.on = false;
  timer_Smore.on = false;

  pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
}

void loop(){

  // update timer_control
  update_timer(timer_control, true);
  if(timer_control.tit >= T_control){

    // reset timer_control
    update_timer(timer_control, false);
    
    // read inputs
    Sgo_prev = Sgo;
    Sesc_prev = Sesc;
    Smore_prev = Smore;
    
    Sgo = !digitalRead(Sgo_pin);
    Sesc = !digitalRead(Sesc_pin);
    Smore = !digitalRead(Smore_pin);
    
    Sgo_RE = !Sgo_prev && Sgo;
    Sesc_RE = !Sesc_prev && Sesc;
    Smore_RE = !Smore_prev && Smore;
    Smore_FE = Smore_prev && !Smore;

    // update timers
    update_timer(timer_Smore, Smore);
    update_tis(fsm);

    // Serial input
    if(Serial.available()){

      Serial_in = Serial.read();
    }
    else{

      Serial_in = '-';
    }

    // finite state machine
    if((fsm.state == START) && (Sgo_RE || (Serial_in == 'g'))){

      fsm.new_state = COUNTDOWN;
      set_state(fsm);
    }
    else if((fsm.state == COUNTDOWN) && (Sesc_RE || (Serial_in == 'p'))){

      prev_tis = fsm.tis;
      fsm.new_state = FREEZE;
      set_state(fsm);
    }
    else if((fsm.state == COUNTDOWN) && (Sgo_RE || (Serial_in == 'g') || (fsm.tis >= N*T_countdown))){

      fsm.new_state = BLINK;
      set_state(fsm);
    }
    else if((fsm.state == FREEZE) && (Sesc_RE || (Serial_in == 'r'))){

      fsm.new_state = COUNTDOWN;
      set_state(fsm);
      fsm.tes = millis()-prev_tis;
      update_tis(fsm);
    }
    else if((fsm.state == BLINK) && (Sgo_RE || (Serial_in == 'g') || (fsm.tis >= T_blink))){

      fsm.new_state = START;
      set_state(fsm);
    }
    else if((fsm.state == CONFIG) && (Sesc_RE || (Serial_in == 'e'))){

      fsm.new_state = START;
      set_state(fsm);
      // TODO - discard new config, recover old config saved in config_aux
      config.type = config_aux.type;
      for(int i = 0 ; i < NTYPES ; i++){

        config.option[i] = config_aux.option[i];
      }
    }
    else if((fsm.state == CONFIG) && ((timer_Smore.tit >= T_Smore) || (Serial_in == 's'))){

      fsm.new_state = START;
      set_state(fsm);
      // reset timer_Smore
      update_timer(timer_Smore, false);
      // save new config -> done
    }
    else if((fsm.state != CONFIG) && ((timer_Smore.tit >= T_Smore) || (Serial_in == 'c'))){

      fsm.new_state = CONFIG;
      set_state(fsm);
      // reset timer_Smore
      update_timer(timer_Smore, false);
      // save current config in config_aux
      config_aux.type = config.type;
      for(int i = 0 ; i < NTYPES ; i++){

        config_aux.option[i] = config.option[i];
      }
    }
    else if((fsm.state == COUNTDOWN) && (Smore_FE || (Serial_in == 'm'))){

      fsm.tes += T_countdown;
      // dont let tes exceed millis()
      if(fsm.tes > millis()) fsm.tes = millis();
      update_tis(fsm);
    }
    else if((fsm.state == CONFIG) && (Smore_RE || (Serial_in == 'm'))){

      // TODO: go to next type of configuration
      config.type = (config.type+1)%NTYPES;
    }
    else if((fsm.state == CONFIG) && Sgo_RE){

      // TODO: go to next option of configuration (within the same type)
      config.option[config.type] = (config.option[config.type]+1)%config.optionsInType[config.type];
    }
    else if((fsm.state == CONFIG) && (config.type == 0)){

      switch(Serial_in){

        case '1':
          config.option[0] = 0;
          break;
        case '2':
          config.option[0] = 1;
          break;
        case '5':
          config.option[0] = 2;
          break;
        case 'A':
          config.option[0] = 3;
          break;
      }
    }
    else if((fsm.state == CONFIG) && (config.type == 1)){

      switch(Serial_in){

        case 'o':
          config.option[1] = 0;
          break;
        case 'b':
          config.option[1] = 1;
          break;
        case 'f':
          config.option[1] = 2;
          break;
      }
    }
    else if((fsm.state == CONFIG) && (config.type == 2)){

      switch(Serial_in){

        case 'B':
          config.option[2] = 0;
          break;
        case 'G':
          config.option[2] = 1;
          break;
        case 'Y':
          config.option[2] = 2;
          break;
        case 'W':
          config.option[2] = 3;
          break;
      }
    }

    // update config of T_countdown
    T_countdown = T_countdown_v[config.option[0]];

    // computes outputs
    if(fsm.state == START){

      for(int i = 0 ; i < N ; i++){

        led[i].on = false;
      }
    }
    else if(fsm.state == COUNTDOWN){

      for(int i = 0 ; i < N ; i++){

        led[i].on = fsm.tis<((i+1)*T_countdown);
        led[i].intensity = led_intensity;
        led[i].color = config.option[2];
      }
      // different configs
      if(config.option[1] == 1){ // FASTBLINK

        for(int i = 0 ; i < N ; i++){

          if(!led[i].on){

            led[i].on = blink(fsm.tis, f_fastblink);
            led[i].color = RED;
          }
        }
      }

      else if(config.option[1] == 2){ // FADEOUT

        for(int i = 0 ; i < N ; i++){

          if((fsm.tis > i*T_countdown) && (fsm.tis < (i+1)*T_countdown)){
            
            led[i].intensity = (int)((double)led_intensity*((i+1)-(double)fsm.tis/T_countdown));
          }
        }
      }
    }
    else if(fsm.state == BLINK){

      for(int i = 0 ; i < N ; i++){

        led[i].on = blink(fsm.tis, f_blink);
        led[i].intensity = led_intensity;
        led[i].color = RED;
      }
    }
    else if(fsm.state == FREEZE){

      for(int i = 0 ; i < N ; i++){

        led[i].on = prev_tis < (i+1)*T_countdown;
        led[i].intensity = led_intensity;
        led[i].color = config.option[2];
      }

      for(int i = 0 ; i < N ; i++){

        if(led[i].on){

          led[i].on = blink(fsm.tis, f_freeze);
        }
      }
    }
    else if(fsm.state == CONFIG){

      for(int i = 0 ; i < MAXIMUM_NUM_NEOPIXELS ; i++) led[i].on = false;
      led[MAXIMUM_NUM_NEOPIXELS-1].on = blink(fsm.tis, f_config);
      led[MAXIMUM_NUM_NEOPIXELS-1].intensity = led_intensity;
      led[MAXIMUM_NUM_NEOPIXELS-1].color = config.type;

      for(int i = 0 ; i <= config.option[config.type] ; i++){

        led[i].on = true;
        led[i].intensity = led_intensity;
        led[i].color = config.type;
      }
    }

    // update rgb values
    for(int i = 0 ; i < N ; i++){

      update_rgb(led[i]);
    }

    // show color in led
    for(int i = 0 ; i < MAXIMUM_NUM_NEOPIXELS ; i++){

      pixels.setPixelColor(i, pixels.Color(0, 0, 0));
    }

    for(int i = 0 ; i < N ; i++){

      if(led[i].on) pixels.setPixelColor(led[i].index, pixels.Color(led[i].r, led[i].g, led[i].b));
    }
    pixels.show();

    // print state for debugging
    print_state(fsm);
    Serial.print(" ");
    Serial.print(config.type);
    Serial.print(config.option[config.type]);
    Serial.print(" ");
    for(int i = 0 ; i < NTYPES ; i++) Serial.print(config.option[i]);
    Serial.print(" ");
    for(int i = 0 ; i < N ; i++){

      Serial.print(led[i].on);
      Serial.print(led[i].intensity);
      Serial.print(led[i].r);
      Serial.print(led[i].g);
      Serial.print(led[i].b);
      Serial.print(" ");
    }
    Serial.println();    
  }
}
