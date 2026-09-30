// r4_vic20.ino — VIC-20 emulator on Arduino UNO R4 + 2.42" SSD1309 OLED
// Named slots with DIR and SCRATCH, F1 help, DAC sound, PETSCII graphics

#include <Arduino.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <EEPROM.h>
#include <FspTimer.h>
#include "meteor.h"
#include "chess.h"
#include "castle.h"

#include "char_rom.h"
#include "char_rom_lower.h"
#include "keyboard.h"

#define OLED_CS   10
#define OLED_DC    7
#define OLED_RST   6

U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI u8g2(U8G2_R0, OLED_CS, OLED_DC, OLED_RST);

uint8_t RAM[0x1E00];
uint8_t videomem[512];
volatile uint8_t screen_dirty = 1;
volatile uint8_t help_mode = 0;
volatile uint8_t runstop_pending = 0;

uint8_t colorram[512];
volatile uint8_t color_dirty = 0;

// ---------- EEPROM layout ----------
#define DIR_ENTRIES   8
#define DIR_ENTRY_SZ  16
#define DIR_BYTES     (DIR_ENTRIES * DIR_ENTRY_SZ)
#define DATA_START    DIR_BYTES
#define EEPROM_SIZE   8192

// ---------- Mailbox ----------
#define MAILBOX      0x02A1
#define MB_NAMELEN   (MAILBOX + 0)
#define MB_NAMEPTR_L (MAILBOX + 1)
#define MB_NAMEPTR_H (MAILBOX + 2)
#define MB_START_L   (MAILBOX + 3)
#define MB_START_H   (MAILBOX + 4)
#define MB_END_L     (MAILBOX + 5)
#define MB_END_H     (MAILBOX + 6)
#define MB_OP        (MAILBOX + 7)

#define MB_OP_SAVE   0xA5
#define MB_OP_LOAD   0x5A

extern "C" {
  uint16_t get_pc(void);
  void     set_pc(uint16_t v);
  uint8_t  get_sp(void);
  void     set_sp(uint8_t v);
  uint8_t  get_a(void);
  void     set_a(uint8_t v);
  uint8_t  get_x(void);
  void     set_x(uint8_t v);
  uint8_t  get_y(void);
  void     set_y(uint8_t v);
  uint8_t  get_status(void);
  void     set_status(uint8_t v);
  void exec6502(void);
  void reset6502(void);
  uint8_t read6502(uint16_t address);
  void    write6502(uint16_t address, uint8_t value);
}

extern const unsigned char BIOS[16384];

const uint8_t screen_to_ascii[128] PROGMEM = {
  '@','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O',
  'P','Q','R','S','T','U','V','W','X','Y','Z','[','\\',']','^','_',
  ' ','!','"','#','$','%','&','\'','(',')','*','+',',','-','.','/',
  '0','1','2','3','4','5','6','7','8','9',':',';','<','=','>','?',
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
  0x20,0x20,0x20,0x20,0x20,0x20,'_',0x20,
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
};

// ---------- 6502 SAVE handler at $033C ----------
const uint8_t save_handler[] PROGMEM = {
  0xA5, 0xB7, 0x8D, 0xA1, 0x02,
  0xA5, 0xBB, 0x8D, 0xA2, 0x02,
  0xA5, 0xBC, 0x8D, 0xA3, 0x02,
  0xA5, 0xC3, 0x8D, 0xA4, 0x02,
  0xA5, 0xC4, 0x8D, 0xA5, 0x02,
  0xA5, 0xC1, 0x8D, 0xA6, 0x02,
  0xA5, 0xC2, 0x8D, 0xA7, 0x02,
  0xA9, 0xA5, 0x8D, 0xA8, 0x02,
  0x18, 0x60
};

// ---------- 6502 LOAD handler at $0378 ----------
const uint8_t load_handler[] PROGMEM = {
  0xA5, 0xB7, 0x8D, 0xA1, 0x02,
  0xA5, 0xBB, 0x8D, 0xA2, 0x02,
  0xA5, 0xBC, 0x8D, 0xA3, 0x02,
  0xA9, 0x5A, 0x8D, 0xA8, 0x02,
  0xA6, 0x2B, 0xA4, 0x2C,
  0x18, 0x60
};

#define SAVE_HANDLER_ADDR 0x033C
#define LOAD_HANDLER_ADDR 0x0378

// ---------- DAC register definitions (RA4M1) ----------
#define MSTP_MSTPCRD   ((volatile unsigned int *)(0x40040000 + 0x7008))
#define PORTBASE       0x40040000
#define PFS_P014PFS    ((volatile unsigned int *)(PORTBASE + 0x100 + (14 * 4)))

#define DACBASE        0x40050000
#define DAC12_DADR0    ((volatile unsigned short *)(DACBASE + 0xE000))
#define DAC12_DACR     ((volatile unsigned char  *)(DACBASE + 0xE004))
#define DAC12_DADPR    ((volatile unsigned char  *)(DACBASE + 0xE005))
#define DAC12_DAVREFCR ((volatile unsigned char  *)(DACBASE + 0xE007))

// ---------- Sound state ----------
volatile uint8_t  vic_freq[3]    = {0,0,0};
volatile uint8_t  vic_noise_freq = 0;
volatile uint8_t  vic_volume     = 0;

static uint32_t phase[3] = {0,0,0};
static uint32_t step[3]  = {0,0,0};
static uint16_t lfsr = 0xFFFF;

#define SAMPLE_RATE 8000

FspTimer soundTimer;

// ---------- Sound ISR ----------
void sound_isr(timer_callback_args_t *args) {
  (void)args;
  uint8_t mixed = 0;

  for (int v = 0; v < 3; v++) {
    if (step[v] == 0) continue;
    phase[v] += step[v];
    mixed += (phase[v] >> 31);
  }

  if (vic_noise_freq >= 128) {
    uint8_t bit = ((lfsr >> 3) ^ (lfsr >> 12) ^ (lfsr >> 14) ^ (lfsr >> 15)) & 1;
    lfsr = (lfsr << 1) | bit;
    mixed += (lfsr & 1);
  }

  uint16_t level = (uint16_t)mixed * vic_volume * 4095 / 60;

  *DAC12_DADR0 = level;
}

// ---------- DAC init ----------
void dac_init() {
  *MSTP_MSTPCRD &= ~(0x01 << 20);

  *PFS_P014PFS = 0x00000000;
  *PFS_P014PFS |= (0x1 << 15);

  *DAC12_DADPR = 0x00;

  *DAC12_DADR0 = 0x0000;
  *DAC12_DAVREFCR = 0x00;
  *DAC12_DAVREFCR = 0x01;

  *DAC12_DACR = 0x5F;

  *DAC12_DADR0 = 0x0000;
}

// ---------- Sound init ----------
void sound_init() {
  dac_init();

  uint8_t timer_type = GPT_TIMER;
  int8_t channel = FspTimer::get_available_timer(timer_type);
  if (channel < 0) return;

  bool ok = soundTimer.begin(
    TIMER_MODE_PERIODIC,
    timer_type,
    channel,
    (float)SAMPLE_RATE,
    50.0f,
    sound_isr
  );
  if (!ok) return;

  soundTimer.setup_overflow_irq();
  soundTimer.open();
  soundTimer.start();
}

// ---------- Keyboard callback ----------
void on_keypressed(uint8_t code) {
  if (code == 0) return;
  if (code == KEY_F1) { help_mode = 1; return; }
  if (code == KEY_F2) { load_meteor(); return; }
  if (code == KEY_F3) { load_chess(); return; }
  if (code == KEY_F4) { load_castle(); return; }
  if (code == KEY_RUNSTOP) { runstop_pending = 1; return; }
  if (help_mode) { help_mode = 0; return; }
  if (RAM[0xC6] >= 10) return;
  RAM[0x0277 + RAM[0xC6]] = code;
  RAM[0xC6]++;
}

// ---------- 6502 memory hooks ----------
extern "C" uint8_t read6502(uint16_t address) {
  if (address < 0x1E00) return RAM[address];
  if (address >= 0xC000) return pgm_read_byte(&BIOS[address - 0xC000]);
  if ((address & 0xFE00) == 0x1E00) return videomem[address & 0x1FF];
  if ((address & 0xFFF0) == 0x9110) return ((address & 0x0F) == 0x1) ? 0x7F : 0x00;
  if ((address & 0xFFF0) == 0x9120) return 0x00;
  if ((address & 0xFFF0) == 0x9000) return 0x00;
  if (address >= 0x9600 && address < 0x9800) return colorram[address - 0x9600];
  return 0xFF;
}

extern "C" void write6502(uint16_t address, uint8_t value) {
  if (address < 0x1E00) { RAM[address] = value; return; }
  if ((address & 0xFE00) == 0x1E00) {
    if (videomem[address & 0x1FF] != value) {
      videomem[address & 0x1FF] = value;
      screen_dirty = 1;
    }
    return;
  }

  if (address == 0x900A) {
    vic_freq[0] = value;
    step[0] = (value >= 128) ? (uint32_t)(value - 128) * 5000000UL : 0;
    return;
  }
  if (address == 0x900B) {
    vic_freq[1] = value;
    step[1] = (value >= 128) ? (uint32_t)(value - 128) * 5000000UL : 0;
    return;
  }
  if (address == 0x900C) {
    vic_freq[2] = value;
    step[2] = (value >= 128) ? (uint32_t)(value - 128) * 5000000UL : 0;
    return;
  }
  if (address >= 0x9600 && address < 0x9800) {
    colorram[address - 0x9600] = value;
    color_dirty = 1;
    return;
  }
  if (address == 0x900D) { vic_noise_freq = value; return; }
  if (address == 0x900E) { vic_volume = value & 0x0F; return; }
}

// ---------- EEPROM directory ----------
uint8_t dir_get_name(uint8_t slot, char *buf) {
  if (slot >= DIR_ENTRIES) { buf[0] = 0; return 0; }
  uint16_t base = slot * DIR_ENTRY_SZ;
  uint8_t len = 0;
  for (uint8_t i = 0; i < 8; i++) {
    char c = EEPROM.read(base + i);
    if (c < 0x20 || c > 0x7E) break;
    buf[len++] = c;
  }
  buf[len] = 0;
  return len;
}

void dir_set_name(uint8_t slot, const char *name, uint8_t len) {
  if (slot >= DIR_ENTRIES) return;
  if (len > 8) len = 8;
  uint16_t base = slot * DIR_ENTRY_SZ;
  for (uint8_t i = 0; i < 8; i++) {
    char c = (i < len) ? name[i] : 0xFF;
    EEPROM.write(base + i, c);
  }
}

uint16_t dir_get_offset(uint8_t slot) {
  if (slot >= DIR_ENTRIES) return 0xFFFF;
  uint16_t base = slot * DIR_ENTRY_SZ + 8;
  return EEPROM.read(base) | (EEPROM.read(base + 1) << 8);
}

void dir_set_offset(uint8_t slot, uint16_t offset) {
  if (slot >= DIR_ENTRIES) return;
  uint16_t base = slot * DIR_ENTRY_SZ + 8;
  EEPROM.write(base, offset & 0xFF);
  EEPROM.write(base + 1, offset >> 8);
}

uint16_t dir_get_len(uint8_t slot) {
  if (slot >= DIR_ENTRIES) return 0;
  uint16_t base = slot * DIR_ENTRY_SZ + 10;
  return EEPROM.read(base) | (EEPROM.read(base + 1) << 8);
}

void dir_set_len(uint8_t slot, uint16_t len) {
  if (slot >= DIR_ENTRIES) return;
  uint16_t base = slot * DIR_ENTRY_SZ + 10;
  EEPROM.write(base, len & 0xFF);
  EEPROM.write(base + 1, len >> 8);
}

bool dir_is_empty(uint8_t slot) {
  if (slot >= DIR_ENTRIES) return true;
  uint16_t base = slot * DIR_ENTRY_SZ;
  return EEPROM.read(base) == 0xFF;
}

void dir_clear(uint8_t slot) {
  if (slot >= DIR_ENTRIES) return;
  uint16_t base = slot * DIR_ENTRY_SZ;
  for (uint8_t i = 0; i < DIR_ENTRY_SZ; i++) {
    EEPROM.write(base + i, 0xFF);
  }
}

int8_t find_slot_by_name(const char *name, uint8_t len) {
  if (len == 0) return -1;
  for (uint8_t i = 0; i < DIR_ENTRIES; i++) {
    if (dir_is_empty(i)) continue;
    char stored[9];
    uint8_t stored_len = dir_get_name(i, stored);
    if (stored_len != len) continue;
    bool match = true;
    for (uint8_t j = 0; j < len; j++) {
      if (stored[j] != name[j]) { match = false; break; }
    }
    if (match) return i;
  }
  return -1;
}

int8_t find_free_slot() {
  for (uint8_t i = 0; i < DIR_ENTRIES; i++) {
    if (dir_is_empty(i)) return i;
  }
  return -1;
}

uint16_t find_data_end() {
  uint16_t end = DATA_START;
  for (uint8_t i = 0; i < DIR_ENTRIES; i++) {
    if (dir_is_empty(i)) continue;

    uint16_t off = dir_get_offset(i);
    uint16_t len = dir_get_len(i);

    if (off < DATA_START) continue;
    if (off >= EEPROM_SIZE) continue;
    if (len == 0) continue;
    if (len == 0xFFFF) continue;
    if (off + len > EEPROM_SIZE) continue;
    if (off + len < off) continue;
    if (off < DIR_BYTES) continue;

    uint16_t slot_end = off + len;
    if (slot_end > end) end = slot_end;
  }
  return end;
}

// ---------- OLED helpers ----------
void oled_message(const char *msg, uint16_t ms) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(0, 20, msg);
  u8g2.sendBuffer();
  delay(ms);
}

void show_help_banner() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);

  u8g2.drawStr(0,  7, "unoR4/oled VIC20 C.BOSCH");
  u8g2.drawStr(0, 14, "DIR    : SAVE\"@\"");
  u8g2.drawStr(0, 21, "SAVE   : SAVE\"NAME\"");
  u8g2.drawStr(0, 28, "LOAD   : LOAD\"NAME\"");
  u8g2.drawStr(0, 35, "DEL    : SAVE\"#NAME\"");
  u8g2.drawStr(0, 42, "FORMAT : SAVE\"!FORMAT\"");
  u8g2.drawStr(0, 49, "F1: CLR, CAPSLOCK: STOP");
  u8g2.drawStr(0, 56, "F2:ARCA F3:CHESS F4:ADV");
  u8g2.drawStr(0, 63, "press any key");

  u8g2.sendBuffer();
}

// ---------- Save ----------
void do_save(const char *name, uint8_t name_len) {
  if (name_len == 0 || name_len > 8) {
    oled_message("BAD NAME", 1000);
    return;
  }

  bool has_printable = false;
  for (uint8_t i = 0; i < name_len; i++) {
    char c = name[i];
    if (c < 0x20 || c > 0x7E) {
      oled_message("BAD NAME", 1000);
      return;
    }
    if (c != ' ') has_printable = true;
  }
  if (!has_printable) {
    oled_message("BAD NAME", 1000);
    return;
  }

  uint16_t prog_start = RAM[0x2B] | (RAM[0x2C] << 8);
  uint16_t prog_end   = RAM[0x2D] | (RAM[0x2E] << 8);

  if (prog_end <= prog_start || prog_start < 0x0401) {
    oled_message("NO PROGRAM", 1000);
    return;
  }

  uint16_t len = prog_end - prog_start;

  int8_t slot = find_slot_by_name(name, name_len);

  uint16_t base;
  uint16_t free_end = find_data_end();

  if (slot >= 0) {
    uint16_t old_base = dir_get_offset(slot);
    uint16_t old_len  = dir_get_len(slot);

    if (old_base + old_len == free_end && old_base >= DATA_START) {
      base = old_base;

      if (base + len > EEPROM_SIZE) {
        oled_message("DISK FULL", 1500);
        return;
      }

      oled_message("SAVING", 500);
      for (uint16_t i = 0; i < len; i++) {
        EEPROM.write(base + i, RAM[prog_start + i]);
      }

      dir_set_name(slot, name, name_len);
      dir_set_offset(slot, base);
      dir_set_len(slot, len);
      return;
    }
  } else {
    slot = find_free_slot();
    if (slot < 0) {
      oled_message("NO SLOTS", 1000);
      return;
    }
  }

  base = free_end;

  if (base + len > EEPROM_SIZE) {
    oled_message("DISK FULL", 1500);
    return;
  }

  oled_message("SAVING", 500);

  for (uint16_t i = 0; i < len; i++) {
    EEPROM.write(base + i, RAM[prog_start + i]);
  }

  dir_set_name(slot, name, name_len);
  dir_set_offset(slot, base);
  dir_set_len(slot, len);
}

// ---------- Load ----------
void do_load(const char *name, uint8_t name_len) {
  if (name_len == 0 || name_len > 8) {
    oled_message("BAD NAME", 1000);
    return;
  }

  int8_t slot = find_slot_by_name(name, name_len);
  if (slot < 0) {
    oled_message("NOT FOUND", 1000);
    return;
  }

  uint16_t base = dir_get_offset(slot);
  uint16_t len  = dir_get_len(slot);

  if (base == 0xFFFF || base < DATA_START || base >= EEPROM_SIZE) {
    oled_message("CORRUPT", 1000);
    return;
  }
  if (len == 0 || len == 0xFFFF || base + len > EEPROM_SIZE) {
    oled_message("CORRUPT", 1000);
    return;
  }

  oled_message("LOADING", 500);

  uint16_t prog_start = RAM[0x2B] | (RAM[0x2C] << 8);
  if (prog_start < 0x0401 || prog_start > 0x1C00) prog_start = 0x0401;
  uint16_t prog_end = prog_start + len;

  for (uint16_t i = 0; i < len; i++) {
    RAM[prog_start + i] = EEPROM.read(base + i);
  }

  RAM[0x2B] = prog_start & 0xFF;
  RAM[0x2C] = prog_start >> 8;
  RAM[0x2D] = prog_end & 0xFF;
  RAM[0x2E] = prog_end >> 8;
  RAM[0x2F] = RAM[0x2D];
  RAM[0x30] = RAM[0x2E];
  RAM[0x31] = RAM[0x2D];
  RAM[0x32] = RAM[0x2E];
}

// ---------- Directory listing ----------
void do_dir() {
  oled_message("DIR", 800);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);

  uint8_t line = 0;
  for (uint8_t i = 0; i < DIR_ENTRIES && line < 8; i++) {
    if (dir_is_empty(i)) continue;

    char name[9];
    uint8_t name_len = dir_get_name(i, name);
    uint16_t len = dir_get_len(i);

    char buf[24];
    uint8_t p = 0;
    for (uint8_t j = 0; j < name_len && p < 20; j++) buf[p++] = name[j];
    while (p < 10) buf[p++] = ' ';
    buf[p++] = ' ';
    if (len >= 1000) buf[p++] = '0' + (len / 1000);
    if (len >= 100)  buf[p++] = '0' + ((len / 100) % 10);
    if (len >= 10)   buf[p++] = '0' + ((len / 10) % 10);
    buf[p++] = '0' + (len % 10);
    buf[p] = 0;

    u8g2.drawStr(0, (line + 1) * 7, buf);
    line++;
  }

  if (line == 0) {
    u8g2.drawStr(0, 20, "(empty)");
  }

  u8g2.sendBuffer();
  delay(2000);
}

// ---------- Scratch ----------
void do_scratch(const char *name, uint8_t name_len) {
  if (name_len == 0 || name_len > 8) {
    oled_message("BAD NAME", 1000);
    return;
  }

  int8_t slot = find_slot_by_name(name, name_len);
  if (slot < 0) {
    oled_message("NOT FOUND", 1000);
    return;
  }

  oled_message("DELETED", 1000);
  dir_clear(slot);
}

// ---------- Format ----------
void do_format() {
  oled_message("FORMATTING", 500);
  for (uint16_t i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0xFF);
  }
  oled_message("FORMATTED", 1000);
}

// ---------- Mailbox dispatch ----------
uint8_t name_from_mailbox(char *buf, uint8_t max_len) {
  uint8_t len = RAM[MB_NAMELEN];
  uint16_t ptr = RAM[MB_NAMEPTR_L] | (RAM[MB_NAMEPTR_H] << 8);
  if (len > max_len) len = max_len;
  for (uint8_t i = 0; i < len; i++) {
    uint8_t c = RAM[ptr + i];
    if (c >= 0xC1 && c <= 0xDA) c -= 0x80;
    buf[i] = c;
  }
  buf[len] = 0;
  return len;
}

void check_mailbox() {
  uint8_t op = RAM[MB_OP];
  if (op == 0) return;

  if (op != MB_OP_SAVE && op != MB_OP_LOAD) return;

  char name[12];
  uint8_t name_len = name_from_mailbox(name, 8);

  if (name_len == 7 &&
      name[0] == '!' && name[1] == 'F' && name[2] == 'O' && name[3] == 'R' &&
      name[4] == 'M' && name[5] == 'A' && name[6] == 'T') {
    do_format();
    RAM[MB_OP] = 0;
    return;
  }

  if (name_len == 1 && name[0] == '@') {
    do_dir();
    RAM[MB_OP] = 0;
    return;
  }

  if (name_len == 1 && name[0] == '#') {
    oled_message("BAD NAME", 1000);
    RAM[MB_OP] = 0;
    return;
  }

  if (name_len >= 2 && name[0] == '#') {
    do_scratch(name + 1, name_len - 1);
    RAM[MB_OP] = 0;
    return;
  }

  if (op == MB_OP_SAVE)      do_save(name, name_len);
  else if (op == MB_OP_LOAD) do_load(name, name_len);

  RAM[MB_OP] = 0;
}

void install_save_load() {
  for (uint16_t i = 0; i < sizeof(save_handler); i++) {
    RAM[SAVE_HANDLER_ADDR + i] = pgm_read_byte(&save_handler[i]);
  }
  for (uint16_t i = 0; i < sizeof(load_handler); i++) {
    RAM[LOAD_HANDLER_ADDR + i] = pgm_read_byte(&load_handler[i]);
  }

  RAM[0x0330] = LOAD_HANDLER_ADDR & 0xFF;
  RAM[0x0331] = LOAD_HANDLER_ADDR >> 8;
  RAM[0x0332] = SAVE_HANDLER_ADDR & 0xFF;
  RAM[0x0333] = SAVE_HANDLER_ADDR >> 8;

  RAM[MB_OP] = 0;
}

// ---------- PETSCII graphics rendering ----------

/*
void draw_petscii_glyph(uint8_t x, uint8_t y, uint8_t code) {
  for (uint8_t row = 0; row < 7; row++) {
    uint8_t bits = pgm_read_byte(&charROM[row * 128 + (code & 0x7F)]);
    for (uint8_t col = 0; col < 5; col++) {
      uint8_t rom_col = (col * 8) / 5;
      if (bits & (0x80 >> rom_col)) {
        u8g2.drawPixel(x + col, y + row);
      }
    }
  }
}*/

void draw_petscii_glyph(uint8_t x, uint8_t y, uint8_t code) {
  for (uint8_t row = 0; row < 7; row++) {
    uint8_t bits = pgm_read_byte(&charROM[row * 128 + (code & 0x7F)]);
    for (uint8_t col = 0; col < 5; col++) {
      uint8_t rom_col = (col * 8) / 5;
      if (bits & (0x80 >> rom_col)) {
        u8g2.drawPixel(x + col, y + row);
      }
    }
  }
}

/*
void draw_petscii_lower(uint8_t x, uint8_t y, uint8_t code) {
  for (uint8_t row = 0; row < 7; row++) {
    uint8_t bits = pgm_read_byte(&charROM_lower[row * 128 + (code & 0x7F)]);
    for (uint8_t col = 0; col < 5; col++) {
      uint8_t rom_col = (col * 8) / 5;
      if (bits & (0x80 >> rom_col)) {
        u8g2.drawPixel(x + col, y + row);
      }
    }
  }
}
*/
void draw_petscii_lower(uint8_t x, uint8_t y, uint8_t code) {
  uint8_t idx = code & 0x7F;
  for (uint8_t row = 0; row < 7; row++) {
    uint8_t bits = pgm_read_byte(&charROM_lower[idx * 8 + row]);
    for (uint8_t col = 0; col < 5; col++) {
      uint8_t rom_col = (col * 8) / 5;
      if (bits & (0x80 >> rom_col)) {
        u8g2.drawPixel(x + col, y + row);
      }
    }
  }
}
// ---------- Rendering ----------
void render_screen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);

  const int CELL_W = 5;
  const int CELL_H = 7;
  const int ASCENT = 7;
  const uint8_t VISIBLE_ROWS = 64 / CELL_H;
  const uint8_t BOTTOM_CLAMP = 23 - VISIBLE_ROWS;

  uint16_t base = ((uint16_t)RAM[0xD2] << 8) | RAM[0xD1];
  uint16_t cursor_ptr = base + RAM[0xD3];
  uint8_t cursor_row = 0;

  if (cursor_ptr >= 0x1E00 && cursor_ptr < 0x1E00 + 506) {
    cursor_row = (cursor_ptr - 0x1E00) / 22;
  }

  static uint8_t last_first_row = 0;

  uint8_t first_row = 0;
  if (cursor_row >= VISIBLE_ROWS) {
    first_row = cursor_row - (VISIBLE_ROWS - 1);
  }
  if (first_row > BOTTOM_CLAMP) first_row = BOTTOM_CLAMP;

  if (cursor_row >= 22 && last_first_row == BOTTOM_CLAMP) {
    first_row = last_first_row;
  }
  last_first_row = first_row;

  for (uint8_t r = 0; r < VISIBLE_ROWS; r++) {
    uint8_t vic_row = first_row + r;
    int y_baseline = r * CELL_H + ASCENT;
    int y_top = r * CELL_H;

    for (uint8_t c = 0; c < 22; c++) {
      uint8_t raw = videomem[vic_row * 22 + c];
      uint8_t code = raw & 0x7F;
      uint8_t color = colorram[vic_row * 22 + c];
      bool reverse = (color & 0x08) != 0;
      bool lower = (raw & 0x80) != 0;
      int x = c * CELL_W;

      if (code == 0x66 && !reverse) {
        u8g2.drawBox(x, y_top, CELL_W, CELL_H);
        continue;
      }

      if (reverse) {
        u8g2.setDrawColor(1);
        u8g2.drawBox(x, y_top, CELL_W, CELL_H);
        u8g2.setDrawColor(0);
      }

      if (lower) {
        draw_petscii_lower(x, y_top, code);
      } else if (code >= 0x40) {
        draw_petscii_glyph(x, y_top, code);
      } else {
        uint8_t ascii = pgm_read_byte(&screen_to_ascii[code]);
        if (ascii >= 0x20 && ascii <= 0x7E) {
          u8g2.drawGlyph(x, y_baseline, ascii);
        }
      }

      if (reverse) {
        u8g2.setDrawColor(1);
      }
    }
  }

  static uint32_t last_blink = 0;
  static uint8_t blink_on = 1;
  if (millis() - last_blink >= 500) {
    last_blink = millis();
    blink_on ^= 1;
  }

  if (blink_on && cursor_ptr >= 0x1E00 && cursor_ptr < 0x1E00 + 506) {
    uint16_t off = cursor_ptr - 0x1E00;
    uint8_t cur_row = off / 22;
    uint8_t cur_col = off % 22;
    if (cur_row >= first_row && cur_row < first_row + VISIBLE_ROWS) {
      uint8_t screen_row = cur_row - first_row;
      u8g2.drawBox(cur_col * CELL_W, screen_row * CELL_H, CELL_W, CELL_H);
    }
  }

  u8g2.sendBuffer();
}

void load_meteor() {
  uint16_t load_addr = pgm_read_byte(&meteor_prg[0]) | (pgm_read_byte(&meteor_prg[1]) << 8);
  uint16_t data_len = meteor_prg_size - 2;
  if (load_addr + data_len > 0x1DFF) { oled_message("TOO BIG", 1000); return; }
  for (uint16_t i = 0; i < data_len; i++) { RAM[load_addr + i] = pgm_read_byte(&meteor_prg[2 + i]); }
  RAM[0x2B] = load_addr & 0xFF;
  RAM[0x2C] = load_addr >> 8;
  RAM[0x2D] = (load_addr + data_len) & 0xFF;
  RAM[0x2E] = (load_addr + data_len) >> 8;
  RAM[0x2F] = RAM[0x2D];
  RAM[0x30] = RAM[0x2E];
  RAM[0x31] = RAM[0x2D];
  RAM[0x32] = RAM[0x2E];
  oled_message("METEOR LOADED - TYPE RUN", 1000);
}

void load_chess() {
  uint16_t load_addr = pgm_read_byte(&chess_prg[0]) | (pgm_read_byte(&chess_prg[1]) << 8);
  uint16_t data_len = chess_prg_size - 2;
  if (load_addr + data_len > 0x1DFF) { oled_message("TOO BIG", 1000); return; }
  for (uint16_t i = 0; i < data_len; i++) { RAM[load_addr + i] = pgm_read_byte(&chess_prg[2 + i]); }
  RAM[0x2B] = load_addr & 0xFF;
  RAM[0x2C] = load_addr >> 8;
  RAM[0x2D] = (load_addr + data_len) & 0xFF;
  RAM[0x2E] = (load_addr + data_len) >> 8;
  RAM[0x2F] = RAM[0x2D];
  RAM[0x30] = RAM[0x2E];
  RAM[0x31] = RAM[0x2D];
  RAM[0x32] = RAM[0x2E];
  oled_message("CHESS LOADED - TYPE RUN", 1000);
}

void load_castle() {
  uint16_t load_addr = pgm_read_byte(&castle_prg[0]) | (pgm_read_byte(&castle_prg[1]) << 8);
  uint16_t data_len = castle_prg_size - 2;
  if (load_addr + data_len > 0x1DFF) { oled_message("TOO BIG", 1000); return; }
  for (uint16_t i = 0; i < data_len; i++) { RAM[load_addr + i] = pgm_read_byte(&castle_prg[2 + i]); }
  RAM[0x2B] = load_addr & 0xFF;
  RAM[0x2C] = load_addr >> 8;
  RAM[0x2D] = (load_addr + data_len) & 0xFF;
  RAM[0x2E] = (load_addr + data_len) >> 8;
  RAM[0x2F] = RAM[0x2D];
  RAM[0x30] = RAM[0x2E];
  RAM[0x31] = RAM[0x2D];
  RAM[0x32] = RAM[0x2E];
  oled_message("CASTLE LOADED - TYPE RUN", 1000);
}

// ---------- setup ----------
void setup() {
  u8g2.begin();
  u8g2.setPowerSave(0);
  u8g2.clearBuffer();
  u8g2.sendBuffer();

  memset(RAM, 0, sizeof(RAM));
  memset(videomem, 0x20, sizeof(videomem));
  memset(colorram, 0x0E, sizeof(colorram));

  sound_init();
  kb_init();
  reset6502();

  uint32_t boot_guard = 0;
  while (boot_guard < 500000) {
    exec6502();
    boot_guard++;
    uint16_t pc = get_pc();
    if (pc == 0xE56D || pc == 0xE570) break;
  }

  install_save_load();
}

// ---------- loop ----------
void loop() {
  RAM[0x0330] = LOAD_HANDLER_ADDR & 0xFF;
  RAM[0x0331] = LOAD_HANDLER_ADDR >> 8;
  RAM[0x0332] = SAVE_HANDLER_ADDR & 0xFF;
  RAM[0x0333] = SAVE_HANDLER_ADDR >> 8;

  for (int i = 0; i < 100; i++) {
    if (runstop_pending) {
      runstop_pending = 0;
      set_pc(0xE386);
    }
    exec6502();
  }

  check_mailbox();

  static uint32_t last_render = 0;

  if (help_mode) {
    show_help_banner();
    last_render = millis();
  } else if (millis() - last_render >= 32) {
    render_screen();
    last_render = millis();
  }
}