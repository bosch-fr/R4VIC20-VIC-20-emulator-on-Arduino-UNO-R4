#include <Arduino.h>

#ifndef KEYMAP_H
#define KEYMAP_H

#define BREAK   0xF0   // Release prefix scancode
#define DTA_PIN 2      // Data pin
#define CLK_PIN 3      // Clock pin (must be interrupt-capable)

// Reserved PETSCII code for F1 F2 F3 F4
#define KEY_F1  0xFE
#define KEY_F2  0xFD
#define KEY_F3  0xFB
#define KEY_F4  0xFA
// Reserved PETSCII code for RUN/STOP (emitted when Caps Lock is pressed)
#define KEY_RUNSTOP 0xFC

void kb_init();
void on_keypressed(uint8_t code);

#endif