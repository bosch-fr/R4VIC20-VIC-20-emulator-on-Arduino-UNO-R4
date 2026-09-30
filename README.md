# R4VIC20 — VIC-20 emulator on Arduino UNO R4

A working Commodore VIC-20 emulator that fits on a single Arduino UNO R4
Minima/WiFi, drives a 2.42" SSD1309 OLED, reads a real PS/2 keyboard,
plays sound through the onboard DAC, and stores BASIC programs in the
MCU's internal EEPROM.

No Raspberry Pi. No Linux. No SBC. Just a Cortex-M4 at 48 MHz running a
6502 core, a PS/2 driver, a filesystem, a video renderer, and a wavetable
synth — at the same time.

<img width="800" height="600" alt="a" src="https://github.com/user-attachments/assets/2332debd-3dd4-4c2f-825d-984ea242c9cf" />

# What works

- Commodore VIC-20 KERNAL + BASIC booting to `READY.`
- 22×9 character display on the 128×64 OLED, PETSCII glyphs decoded
  from the original character ROM, reverse video, cursor blink, and
  a viewport that follows the KERNAL cursor.
- Display is absolutely stable and crisp with 2.42" OLED.
- PS/2 keyboard input with PETSCII translation and modifier handling.
- Esc acts as CLEAR SCREEN.
- Caps Lock acts as RUN/STOP
  (aborts the running BASIC statement, returns to READY).
- DAC audio synthesised on the RA4M1's internal 12-bit DAC, driven by
  a GPT timer ISR at 8 kHz. Three tone voices plus noise, mapped to the
  VIC-I sound registers at `$900A`–`$900E`.
- EEPROM "disk" with named slots. From BASIC:
  - `SAVE"NAME"` — store current program
  - `LOAD"NAME"` — load a stored program
  - `LOAD"@"` — directory listing
  - `SAVE"#NAME"` — delete a slot
  - `SAVE"!FORMAT"` — wipe all slots
- F1 shows a help banner. F2 loads a bundled METEOR example.
- F3 loads a bundled CHESS example. F4 loads a bundled CASTLE example.

<img width="1200" height="450" alt="d" src="https://github.com/user-attachments/assets/96ce3b5c-f408-461d-8a5e-5787477812c3" />

# What doesn't work

- No VIC-I chip emulation — no sprites, no color, no raster IRQ
- The screen window is 22×9, not the VIC-20's 22×23. Software that uses
  the full screen will render partially, just use the arrows to scroll up/down.

# Hardware

<img width="908" height="600" alt="b" src="https://github.com/user-attachments/assets/d0388ae6-ff85-41b8-adac-37cb973f7342" />

- Extremely simple : Arduino UNO R4, 2.42" SPI OLED, passive buzzer, PS/2 keyboard, some wires.
  No soldering !
- I did use an Arduino UNO shield for conveniance, that's optional.
- I did use a small/cheap USB keyboard and build an USBtoPS/2 adapter, that's optional :
	https://github.com/No0ne/ps2x2pico

# Wiring

<img width="800" height="640" alt="c" src="https://github.com/user-attachments/assets/f1b68627-f034-4a45-8e1d-1da33b0d6cab" />

# Limitations worth stating up front

- This is a demonstration emulator, not a general-purpose VIC-20
  replacement. It boots BASIC, runs BASIC, and does the audio. It does
  not emulate the VIC-I video chip, the VIA interface chips, or the
  raster interrupt, which means most commercial software will not run.
  It is a project about fitting an old computer onto a small
  microcontroller, not a project about running old software perfectly.

# note:
You can also take a look at my other project, a working Commodore VIC-20
emulator that fits on a single Arduino UNO R3, with composite output,
reads a real PS/2 keyboard, plays sound, and stores BASIC programs in the
external EEPROM.

# Acknowledgements

- Author : Christian Bosch - 2026 - "Lets be honest, AI did most of the job !"
- The main code is derived from Jan Ostman Published February 3, 2014
	https://www.hackster.io/janost/the-nano-vic-20-e37b39
	https://github.com/mganthon/nanoVIC-20
- The PS/2 driver is derived from a Commodore 64 emulator sketch by Doctor Volt
	https://github.com/michalin/Arduino-C64-Emulator
- U8g2 by olikraus.
	https://github.com/olikraus/u8g2  
  
# License

- If you use/modify/whatever this code, just give me some credits, and give this github link.
  

