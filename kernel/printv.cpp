#include "printv.hpp"
#include <stdint.h>

#define VGA_MEMORY 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

volatile uint16_t *VGA = reinterpret_cast<volatile uint16_t *>(VGA_MEMORY);

uint16_t cursor_col = 0;
uint16_t cursor_row = 0;

void clear_screen()
{
	for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
		VGA[i] = (0x0F << 8) | ' ';
	}
	cursor_col = 0;
	cursor_row = 0;
}

void scroll_screen()
{
	for (int y = 0; y < VGA_HEIGHT - 1; y++) {
		for (int x = 0; x < VGA_WIDTH; x++) {
			int src_idx = (y + 1) * VGA_WIDTH + x;
			int dest_idx = y * VGA_WIDTH + x;
			VGA[dest_idx] = VGA[src_idx];
		}
	}

	int bottom_row_start = (VGA_HEIGHT - 1) * VGA_WIDTH;
	for (int x = 0; x < VGA_WIDTH; x++) {
		VGA[bottom_row_start + x] = (0x0F << 8) | ' ';
	}

	cursor_row = VGA_HEIGHT - 1;
}

void printv(const char *str)
{
	for (int i = 0; str[i] != '\0'; i++) {
		if (str[i] == '\n') {
			cursor_col = 0;
			cursor_row++;
			if (cursor_row >= VGA_HEIGHT) {
				scroll_screen();
			}
			continue;
		}

		int vga_index = (cursor_row * VGA_WIDTH) + cursor_col;
		VGA[vga_index] = (0x0F << 8) | str[i];

		cursor_col++;
		if (cursor_col >= VGA_WIDTH) {
			cursor_col = 0;
			cursor_row++;
			if (cursor_row >= VGA_HEIGHT) {
				scroll_screen();
			}
		}
	}
}
