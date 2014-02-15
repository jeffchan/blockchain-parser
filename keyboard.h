#ifndef KEYBOARD_H

#define KEYBOARD_H

#include "stdint.h"

bool isKeyDown(void); // returns true if a key has been pressed.
int  getKey(void); 	  // returns a key
const char **getInputString(uint32_t &argc);

#endif
