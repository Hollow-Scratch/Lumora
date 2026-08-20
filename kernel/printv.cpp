#include <stdint.h>

#define VGA_MEMORY 0xB8000

volatile uint16_t* VGA = reinterpret_cast<volatile uint16_t*>(VGA_MEMORY);

void printv(const char* str) {
  for(int i = 0; str[i] != '\0'; i++) {
    VGA[i] = (0x0F << 8) | str[i];
  }
}
