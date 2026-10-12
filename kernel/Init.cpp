#include <stdint.h>

struct VgaCell {
  char character;
  uint8_t color;
};

extern "C" void kernel_main() {
  auto *vga_buffer = reinterpret_cast<VgaCell *>(0xB8000);

  uint8_t color_white_on_black = 0x0F;

  for (int i = 0; i < 80 * 25; ++i) {
    vga_buffer[i].character = ' ';
    vga_buffer[i].color = color_white_on_black;
  }

  const char *message = "WELCOME TO LUMORA OS (MINIMAL VGA TEXT MODE)";

  int index = 0;
  while (message[index] != '\0') {
    vga_buffer[index].character = message[index];
    vga_buffer[index].color = 0x0A; // 0x0A = Bright Green text!
    index++;
  }

  while (1) {
    asm volatile("hlt");
  }
}
