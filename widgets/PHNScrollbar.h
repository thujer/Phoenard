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

/**
 * @file
 * @brief Contains the PHN_Scrollbar widget; a widget for a bar that can be dragged
 */

#include "PHNWidget.h"

#ifndef _PHN_SCROLLBAR_H_
#define _PHN_SCROLLBAR_H_

/**
 * @brief Shows a scroll bar the user can drag
 *
 * Depending on the width and height a vertical or horizontal bar is used.
 * Set up the range before use.
 */
class PHN_Scrollbar : public PHN_Widget {
 public:
  /// Initializes the value to the pure minimum
  PHN_Scrollbar(void) : scrollPos(-32767) {}
  /// Sets the minimum and maximum scrolling range
  void setRange(int minValue, int maxValue);
  /// Sets the scrollbar value
  void setValue(int value);
  /// Gets the scrollbar value
  const int value(void) { return scrollPos; }
  /// Gets whether the value was changed since the last update
  const bool isValueChanged(void) { return valueChanged; }

  virtual void draw(void);
  virtual void update(void);
 private:
  void updateBar(bool redrawing);
  int scrollPos, scrollMin, scrollMax;
  int sliderPos;
  bool barWasPressed;
  bool navWasPressed;
  bool valueChanged;
};

#endif