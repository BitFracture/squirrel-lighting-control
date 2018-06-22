
#ifndef __HELPERS
#define __HELPERS

/**
 * Returns the remainder of a division between numerator and denominator. 
 */
float easyFMod(float numerator, float denominator) {
  while (numerator < 0)
    numerator += denominator;
  while (numerator >= denominator)
    numerator -= denominator;

  return numerator;
}

/**
 * Locks the value of a float between two bounds.
 */
float clamp(float value, float lower, float upper) {

  if (value < lower)
    value = lower;
  if (value > upper)
    value = upper;
  return value;
}

/**
 * Note:
 * The hsvToRgb functions was copied from "https://github.com/ratkins/RGBConverter/"
 * with a free to use license.
 *
 * Converts an HSV color value to RGB. Conversion formula
 * adapted from http://en.wikipedia.org/wiki/HSV_color_space.
 * Assumes h, s, and v are contained in the set [0, 1] and
 * returns r, g, and b in the set [0, 255].
 *
 * @param   Number  h       The hue
 * @param   Number  s       The saturation
 * @param   Number  v       The value
 * @return  Array           The RGB representation
 */
void hsvToRgb(float _h, float _s, float _v, uint8_t& _r, uint8_t& _g, uint8_t& _b) {
    float h = easyFMod(_h / 360.0f, 1.0f);
    float s = clamp(_s / 100.0f, 0.0f, 1.0f);
    float v = clamp(_v / 100.0f, 0.0f, 1.0f);
    
    float r, g, b;

    int i = int(h * 6);
    float f = h * 6 - i;
    float p = v * (1 - s);
    float q = v * (1 - f * s);
    float t = v * (1 - (1 - f) * s);

    switch(i % 6){
        case 0: r = v, g = t, b = p; break;
        case 1: r = q, g = v, b = p; break;
        case 2: r = p, g = v, b = t; break;
        case 3: r = p, g = q, b = v; break;
        case 4: r = t, g = p, b = v; break;
        case 5: r = v, g = p, b = q; break;
    }

    _r = r * 255;
    _g = g * 255;
    _b = b * 255;
}

#endif
