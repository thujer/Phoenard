/*
The MIT License (MIT)

This file is part of the Phoenard Arduino library
Copyright (c) 2014 Phoenard

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "PHNDisplay.h"

static const uint8_t DIR_TRANSFORM[] = {DIR_RIGHT_WRAP_DOWN, DIR_DOWN_WRAP_DOWN,
                                          DIR_LEFT_WRAP_DOWN, DIR_UP_WRAP_DOWN,
                                          DIR_RIGHT_WRAP_UP, DIR_DOWN_WRAP_UP,
                                          DIR_LEFT_WRAP_UP, DIR_UP_WRAP_UP};

// ================ Functions used to transform x/y based on screen rotation==================

static void calcGRAMPosition_0(uint16_t *posx, uint16_t *posy) {
}
static void calcGRAMPosition_1(uint16_t *posx, uint16_t *posy) {
  swap(*posx, *posy);
  *posx = PHNDisplayHW::WIDTH - 1 - *posx;
}
static void calcGRAMPosition_1_inv(uint16_t *posx, uint16_t *posy) {
  *posx = PHNDisplayHW::WIDTH - 1 - *posx;
  swap(*posx, *posy);
}
static void calcGRAMPosition_2(uint16_t *posx, uint16_t *posy) {
  *posx = PHNDisplayHW::WIDTH - 1 - *posx;
  *posy = PHNDisplayHW::HEIGHT - 1 - *posy;
}
static void calcGRAMPosition_3(uint16_t *posx, uint16_t *posy) {
  swap(*posx, *posy);
  *posy = PHNDisplayHW::HEIGHT - 1 - *posy;
}
static void calcGRAMPosition_3_inv(uint16_t *posx, uint16_t *posy) {
  *posy = PHNDisplayHW::HEIGHT - 1 - *posy;
  swap(*posx, *posy);
}
// ===========================================================================================
                                          
// Initialize display here
PHN_Display display;

PHN_Display::PHN_Display() {
  /* LCD Initialization for first-time use */
  screenRot = 0;
  _scroll = 0;
  _width = PHNDisplayHW::WIDTH;
  _height = PHNDisplayHW::HEIGHT;
  _viewport.x = 0;
  _viewport.y = 0;
  _viewport.w = _width;
  _viewport.h = _height;
  textOpt.text_hasbg = false;
  textOpt.textcolor = WHITE;
  textOpt.textsize = 1;
  cgramFunc = calcGRAMPosition_0;
  cgramFunc_inv = calcGRAMPosition_0;
}

void PHN_Display::setSleeping(bool sleeping) {
  PHNDisplayHW::writeRegister(LCD_CMD_POW_CTRL1, sleeping ? 0x17B2 : 0x17B0);
}

void PHN_Display::setBacklight(int level) {
  if (level <= 0) {
    digitalWrite(TFTLCD_BL_PIN, LOW);
  } else if (level >= 256) {
    digitalWrite(TFTLCD_BL_PIN, HIGH);
  } else {
    analogWrite(TFTLCD_BL_PIN, level);
  }
}

uint16_t PHN_Display::width() {
  return _viewport.w;
}

uint16_t PHN_Display::height() {
  return _viewport.h;
}

bool PHN_Display::isWidescreen() {
  return _width > _height;
}

void PHN_Display::setScreenRotation(uint8_t rotation) {
  screenRot = rotation & 0x3;
  // Update the x/y transfer function
  switch (screenRot) {
  case 0:
    cgramFunc = calcGRAMPosition_0;
    cgramFunc_inv = calcGRAMPosition_0;
    break;
  case 1:
    cgramFunc = calcGRAMPosition_1;
    cgramFunc_inv = calcGRAMPosition_1_inv;
    break;
  case 2:
    cgramFunc = calcGRAMPosition_2;
    cgramFunc_inv = calcGRAMPosition_2;
    break;
  case 3:
    cgramFunc = calcGRAMPosition_3;
    cgramFunc_inv = calcGRAMPosition_3_inv;
    break;
  }
  // Update screen dimensions
  _width = PHNDisplayHW::WIDTH * (1-(screenRot & 0x1)) + PHNDisplayHW::HEIGHT * (screenRot & 0x1);
  _height = PHNDisplayHW::HEIGHT * (1-(screenRot & 0x1)) + PHNDisplayHW::WIDTH * (screenRot & 0x1);
  // Reset viewport (original bounds are no longer valid!)
  resetViewport();
}

uint8_t PHN_Display::getScreenRotation() {
  return screenRot & 0x3;
}

void PHN_Display::setWrapMode(uint8_t mode) {
  wrapMode = mode;
}

void PHN_Display::goTo(uint16_t x, uint16_t y) {
  goTo(x, y, 0xFF);
}

void PHN_Display::goTo(uint16_t x, uint16_t y, uint8_t direction) {
  // Apply screen rotation transform to x/y
  x += _viewport.x;
  y += _viewport.y;

  // Use the CGRAM transform function for the current screen rotation
  // Make use of a switch statement so compiler can optimize it
  switch (screenRot) {
  case 0:
    calcGRAMPosition_0(&x, &y);
    break;
  case 1:
    calcGRAMPosition_1(&x, &y);
    break;
  case 2:
    calcGRAMPosition_2(&x, &y);
    break;
  case 3:
    calcGRAMPosition_3(&x, &y);
    break;
  }

  // Write to the display
  direction++;
  if (direction) {
    direction--;
    direction += screenRot;
    direction &= 0x3;
    direction |= wrapMode;
    PHNDisplayHW::setCursor(x, y, DIR_TRANSFORM[direction]);
  } else {
    PHNDisplayHW::setCursor(x, y, DIR_RIGHT);
  }
}

void PHN_Display::resetViewport() {
  setViewport(0, 0, _width, _height);
}

void PHN_Display::setViewportRelative(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  setViewport(_viewport.x + x, _viewport.y + y, w, h);
}

void PHN_Display::setViewport(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  // Update viewport
  Viewport newViewport;
  newViewport.x = x;
  newViewport.y = y;
  newViewport.w = w;
  newViewport.h = h;
  setViewport(newViewport);
}

void PHN_Display::setViewport(Viewport viewport) {
  _viewport = viewport;
  // Start/end values to be set
  uint16_t x1 = viewport.x, y1 = viewport.y;
  uint16_t x2 = x1 + viewport.w - 1, y2 = y1 + viewport.h - 1;
  // Apply screen transforms
  cgramFunc(&x1, &y1);
  cgramFunc(&x2, &y2);
  // Apply to the display
  PHNDisplayHW::setViewport(x1, y1, x2, y2);
}

Viewport PHN_Display::getViewport() {
  return _viewport;
}

uint32_t PHN_Display::getViewportArea() {
  return (uint32_t) _viewport.w * (uint32_t) _viewport.h;
}

// draw a triangle!
void PHN_Display::drawTriangle(uint16_t x0, uint16_t y0,
        uint16_t x1, uint16_t y1,
        uint16_t x2, uint16_t y2, color_t color) {
  drawLine(x0, y0, x1, y1, color);
  drawLine(x1, y1, x2, y2, color);
  drawLine(x2, y2, x0, y0, color); 
}

void PHN_Display::fillTriangle ( int32_t x0, int32_t y0,
        int32_t x1, int32_t y1,
        int32_t x2, int32_t y2, color_t color) {
  if (y0 > y1) {
    swap(y0, y1); swap(x0, x1);
  }
  if (y1 > y2) {
    swap(y2, y1); swap(x2, x1);
  }
  if (y0 > y1) {
    swap(y0, y1); swap(x0, x1);
  }

  int32_t dx1, dx2, dx3; // Interpolation deltas
  int32_t sx1, sx2, sy; // Scanline co-ordinates

  sx2=(int32_t)x0 * (int32_t)1000; // Use fixed point math for x axis values
  sx1 = sx2;
  sy=y0;

  // Calculate interpolation deltas
  if (y1-y0 > 0) dx1=((x1-x0)*1000)/(y1-y0);
    else dx1=0;
  if (y2-y0 > 0) dx2=((x2-x0)*1000)/(y2-y0);
    else dx2=0;
  if (y2-y1 > 0) dx3=((x2-x1)*1000)/(y2-y1);
    else dx3=0;

  // Render scanlines (horizontal lines are the fastest rendering method)
  if (dx1 > dx2) {
    for(; sy<=y1; sy++, sx1+=dx2, sx2+=dx1) {
      drawHorizontalLine(sx1/1000, sy, (sx2-sx1)/1000, color);
    }
    sx2 = x1*1000;
    sy = y1;
    for(; sy<=y2; sy++, sx1+=dx2, sx2+=dx3) {
      drawHorizontalLine(sx1/1000, sy, (sx2-sx1)/1000, color);
    }
  } else {
    for(; sy<=y1; sy++, sx1+=dx1, sx2+=dx2) {
      drawHorizontalLine(sx1/1000, sy, (sx2-sx1)/1000, color);
    }
    sx1 = x1*1000;
    sy = y1;
    for(; sy<=y2; sy++, sx1+=dx3, sx2+=dx2) {
      drawHorizontalLine(sx1/1000, sy, (sx2-sx1)/1000, color);
    }
  }
}

// draw a rectangle
void PHN_Display::drawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, 
          color_t color) {
  // smarter version
  drawHorizontalLine(x, y, w, color);
  drawHorizontalLine(x, y+h-1, w, color);
  drawVerticalLine(x, y, h, color);
  drawVerticalLine(x+w-1, y, h, color);
}

// draw a rounded rectangle
void PHN_Display::drawRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t r,
         color_t color) {
  // smarter version
  drawHorizontalLine(x+r, y, w-2*r, color);
  drawHorizontalLine(x+r, y+h-1, w-2*r, color);
  drawVerticalLine(x, y+r, h-2*r, color);
  drawVerticalLine(x+w-1, y+r, h-2*r, color);
  // draw four corners
  drawCircleHelper(x+r, y+r, r, 1, color);
  drawCircleHelper(x+w-r-1, y+r, r, 2, color);
  drawCircleHelper(x+w-r-1, y+h-r-1, r, 4, color);
  drawCircleHelper(x+r, y+h-r-1, r, 8, color);
}

// fill a rounded rectangle
void PHN_Display::fillRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t r,
         color_t color) {
  // smarter version
  fillRect(x+r, y, w-2*r, h, color);

  // draw four corners
  fillCircleHelper(x+w-r-1, y+r, r, 1, h-2*r-1, color);
  fillCircleHelper(x+r, y+r, r, 2, h-2*r-1, color);
}

// fill a circle
void PHN_Display::fillCircle(uint16_t x0, uint16_t y0, uint16_t r, color_t color) {
  drawVerticalLine(x0, y0-r, 2*r+1, color);
  fillCircleHelper(x0, y0, r, 3, 0, color);
}

// fill circle with a border
void PHN_Display::fillBorderCircle(uint16_t x, uint16_t y, uint16_t r, color_t color, color_t borderColor) {
  fillCircle(x, y, r, color);
  drawCircle(x, y, r, borderColor);
}

// used to do circles and roundrects!
void PHN_Display::fillCircleHelper(uint16_t x0, uint16_t y0, uint16_t r, uint8_t cornername, uint16_t delta,
      color_t color) {

  int16_t f = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x = 0;
  int16_t y = r;
  int16_t x_len = 2*x+delta+1;
  int16_t y_len = 2*y+delta+1;
  uint8_t corner_a = cornername & 0x1;
  uint8_t corner_b = cornername & 0x2;
  
  while (x<y) {
    if (f >= 0) {
      y--;
      y_len -= 2;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    x_len += 2;
    ddF_x += 2;
    f += ddF_x;
    
    if (corner_a) {
      drawVerticalLine(x0+x, y0-y, y_len, color);
      drawVerticalLine(x0+y, y0-x, x_len, color);
    }
    if (corner_b) {
      drawVerticalLine(x0-x, y0-y, y_len, color);
      drawVerticalLine(x0-y, y0-x, x_len, color);
    }
  }
}


// draw a circle outline

void PHN_Display::drawCircle(uint16_t x0, uint16_t y0, uint16_t r, 
      color_t color) {
  drawPixel(x0, y0+r, color);
  drawPixel(x0, y0-r, color);
  drawPixel(x0+r, y0, color);
  drawPixel(x0-r, y0, color);

  drawCircleHelper(x0, y0, r, 0xF, color);
}

void PHN_Display::drawCircleHelper(uint16_t x0, uint16_t y0, uint16_t r, uint8_t cornername,
      color_t color) {
  int16_t f = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x = 0;
  int16_t y = r;
  uint8_t corner1 = cornername & 0x1;
  uint8_t corner2 = cornername & 0x2;
  uint8_t corner4 = cornername & 0x4;
  uint8_t corner8 = cornername & 0x8;
  uint8_t cornerAll = (cornername == 0xF);

  while (x<y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;

    if (cornerAll) {
      x0 += x;
      drawPixel(x0, y0 + y, color);
      drawPixel(x0, y0 - y, color);
      x0 -= x + x;
      drawPixel(x0, y0 + y, color);
      drawPixel(x0, y0 - y, color);
      x0 += x + y;
      drawPixel(x0, y0 + x, color);
      drawPixel(x0, y0 - x, color);
      x0 -= y + y;
      drawPixel(x0, y0 + x, color);
      drawPixel(x0, y0 - x, color);
      x0 += y;
    } else {
      if (corner4) {
        drawPixel(x0 + x, y0 + y, color);
        drawPixel(x0 + y, y0 + x, color);
      } 
      if (corner2) {
        drawPixel(x0 + x, y0 - y, color);
        drawPixel(x0 + y, y0 - x, color);
      }
      if (corner8) {
        drawPixel(x0 - y, y0 + x, color);
        drawPixel(x0 - x, y0 + y, color);
      }
      if (corner1) {
        drawPixel(x0 - y, y0 - x, color);
        drawPixel(x0 - x, y0 - y, color);
      }
    }
  }
}

// fill a rectangle
void PHN_Display::fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, 
          color_t fillcolor) {

  if (w>>4 && h>>4) {
    // Use viewport for large surface areas
    Viewport oldViewport = getViewport();
    setViewportRelative(x, y, w, h);
    fill(fillcolor);
    setViewport(oldViewport);
  } else if (h >= w) {
    // Draw vertical lines when height > width
    for (uint16_t wx = 0; wx < w; wx++) {
      drawVerticalLine(x+wx, y, h, fillcolor);
    }
  } else {
    // Draw horizontal lines otherwise
    for (uint16_t wy = 0; wy < h; wy++) {
      drawHorizontalLine(x, y+wy, w, fillcolor);
    }
  }
}

// fill a rectangle and draw a border
void PHN_Display::fillBorderRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, color_t color, color_t borderColor) {
  drawRect(x, y, w, h, borderColor);
  fillRect(x+1, y+1, w-2, h-2, color);
}

// fill a rounded rectangle and draw a border
void PHN_Display::fillBorderRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t radius, color_t color, color_t borderColor) {
  fillRoundRect(x, y, w, h, radius, color);
  drawRoundRect(x, y, w, h, radius, borderColor);
}

void PHN_Display::drawVerticalLine(uint16_t x, uint16_t y, uint16_t length, color_t color) {
  drawStraightLine(x, y, length, 1, color);
}

void PHN_Display::drawHorizontalLine(uint16_t x, uint16_t y, uint16_t length, color_t color) {
  drawStraightLine(x, y, length, 0, color);
}

void PHN_Display::drawStraightLine(uint16_t x, uint16_t y, uint16_t length, uint8_t direction, color_t color) {
  goTo(x, y, direction);
  PHNDisplay16Bit::writePixels(color, length);
}

// Make use of Bresenham's algorithm with straight-line boosts and minimal flash
void PHN_Display::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, color_t color) {
  int16_t dx, dy;
  dx = x1 - x0;
  dy = y1 - y0;
  int16_t adx, ady;
  adx = abs(dx);
  ady = abs(dy);

  if (x0 == x1) {
    drawVerticalLine(x0, min(y0, y1), ady, color);
    return;
  }

  bool steep = ady > adx;
  if (steep) {
    swap(x0, y0);
    swap(x1, y1);
    swap(adx, ady);
  }
  if (x0 > x1) {
    swap(x0, x1);
    swap(y0, y1);
  }

  if (y0 == y1) {
    drawHorizontalLine(x0, y0, adx, color);
    return;
  }

  int16_t err = adx;
  int8_t ystep = (y0 < y1) ? 1 : -1;

  adx += adx;
  ady += ady;
  if (steep) {
    for (; x0<=x1; x0++) {
      drawPixel(y0, x0, color);
      err -= ady;
      if (err < 0) {
        y0 += ystep;
        err += adx;
      }
    }
  } else {
    for (; x0<=x1; x0++) {
      drawPixel(x0, y0, color);
      err -= ady;
      if (err < 0) {
        y0 += ystep;
        err += adx;
      }
    }
  }
}

void PHN_Display::drawLineAngle(int x, int y, int r1, int r2, float angle, color_t color) {
  float a_sin = sin(angle);
  float a_cos = cos(angle);
  display.drawLine(x + r1 * a_cos, y + r1 * a_sin, x + r2 * a_cos, y + r2 * a_sin, color);
}

void PHN_Display::fill(color_t color) {
  goTo(0, 0, 0);
  PHNDisplay16Bit::writePixels(color, getViewportArea());
}

void PHN_Display::drawPixel(uint16_t x, uint16_t y, color_t color) {
  goTo(x, y);
  PHNDisplay16Bit::writePixel(color);
}

// ======================== Slider touch ===========================

float PHN_Display::getSlider() {
  return sliderLive;
}
float PHN_Display::getSliderStart() {
  return sliderStart;
}

bool PHN_Display::isSliderTouched() {
  return sliderDown;
}
bool PHN_Display::isSliderTouchDown() {
  return sliderDown && !sliderWasDown;
}
bool PHN_Display::isSliderTouchUp() {
  return !sliderDown && sliderWasDown;
}

// ======================== Display touch ==========================

PressPoint PHN_Display::getTouch() {
  return touchLive;
}
PressPoint PHN_Display::getTouchStart() {
  return touchStart;
}

PressPoint PHN_Display::getTouchLast() {
  return touchLast;
}

bool PHN_Display::isTouched() {
  return touchLive.pressure;
}
bool PHN_Display::isTouched(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
  return touchLive.isPressed(x, y, width, height);
}

bool PHN_Display::isTouchDown() {
  return !touchLast.pressure && touchLive.pressure;
}
bool PHN_Display::isTouchEnter(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
  // Now pressed down and it wasn't previously?
  return touchLive.isPressed(x, y, width, height) && !touchLast.isPressed(x, y, width, height);
}

bool PHN_Display::isTouchUp() {
  return touchLast.pressure && !touchLive.pressure;
}
bool PHN_Display::isTouchLeave(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
  // Was touched down before but is now no longer?
  return touchLast.isPressed(x, y, width, height) && !touchLive.isPressed(x, y, width, height);
}

bool PHN_Display::isTouchClicked(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
  // Area was previously touched but the touch is now released?
  return touchClicked && touchLast.isPressed(x, y, width, height);
}

bool PHN_Display::isTouchChange(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
  bool touched_old = touchLast.isPressed(x, y, width, height);
  bool touched_new = touchLive.isPressed(x, y, width, height);
  return (touchClicked && touched_old) || (touched_old != touched_new);
}

void PHN_Display::update() {
  updateTouch();
  PHN_WidgetContainer::updateWidgets(true, true, false);
}

void PHN_Display::updateWidgets() {
  touchStart.pressure = 0.0F;
  touchLive.pressure = 0.0F;
  touchLast.pressure = 0.0F;
  touchInput = false;
  touchInputLive = false;
  touchInputSlider = false;
  touchClicked = false;
  sliderDown = false;
  sliderWasDown = false;
  PHN_WidgetContainer::updateWidgets(true, true, false);
}

void PHN_Display::invalidate(void) {
  int cnt = widgetCount();
  while (cnt--) {
    widget(cnt)->invalidate();
  }
}

void PHN_Display::updateTouch() {
  uint16_t touch_x, touch_y;

  // Store the previous state
  touchLast = touchLive;

  // Read the touch input
  PHNDisplayHW::readTouch(&touch_x, &touch_y, &touchLive.pressure);

  // Update live touched down state
  if (touchLive.pressure >= PHNDisplayHW::PRESSURE_THRESHOLD) {
    // Touch input received
    touchInputLive = true;

    // Pressed inside the screen, or pressed the slider?
    touchInputSlider = (touch_x >= PHNDisplayHW::WIDTH);
    
    if (touchInputSlider) {
      // Update slider value
      sliderLive = (float) touch_y / (float) PHNDisplayHW::HEIGHT;

      // Should reach full 0 - 1 easily, this ensures it
      sliderLive = 1.1*sliderLive-0.05;
      if (sliderLive > 1.0)
        sliderLive = 1.0;
      else if (sliderLive < 0.0)
        sliderLive = 0.0;

      // Reverse the slider for some of the rotation angles
      if (getScreenRotation() == 0 || getScreenRotation() == 1)
        sliderLive = 1.0 - sliderLive;
    } else {
      // Transform the touched x/z values to display space
      cgramFunc_inv(&touch_x, &touch_y);

      // Apply a smoothing if touched previously
      if (touchLast.isPressed()) {
        // Calculate the change in touch value
        int dx = ((int) touch_x - (int) touchLive.x);
        int dy = ((int) touch_y - (int) touchLive.y);

        // Apply a magic formula to smoothen changes
        touchLive.x += TFTLCD_TOUCH_SMOOTH(dx);
        touchLive.y += TFTLCD_TOUCH_SMOOTH(dy);
      } else {
        // Set it instantly
        touchLive.x = touch_x;
        touchLive.y = touch_y;
      }
    }
  } else if (!touchLive.isPressed()) {
    // No longer pressed when pressure reaches 0
    touchInputLive = false;
  }

  // When unchanging for delay, assume correct state
  if (touchInputLive == touchInput) {
    touchInputLastStable = millis();
  } else if ((millis() - touchInputLastStable) > TFTLCD_TOUCH_PRESSDELAY) {
    touchInputLastStable = millis();
    touchInput = touchInputLive;
  }

  // Update API state
  sliderWasDown = sliderDown;
  sliderDown = touchInput && touchInputSlider;
  if (!touchInput || touchInputSlider) {
    touchLive.pressure = 0.0F;
  }

  // Further point updates on state changes
  if (touchLive.isPressed() && !touchLast.isPressed()) {
    touchStart = touchLive;
  }
  if (sliderDown && !sliderWasDown) {
    sliderStart = sliderLive;
  }
  
  // If press was released, mark it with a flag just this time
  if (!touchLive.isPressed() && touchLast.isPressed()) {
     // If clicked already handled, undo it and proceed with normal leave logical_and
     if (touchClicked) {
       touchClicked = false;
     } else {
       touchClicked = true;
       // Clicking: restore the 'last' state for next time
       touchLive = touchLast;
     }
  } else {
    touchClicked = false;
  }
}

/* =========================== TEXT DRAWING ============================== */

void PHN_Display::setCursorDown(uint16_t x) {
  setCursor(x, textOpt.cursor_y + textOpt.textsize*8);
}

void PHN_Display::setCursor(uint16_t x, uint16_t y) {
  textOpt.cursor_x = x;
  textOpt.cursor_x_start = x;
  textOpt.cursor_y = y;
}

void PHN_Display::setTextSize(uint8_t s) {
  textOpt.textsize = s;
}

void PHN_Display::setTextColor(color_t c) {
  textOpt.textcolor = c;
  textOpt.text_hasbg = false;
}

void PHN_Display::setTextColor(color_t c, color_t bg) {
  textOpt.textcolor = c;
  textOpt.textbg = bg;
  textOpt.text_hasbg = true;
}

void PHN_Display::setScroll(int value) {
  value %= 320;
  if (value < 0) {
    value += 320;
  }
  _scroll = value;
  PHNDisplayHW::writeRegister(LCD_CMD_GATE_SCAN_CTRL3, _scroll);
}

size_t PHN_Display::write(uint8_t c) {
  if (c == '\n') {
    textOpt.cursor_y += textOpt.textsize*8;
    textOpt.cursor_x = textOpt.cursor_x_start;
  } else if (c == '\r') {
    // skip em
  } else {
    drawChar(textOpt.cursor_x, textOpt.cursor_y, c, textOpt.textsize);
    textOpt.cursor_x += textOpt.textsize*6;
  }
  return 0;
}

void PHN_Display::printShortTime(Date date) {
  if (date.hour < 10)
    print('0');
  print(date.hour);
  print(':');
  if (date.minute < 10)
    print('0');
  print(date.minute);
}

void PHN_Display::printTime(Date date) {
  printShortTime(date);
  print(':');
  if (date.second < 10)
    print('0');
  print(date.second);
}

void PHN_Display::printDate(Date date) {
  if (date.day < 10)
    print('0');
  print(date.day);
  print('/');
  if (date.month < 10)
    print('0');
  print(date.month);
  print('/');
  if (date.year < 10)
    print('0');
  print(date.year);
}

void PHN_Display::printPadding(int nrChars) {
  nrChars -= (textOpt.cursor_x - textOpt.cursor_x_start) / (6 * textOpt.textsize);
  while (nrChars > 0) {
    print(' ');
    nrChars--;
  }
}

void PHN_Display::printMem(const uint8_t* font_char) {
  drawCharMem(textOpt.cursor_x, textOpt.cursor_y, font_char, textOpt.textsize);
  write(' ');
}

TextBounds PHN_Display::computeMiddleBounds(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const char* text) {
  TextBounds bounds;

  // Calculate the total amount of rows and columns the text displays
  unsigned int textWidth = 0;
  unsigned int textWidth_tmp = 0;
  unsigned int textHeight = 8;
  unsigned int textSize = 2;
  const char* text_p = text;
  while (*text_p) {
    if (*text_p == '\n') {
      textHeight += 8;
      textWidth_tmp = 0;
    } else {
      textWidth_tmp += 6;
      if (textWidth_tmp > textWidth) {
        textWidth = textWidth_tmp;
      }
    }
    text_p++;
  }

  // Find an appropriate size to fit the rectangle
  while ((textWidth*textSize+2) < width && (textHeight*textSize+2) < height) {
    textSize++;
  }
  textSize--;

  // Find the bounds of the piece of text at this size
  bounds.size = textSize;
  bounds.w = textWidth * textSize;
  bounds.h = textHeight * textSize;
  bounds.x = x + ((width - bounds.w) >> 1);
  bounds.y = y + ((height - bounds.h) >> 1);

  return bounds;
}

void PHN_Display::drawStringMiddle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const char* text) {
  TextBounds bounds = computeMiddleBounds(x, y, width, height, text);
  setTextSize(bounds.size);
  setCursor(bounds.x, bounds.y);
  print(text);
}

void PHN_Display::drawString(uint16_t x, uint16_t y, const char *c, uint8_t size) {
  uint16_t c_x = x;
  uint16_t c_y = y;
  while (*c) {
    if (*c == '\n') {
      c_x = x;
      c_y += size*8;
    } else {
      drawChar(c_x, c_y, *c, size);
      c_x += size*6;
    }
    c++;
  }
}

// draw a character
void PHN_Display::drawChar(uint16_t x, uint16_t y, char c, uint8_t size) {
  drawCharMem(x, y, phn_font_5x7+(c*5), size);
}

void PHN_Display::drawCharMem(uint16_t x, uint16_t y, const uint8_t* font_char, uint8_t size) {
  // Read character data from FLASH into memory
  uint8_t c[5];
  memcpy_P(c, (void*) font_char, 5);
  drawCharRAM(x, y, c, size);
}

void PHN_Display::drawCharRAM(uint16_t x, uint16_t y, const uint8_t* font_data, uint8_t size) {
  // If transparent background, make use of a (slower) cube drawing algorithm
  // For non-transparent backgrounds, make use of the faster 1-bit image drawing function
  if (textOpt.text_hasbg) {
    // Use a scale-based drawing function which is quite a bit faster
    // This is almost equivalent to the below function, except we don't handle viewport/rotation in there
    // It had to be copied over, making use of goTo instead of the internal setCursor.
    //PHNDisplay16Bit::writeFont_1bit(x, y, size, font_char, textOpt.textbg, textOpt.textcolor);

    // Draw the read pixel data
    const uint8_t width = 8;
    const uint8_t height = 5;
    uint8_t pix_dat = 0, dy, dx, si = 0;
    uint8_t l = height * size;
    for (dy = 0; dy < l; dy++) {
      goTo(x, y, 1);
      x++;

      pix_dat = *font_data;
      for (dx = 0; dx < width; dx++) {
        /* Draw SCALE pixels for each pixel in a line */
        PHNDisplay16Bit::writePixels((pix_dat & 0x1) ? textOpt.textcolor : textOpt.textbg, size);
        /* Next pixel data bit */
        pix_dat >>= 1;
      }

      // Read next byte as needed
      if (++si >= size) {
        si = 0;
        font_data++;
      }
    }
  } else {
    // Draw in vertical 'dot' chunks for each 5 columns
    // Empty (0) data 'blocks' are skipped leaving them 'transparent'
    uint16_t cx, cy;
    uint16_t cx_end = x+size*4;
    uint16_t pcount = 0;
    uint8_t line = *font_data;
    color_t c = textOpt.textcolor;
    cx = x;
    cy = y;
    for (;;) {
      if (line & 0x1) {
        do {
          line >>= 1;
          pcount += size;
        } while (line & 0x1);
        fillRect(cx, cy, size, pcount, c);
        cy += pcount;
        pcount = 0;
      } else if (line) {
        line >>= 1;
        cy += size;
      } else if (cx == cx_end) {
        break;
      } else {
        line = *(++font_data);
        cx += size;
        cy = y;
      }
    }
  }
}

void PHN_Display::debugPrint(uint16_t x, uint16_t y, uint8_t size, const char* text, uint8_t padding) {
  // Store old options
  TextOptions oldTextOpt = textOpt;

  // Set draw parameters
  setCursor(x, y);
  setTextSize(size);
  setTextColor(WHITE, BLACK);

  // Draw the text
  print(text);  

  // Pad the end with spaces
  uint8_t len = strlen(text);
  while (len < padding) {
    print(' ');
    len++;
  }

  // Restore text options
  textOpt = oldTextOpt;
}

void PHN_Display::debugPrint(uint16_t x, uint16_t y, uint8_t size, int value) {
  char buff[7];
  itoa(value, buff, 10);
  debugPrint(x, y, size, buff, 6);
}

void PHN_Display::debugPrint(uint16_t x, uint16_t y, uint8_t size, float value) {
  char buff[7];
  dtostrf(value, 4, 2, buff);
  debugPrint(x, y, size, buff, 6);
}

static float ImgMultColor_r, ImgMultColor_g, ImgMultColor_b;
static void ImgMultColor(uint8_t *r, uint8_t *g, uint8_t *b) {
  *r = (uint8_t) min((float) (*r) * ImgMultColor_r, 255);
  *g = (uint8_t) min((float) (*g) * ImgMultColor_g, 255);
  *b = (uint8_t) min((float) (*b) * ImgMultColor_b, 255);
}

void PHN_Display::drawImage(Stream &imageStream, int x, int y) {
  drawImageMain(imageStream, x, y, NULL, NULL);
}

void PHN_Display::drawImage(Stream &imageStream, int x, int y, float brightness) {
  drawImage(imageStream, x, y, brightness, brightness, brightness);
}

void PHN_Display::drawImage(Stream &imageStream, int x, int y, float cr, float cg, float cb) {
  // Set color component factors, then draw using the ImgMultColor color transform function
  ImgMultColor_r = cr; ImgMultColor_g = cg; ImgMultColor_b = cb;
  drawImageMain(imageStream, x, y, ImgMultColor, NULL);
}
void PHN_Display::drawImage(Stream &imageStream, int x, int y, void (*color)(uint8_t*, uint8_t*, uint8_t*)) {
  drawImageMain(imageStream, x, y, color, NULL);
}

void PHN_Display::drawImage(Stream &imageStream, int x, int y, const color_t *colorMapInput) {
  drawImageMain(imageStream, x, y, NULL, colorMapInput);
}

void PHN_Display::drawImageMain(Stream &imageStream, int x, int y, void (*color)(uint8_t*, uint8_t*, uint8_t*), const color_t *colorMapInput) {
  // Store old viewport for later restoring
  Viewport oldViewport = getViewport();

  char idChar = imageStream.read();
  if (idChar == 'B') {
    if (imageStream.read() == 'M') {
      // Bitmap
      // read the BITMAP file header
      Imageheader_BMP header;
      imageStream.readBytes((char*) &header, sizeof(header));
      if (!header.biClrUsed) header.biClrUsed = (1 << header.bitCount);
  
      // Read the pixel map
      color_t colorMap[header.biClrUsed];
      Color_ARGB argb;
      uint16_t ci;
      for (ci = 0; ci < header.biClrUsed; ci++) {
        imageStream.readBytes((char*) &argb, sizeof(argb));
        if (colorMapInput) {
          colorMap[ci] = colorMapInput[ci];
        } else {
          if (color) {
            color(&argb.r, &argb.g, &argb.b);
          }
          colorMap[ci] = PHNDisplayHW::color565(argb.r, argb.g, argb.b);
        }
      }

      // Skip ahead towards the pixel map (this is usually never needed)
      uint32_t pos = sizeof(header) + 2 + (header.biClrUsed * 4);
      for (; pos < header.pixelDataOffset; pos++) {
        imageStream.read();
      }

      // Set up the viewport to render the bitmap in
      setViewportRelative(x, y, header.width, header.height);

      // Go to the bottom-left pixel, drawing UPWARDS (!)
      setWrapMode(WRAPMODE_UP);
      goTo(0, header.height - 1, 0);

      // Used variables and calculated constants
      uint16_t px, py;
      uint16_t imageWidthSize = ((uint32_t) header.bitCount * header.width) >> 3;
      uint8_t imageWidthPadding = (4 - (imageWidthSize & 0x3)) & 0x3;

      // Process each row of pixels
      for (py = 0; py < header.height; py++) {
        // Draw visible Bitmap portion
        if (header.bitCount == 8) {
          for (px = 0; px < header.width; px++) {
            PHNDisplay16Bit::writePixel(colorMap[imageStream.read() & 0xFF]);
          }
        } else if (header.bitCount == 24) {
          // Color transform used?
          for (px = 0; px < header.width; px++) {
            imageStream.readBytes((char*) &argb, 3);
            if (color) color(&argb.r, &argb.g, &argb.b);
            PHNDisplay16Bit::writePixel(PHNDisplayHW::color565(argb.r, argb.g, argb.b));
          }
        }

        // Handle padding
        for (px = 0; px < imageWidthPadding; px++) {
          imageStream.read();
        }
      }
    }
  } else if (idChar == 'L') { 
    if (imageStream.read() == 'C' && imageStream.read() == 'D') {
      // LCD format
      // Read LCD headers
      Imageheader_LCD header;
      imageStream.readBytes((char*) &header, sizeof(header));
      uint32_t imagePixels = (uint32_t) header.width * (uint32_t) header.height;

      // Read colormap, empty if not used/available
      color_t colorMap[header.colors];
      uint16_t ci;
      color_t c565;
      uint8_t r, g, b;
      for (ci = 0; ci < header.colors; ci++) {
        imageStream.readBytes((char*) &c565, sizeof(c565));
        if (colorMapInput) {
          colorMap[ci] = colorMapInput[ci];
        } else if (color) {
          r = PHNDisplayHW::color565Red(c565);
          g = PHNDisplayHW::color565Green(c565);
          b = PHNDisplayHW::color565Blue(c565);
          color(&r, &g, &b);
          colorMap[ci] = PHNDisplayHW::color565(r, g, b);
        } else {
          colorMap[ci] = c565;
        }
      }

      // Set up the viewport to render the image in
      setViewportRelative(x, y, header.width, header.height);

      // Go to the top-left pixel, drawing DOWNWARDS (!)
      setWrapMode(WRAPMODE_DOWN);
      goTo(0, 0, 0);

      // Write pixels to screen
      if (header.bpp == 16) {
        // Fast method
        uint32_t i;
        for (i = 0; i < imagePixels; i++) {
          imageStream.readBytes((char*) &c565, sizeof(c565));
          if (color) {
            r = PHNDisplayHW::color565Red(c565);
            g = PHNDisplayHW::color565Green(c565);
            b = PHNDisplayHW::color565Blue(c565);
            color(&r, &g, &b);
            c565 = PHNDisplayHW::color565(r, g, b);
          }
          PHNDisplay16Bit::writePixel(c565);
        }
      } else {
        int tmpbuff = 0;
        int tmpbuff_len = 0;
        int pixelmask = (1 << header.bpp) - 1;
        uint32_t i;
        for (i = 0; i < imagePixels; i++) {
          if (!tmpbuff_len) {
            tmpbuff = (imageStream.read() & 0xFF);
            tmpbuff_len = 8;
          }
          PHNDisplay16Bit::writePixel(colorMap[tmpbuff & pixelmask]);
          tmpbuff >>= header.bpp;
          tmpbuff_len -= header.bpp;
        }
      }
    }
  }
  // Restore viewport
  setViewport(oldViewport);
}