#include "math.h"

#include "RGBMoodLifx.h"
#include "Adafruit_PWMServoDriver.h"


// Dim curve
// Used to make 'dimming' look more natural.
uint8_t dc[256] = {
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

// Constructor. Start with leds off.
RGBMoodLifx::RGBMoodLifx(uint8_t type, uint8_t pins[], Adafruit_PWMServoDriver& pwm)
{
  Adafruit_PWMServoDriver pwm_ = pwm;
  /*
  pwm.begin();
  pwm.setPWMFreq(1000);
  */
  type_ = type;
  switch( type_ ) {
    case BULB_RGBW:
      {
        pins_[3] = pins[3];
      }
    case BULB_RGB:
      {
        pins_[2] = pins[2];
        pins_[1] = pins[1];
      }
    default:
      {
        pins_[0] = pins[0];
      }
  }

  mode_ = FIX_MODE; // Stand still

  current_RGB_color_[0] = 0;
  current_RGB_color_[1] = 0;
  current_RGB_color_[2] = 0;
  current_RGB_color_[4] = 0;
  current_HSB_color_[0] = 0;
  current_HSB_color_[1] = 0;
  current_HSB_color_[2] = 0;
  fading_max_steps_ = 200;
  fading_step_time_ = 50;
  holding_color_ = 1000;
  fading_ = false;
  last_update_ = millis();
}

/*
Change instantly the LED colors.
@param h The hue (0..65535) (Will be % 360)
@param s The saturation value (0..255)
@param b The brightness (0..255)
*/
void RGBMoodLifx::setHSB(uint16_t h, uint16_t s, uint16_t b) {
  current_HSB_color_[0] = constrain(h % 360, 0, 360);
  current_HSB_color_[1] = constrain(s, 0, MAX_VAL);
  current_HSB_color_[2] = constrain(b, 0, MAX_VAL);
  hsb2rgb(current_HSB_color_[0], current_HSB_color_[1], current_HSB_color_[2], current_RGB_color_[0], current_RGB_color_[1], current_RGB_color_[2]);
  fading_ = false;
}

/*
Change instantly the LED colors.
@param r The red (0..255)
@param g The green (0..255)
@param b The blue (0..255)
*/
void RGBMoodLifx::setRGB(uint16_t r, uint16_t g, uint16_t b) {
  current_RGB_color_[0] = constrain(r, 0, MAX_PWM);
  current_RGB_color_[1] = constrain(g, 0, MAX_PWM);
  current_RGB_color_[2] = constrain(b, 0, MAX_PWM);
  fading_ = false;
}

void RGBMoodLifx::setRGB(uint32_t color) {
  setRGB((color & 0xFF0000) >> 16, (color & 0x00FF00) >> 8, color & 0x0000FF);
}

/*
Fade from current color to the one provided.
@param h The hue (0..65535) (Will be % 360)
@param s The saturation value (0..255)
@param b The brightness (0..255)
@param shortest Hue takes the shortest path (going up or down)
*/
void RGBMoodLifx::fadeHSB(uint16_t h, uint16_t s, uint16_t b, bool shortest) {
  initial_color_[0] = current_HSB_color_[0];
  initial_color_[1] = current_HSB_color_[1];
  initial_color_[2] = current_HSB_color_[2];
  if (shortest) {
    h = h % 360;
    // We take the shortest way! (0 == 360)
    // Example, if we fade from 10 to h=350, better fade from 370 to h=350.
    //          if we fade from 350 to h=10, better fade from 350 to h=370.
    // 10 -> 350
    if (initial_color_[0] < h) {
      if (h - initial_color_[0] > (initial_color_[0] + 360) - h)
        initial_color_[0] += 360;
    }
    else if (initial_color_[0] > h) { // 350 -> 10
      if (initial_color_[0] - h > (h + 360) - initial_color_[0])
        h += 360;
    }
  }
  target_color_[0] = h;
  target_color_[1] = s;
  target_color_[2] = b;
  fading_ = true;
  fading_step_ = 0;
  fading_in_hsb_ = true;
}

/*
Fade from current color to the one provided.
@param r The red (0..255)
@param g The green (0..255)
@param b The blue (0..255)
*/
void RGBMoodLifx::fadeRGB(uint16_t r, uint16_t g, uint16_t b) {
  initial_color_[0] = current_RGB_color_[0];
  initial_color_[1] = current_RGB_color_[1];
  initial_color_[2] = current_RGB_color_[2];
  target_color_[0] = r;
  target_color_[1] = g;
  target_color_[2] = b;
  fading_ = true;
  fading_step_ = 0;
  fading_in_hsb_ = false;
}

void RGBMoodLifx::fadeRGB(uint32_t color) {
  fadeRGB((color & 0xFF0000) >> 16, (color & 0x00FF00) >> 8, color & 0x0000FF);
}

/*
This function needs to be called in the loop function.
*/
void RGBMoodLifx::tick() {
  unsigned long current_millis = millis();
  if (fading_) {
    // Enough time since the last step ?
    if (current_millis - last_update_ >= fading_step_time_) {
      fading_step_++;
      fade();
      if (fading_step_ >= fading_max_steps_) {
        fading_ = false;
        if (fading_in_hsb_) {
          current_HSB_color_[0] = target_color_[0] % 360;
          current_HSB_color_[1] = target_color_[1];
          current_HSB_color_[2] = target_color_[2];
        }
      }
      last_update_ = current_millis;
    }
  }
  else if (mode_ != FIX_MODE) {
    // We are not fading.
    // If mode_ == 0, we do nothing.
    if (current_millis - last_update_ >= holding_color_) {
      last_update_ = current_millis;
      switch(mode_) {
        case RANDOM_HUE_MODE:
          fadeHSB(random(0, 360), current_HSB_color_[1], current_HSB_color_[2]);
          break;
        case RAINBOW_HUE_MODE:
          fadeHSB(360, current_HSB_color_[1], current_HSB_color_[2], false);
          break;
        case RED_MODE:
          fadeHSB(random(335, 400), random(190*16, 255*16), random(120*16, 255*16));
          break;
        case BLUE_MODE:
          fadeHSB(random(160, 275), random(190*16, 255*16), random(120*16, 255*16));
          break;
        case GREEN_MODE:
          fadeHSB(random(72, 160), random(190*16, 255*16), random(120*16, 255*16));
          break;
        case FIRE_MODE:
          setHSB(random(345, 435), random(190, 255*16), random(120,255*16));
          holding_color_ = random(10, 500);
          break;
      }
    }
  }
  //if (pins_[0] > 0) {
  if (pins_[0] == 4) { // this is the modification for LIFX - allows 0 to be written for each pin to power off the LED
    RGB.control(true);
    RGB.color(current_RGB_color_[0]/16, current_RGB_color_[1]/16, current_RGB_color_[2]/16);
  };
    //set red channel (8 bit value converter to 12 bit value)
    //pwm_.setPWM(1, 0, (((current_RGB_color_[0])*16)));
    switch( type_ ) {

      case BULB_RGBW:
        {
          pwm_.setPin(pins_[3], current_RGB_color_[3]);
        }
      case BULB_RGB:
        {
          pwm_.setPin(pins_[2], current_RGB_color_[2]);
          pwm_.setPin(pins_[1], current_RGB_color_[1]);
        }
      default:
        {
          pwm_.setPin(pins_[0], current_RGB_color_[0]);
        }
    }

}

/*
Convert a HSB color to RGB
This function is used internally but may be used by the end user too. (public).
@param h The hue (0..65535) (Will be % 360)
@param s The saturation value (0..255)
@param b The brightness (0..255)
*/
uint16_t RGBMoodLifx::weberfechner( uint16_t x, uint16_t a, uint16_t b) {
/*  a = Anzahl an Schritte (4, 8, 16, 32, 64, 256)
  b = Auflösung des PWM's (256, 1024, 65536)
  y = Errechneter Wert an einer stelle x
*/
  if ( x == 0 )
    return ( 0 );

  uint16_t y = pow(2, log2(b-1) * (x+1) / a);
  return (y);
}

void RGBMoodLifx::hsb2rgb(uint16_t hue, uint16_t sat, uint16_t val, uint16_t& r, uint16_t& g, uint16_t& b) {
/*
  val = dc[val];
  sat = 255-dc[255-sat];
  hue = hue % 360;

  int r;
  int g;
  int b;
  int base;

  if (sat == 0) { // Acromatic color (gray). Hue doesn't mind.
    red   = val;
    green = val;
    blue  = val;
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
    red   = r;
    green = g;
    blue  = b;
  }
  */
float hue_, val_, sat_, r_ , g_, b_ ;
//hue = fmod(hue,360); // cycle hue around to 0-360 degrees

hue_ = 3.14159*(float)hue/180.0; // Convert to radians.
val_ = (float)val/MAX_VAL*MAX_PWM/3;  // Normalize and scale
sat_ = (float)sat/MAX_VAL;
// sat = sat>0?(sat<1?sat:1):0; // clamp sat and val to interval [0,1]
// val = val>0?(val<1?val:1):0;

// Math! Thanks in part to Kyle Miller.
if(hue_ < 2.09439) {
  r_ = val_*(1.0+sat_*cos(hue_)/cos(1.047196667-hue_));
  g_ = val_*(1.0+sat_*(1.0-cos(hue_)/cos(1.047196667-hue_)));
  b_ = val_*(1.0-sat_);
} else if(hue_ < 4.188787) {
  hue_ = hue_ - 2.09439;
  g_ = val_*(1.0+sat_*cos(hue_)/cos(1.047196667-hue_));
  b_ = val_*(1.0+sat_*(1.0-cos(hue_)/cos(1.047196667-hue_)));
  r_ = val_*(1.0-sat_);
 } else {
  hue_ = hue_ - 4.188787;
  b_ = val_*(1.0+sat_*cos(hue_)/cos(1.047196667-hue_));
  r_ = val_*(1.0+sat_*(1-cos(hue_)/cos(1.047196667-hue_)));
  g_ = val_*(1.0-sat_);
 }
 r = int( r_ );
 g = int( g_ );
 b = int( b_ );
 if( debug >0 ) {
   Serial.println(String::format("r: %d, g: %d, b: %d", r,g,b));
 }

}

#define DEG_TO_RAD(X) (M_PI*(X)/180)

void RGBMoodLifx::hsb2rgbw(uint16_t H, uint16_t S, uint16_t I, uint16_t& r, uint16_t& g, uint16_t& b, uint16_t& w) {

  float cos_h, cos_1047_h, H_, S_, I_, r_, g_, b_, w_;
  //H = fmod(H,360); // cycle H around to 0-360 degrees
  H_ = 3.14159*H/180.0; // Convert to radians.
  I_ = (float)I/MAX_VAL*MAX_PWM;  // Normalize and scale
  S_ = (float)S/MAX_VAL;
  //S = S>0?(S<1?S:1):0; // clamp S and I to interval [0,1]
  //I = I>0?(I<1?I:1):0;

  if(H_ < 2.09439) {
    cos_h = cos(H_);
    cos_1047_h = cos(1.047196667-H_);
    r_ = S_*I_/3.0*(1.0+cos_h/cos_1047_h);
    g_ = S_*I_/3.0*(1.0+(1.0-cos_h/cos_1047_h));
    b_ = 0;
    w_ = (1-S_)*I_;
  } else if(H_ < 4.188787) {
    H_ = H_ - 2.09439;
    cos_h = cos(H_);
    cos_1047_h = cos(1.047196667-H_);
    g_ = S_*I_/3.0*(1.0+cos_h/cos_1047_h);
    b_ = S_*I_/3.0*(1.0+(1.0-cos_h/cos_1047_h));
    r_ = 0.0;
    w_ = (1.0-S_)*I_;
  } else {
    H_ = H_ - 4.188787;
    cos_h = cos(H_);
    cos_1047_h = cos(1.047196667-H_);
    b_ = S_*I_/3.0*(1.0+cos_h/cos_1047_h);
    r_ = S_*I_/3.0*(1.0+(1.0-cos_h/cos_1047_h));
    g_ = 0.0;
    w_ = (1.0-S_)*I_;
  }
  r = int( r_ );
  g = int( g_ );
  b = int( b_ );
  w = int( w_ );
  if( debug >0 ) {
    Serial.println(String::format("r: %d, g: %d, b: %d, w: %d", r,g,b,w));
  }
}

/*  Private functions
------------------------------------------------------------ */

/*
This function is used internaly to do the fading between colors.
*/
void RGBMoodLifx::fade()
{
  if (fading_in_hsb_) {
    current_HSB_color_[0] = (uint16_t)(initial_color_[0] - (fading_step_*((initial_color_[0]-(float)target_color_[0])/fading_max_steps_)));
    current_HSB_color_[1] = (uint16_t)(initial_color_[1] - (fading_step_*((initial_color_[1]-(float)target_color_[1])/fading_max_steps_)));
    current_HSB_color_[2] = (uint16_t)(initial_color_[2] - (fading_step_*((initial_color_[2]-(float)target_color_[2])/fading_max_steps_)));
    switch ( type_ ) {
      case BULB_RGB:
        {
          hsb2rgb(current_HSB_color_[0], current_HSB_color_[1], current_HSB_color_[2], current_RGB_color_[0], current_RGB_color_[1], current_RGB_color_[2]);
        }
        break;
      case BULB_RGBW:
        {
          hsb2rgbw(current_HSB_color_[0], current_HSB_color_[1], current_HSB_color_[2], current_RGB_color_[0], current_RGB_color_[1], current_RGB_color_[2], current_RGB_color_[2]);
        }
        break;
      }
  }
  else {
    current_RGB_color_[0] = (uint16_t)(initial_color_[0] - (fading_step_*((initial_color_[0]-(float)target_color_[0])/fading_max_steps_)));
    current_RGB_color_[1] = (uint16_t)(initial_color_[1] - (fading_step_*((initial_color_[1]-(float)target_color_[1])/fading_max_steps_)));
    current_RGB_color_[2] = (uint16_t)(initial_color_[2] - (fading_step_*((initial_color_[2]-(float)target_color_[2])/fading_max_steps_)));
  }
}
