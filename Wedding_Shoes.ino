#include <Adafruit_LSM303_U.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>

// Hardware defines
#define LEDS_PIN A0
#define NUM_PIXELS 16
#define ACCEL_GND A2 

// State defines
#define STATE_CALIBRATING 0
#define STATE_UP 1
#define STATE_DOWN 2
#define STATE_STANDING_1 3
#define STATE_STANDING_2 4
#define STATE_STANDING_3 5

// Tunables
#define MAX_MAG_FOR_DOWN 10.5
#define MIN_MAG_FOR_DOWN 9.1
#define RESTING_RANGE 1.5
#define DEBOUNCE_ITERATIONS 4
// original values - 11.1; 8.6; .9; 5

int state = STATE_CALIBRATING;
int debounce_counter = 0;
double resting_x, resting_y, resting_z;

// Color/animation Variables
#define MAX_FRAMES 50
int frame = 0;
unsigned long time = 0;    // keeps track of different event start times
int color[3];
byte hue;
byte saturation;

// Hardware objects
Adafruit_NeoPixel led_strip = Adafruit_NeoPixel( NUM_PIXELS, LEDS_PIN, NEO_GRB + NEO_KHZ800 );
Adafruit_LSM303_Accel_Unified accel = Adafruit_LSM303_Accel_Unified();

// utility method to calculate the acceleration vector (which should be close to 9.8 when down)
double getMagnitude( sensors_event_t event ){
  double x = event.acceleration.x;
  double y = event.acceleration.y;
  double z = event.acceleration.z;

  double vector = x*x + y*y + z*z;
  vector = sqrt( vector );
  return vector;
}

// figure out the "normal" x/y/z values for when the show is flat down
// this happens once when starting up
void calibrateAccelerometer(){

  showState();

  double readings_x = 0 , readings_y = 0, readings_z = 0;
  sensors_event_t event; 
    
  for( int i=0; i<10; i++ ){
    accel.getEvent(&event);
    readings_x += event.acceleration.x;
    readings_y += event.acceleration.y;
    readings_z += event.acceleration.z;
    delay(100);
  }
  
  resting_x = readings_x / 10.0;
  resting_y = readings_y / 10.0;
  resting_z = readings_z / 10.0;

  Serial.print( "Resting X: " ); Serial.println( resting_x );
  Serial.print( "Resting Y: " ); Serial.println( resting_y );
  Serial.print( "Resting Z: " ); Serial.println( resting_z );

  state = STATE_DOWN;
}

// render state on the LEDs
void showState(){

  switch( state ){
    
    case STATE_UP:
      led_strip.clear();
    break;
    
    case STATE_DOWN:
    case STATE_STANDING_1:
    case STATE_STANDING_2:
    case STATE_STANDING_3:
      showPatternColorWithSparkles();
    break;

    case STATE_CALIBRATING:
      for( int i=0; i<NUM_PIXELS; i++ )
        led_strip.setPixelColor( i, 0, 0 , 255 );
    break;
  }

  led_strip.show();
}

void showPatternColorWithSparkles(){
    
  /* figure out what to do with these...
   if( stepChanged() ){
    if( stepping ){
      state = STEPPED;
      if( frame < 10 ){
        hue = random(255);
        saturation = random(150, 255);
      }
    }
    else{
      // what's this for? lift??
      state = STANDING_2;
      hue = random(255);
    }
  }*/

  // first frame
  if( frame == 0 ){
    hue = random(255);
    saturation = random(150, 255);
  }
    
  byte level;
  switch( state ){

    // this is when you foot first goes down
    case STATE_DOWN:  // fade in w/ sparkles
      if( frame < MAX_FRAMES )
        frame++;
      level = map( frame, 0, MAX_FRAMES, 0, 255 );
      getRGB( hue, saturation, level, color );
      // set the color
      for( int i=0; i<NUM_PIXELS; i++ ){
        led_strip.setPixelColor( i, color[0], color[1], color[2] );
      }
      // set some white
      for( int i=0; frame>10 && i<random(3); i++ ){
        led_strip.setPixelColor( random(NUM_PIXELS), 255, 255, 255);
      }
      if( frame==MAX_FRAMES ){
        // animation complete, transition to next state (solid)
        state = STATE_STANDING_1;
        time = millis();  // record the start time for this
      }
    break;
    
    case STATE_STANDING_1:  // wait
      if( millis() - time > 3000 ){
        state = STATE_STANDING_2;
        hue = random(255);  // switch to another hue before fading out
      }
      getRGB( hue, saturation, 255, color );
      // set the color
      for( int i=0; i<NUM_PIXELS; i++ ){
        led_strip.setPixelColor( i, color[0], color[1], color[2] );
      }
      // set some white
      if( millis() - time < 2000 )
        for( int i=0; i<random(2); i++ )
          led_strip.setPixelColor( random(NUM_PIXELS), 255, 255, 255);
    break;
    
    case STATE_STANDING_2:  // fade out
      if( frame > 0 ) frame-=2;
      if( frame < 0 ) frame = 0;
      if( frame == 0 ) state = STATE_STANDING_3;
      level = map( frame, 0, MAX_FRAMES, 0, 255 );
      getRGB( hue, saturation, level, color );
      // set the color
      for( int i=0; i<NUM_PIXELS; i++ ){
        led_strip.setPixelColor( i, color[0], color[1], color[2] );
      }
    break;
    
  }
  
  led_strip.show();
  delay(10);  
}

void setup() {

  Serial.begin(9600);
  
  led_strip.setBrightness( 50 );
  led_strip.begin();
  led_strip.show();

  /* Initialise the accelerometer */
  pinMode( ACCEL_GND, OUTPUT );
  digitalWrite( ACCEL_GND, LOW );
  delay(100); // give it time to turn on
  if(!accel.begin())
  {
    /* There was a problem detecting the ADXL345 ... check your connections */
    Serial.println("Ooops, no LSM303 detected ... Check your wiring!");
    for( int i=0; i<NUM_PIXELS; i++ )
      led_strip.setPixelColor( i, 255, 0, 0 );
    led_strip.show();
    while(1);
  }

  calibrateAccelerometer();
}

void loop() {

  /* read the accelerometer */ 
  sensors_event_t event; 
  accel.getEvent(&event);
 
  /* Display the results (acceleration is measured in m/s^2) */
  //Serial.print("X: "); Serial.print(event.acceleration.x); Serial.print("  ");
  //Serial.print("Y: "); Serial.print(event.acceleration.y); Serial.print("  ");
  //Serial.print("Z: "); Serial.print(event.acceleration.z); Serial.print("  ");Serial.println("m/s^2 ");

  /* Display the vector */
  double magnitude = getMagnitude( event );

  // Should it be down?
  if( (magnitude < MAX_MAG_FOR_DOWN && magnitude > MIN_MAG_FOR_DOWN) &&
      abs(resting_x - event.acceleration.x) < RESTING_RANGE &&
      abs(resting_y - event.acceleration.y) < RESTING_RANGE &&
      abs(resting_z - event.acceleration.z) < RESTING_RANGE ) 
  {
        // consider transitioning to DOWN
        if( state == STATE_UP ){
          debounce_counter++;
          if( debounce_counter >= DEBOUNCE_ITERATIONS ){
            // transition to down
            state = STATE_DOWN;
            frame = 0; // reset the animation
          }
        }
        else if( state == STATE_DOWN ){
          debounce_counter = 0;
        }
  }

  // or should it be up?
  else{
    if( state == STATE_DOWN ){
      debounce_counter++;
      if( debounce_counter >= DEBOUNCE_ITERATIONS ){
        state = STATE_UP;
        Serial.print( "UP    " );
      }
    }
    else if( state = STATE_UP ){
      debounce_counter = 0;
    }
  }

  showState();
  //delay(100);
}

const byte dim_curve[] = {
    0,   1,   1,   2,   2,   2,   2,   2,   2,   3,   3,   3,   3,   3,   3,   3,
    3,   3,   3,   3,   3,   3,   3,   4,   4,   4,   4,   4,   4,   4,   4,   4,
    4,   4,   4,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   6,   6,   6,
    6,   6,   6,   6,   6,   7,   7,   7,   7,   7,   7,   7,   8,   8,   8,   8,
    8,   8,   9,   9,   9,   9,   9,   9,   10,  10,  10,  10,  10,  11,  11,  11,
    11,  11,  12,  12,  12,  12,  12,  13,  13,  13,  13,  14,  14,  14,  14,  15,
    15,  15,  16,  16,  16,  16,  17,  17,  17,  18,  18,  18,  19,  19,  19,  20,
    20,  20,  21,  21,  22,  22,  22,  23,  23,  24,  24,  25,  25,  25,  26,  26,
    27,  27,  28,  28,  29,  29,  30,  30,  31,  32,  32,  33,  33,  34,  35,  35,
    36,  36,  37,  38,  38,  39,  40,  40,  41,  42,  43,  43,  44,  45,  46,  47,
    48,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,
    63,  64,  65,  66,  68,  69,  70,  71,  73,  74,  75,  76,  78,  79,  81,  82,
    83,  85,  86,  88,  90,  91,  93,  94,  96,  98,  99,  101, 103, 105, 107, 109,
    110, 112, 114, 116, 118, 121, 123, 125, 127, 129, 132, 134, 136, 139, 141, 144,
    146, 149, 151, 154, 157, 159, 162, 165, 168, 171, 174, 177, 180, 183, 186, 190,
    193, 196, 200, 203, 207, 211, 214, 218, 222, 226, 230, 234, 238, 242, 248, 255,
};

/* Convert hue, saturation and brightness ( HSB/HSV ) to RGB
   The dim_curve is used only on brightness/value and on saturation (inverted).
   This looks the most natural.      
*/
void getRGB(int hue, int sat, int val, int colors[3]) { 
  val = dim_curve[val];
  sat = 255-dim_curve[255-sat];
 
  int r;
  int g;
  int b;
  int base;
 
  if (sat == 0) { // Acromatic color (gray). Hue doesn't mind.
    colors[0]=val;
    colors[1]=val;
    colors[2]=val;  
  } else  { 
 
    base = ((255 - sat) * val)>>8;
 
    switch(hue/60) {
    case 0:
        r = val;
        g = (((val-base)*hue)/60)+base;
        b = base;
    break;
 
    case 1:
        r = (((val-base)*(60-(hue%60)))/60)+base;
        g = val;
        b = base;
    break;
 
    case 2:
        r = base;
        g = val;
        b = (((val-base)*(hue%60))/60)+base;
    break;
 
    case 3:
        r = base;
        g = (((val-base)*(60-(hue%60)))/60)+base;
        b = val;
    break;
 
    case 4:
        r = (((val-base)*(hue%60))/60)+base;
        g = base;
        b = val;
    break;
 
    case 5:
        r = val;
        g = base;
        b = (((val-base)*(60-(hue%60)))/60)+base;
    break;
    }
 
    colors[0]=r;
    colors[1]=g;
    colors[2]=b; 
  }   
}

/*  How to detect motion:
 *   
 *  The magnitude vector tells us how many G's we're pulling (with 1g = 9.8m/s). So for one thing, 
 *  if we see ~1g, we're possibly not moving. But, we could be doing 1g in the wrong direction (not down).
 *   
 *  Anothing thing we could do is 'calibrate'. Record the initial X, Y, Z values and consider that to be the 
 *  values we should see when the shoe is down.
 *  
 *  Debounce:
 *  You have to see the other state several times in a row in order to make a transition.
 * 
 */
