#ifndef PRINT_H
#define PRINT_H

#ifdef __cplusplus
extern "C" {
#endif

// Clears the screen and resets the cursor to the top-left corner
void clear_screen();

// Prints a null-terminated string with automatic line wrapping and scrolling
void printv(const char *str);

#ifdef __cplusplus
}
#endif

#endif	// PRINT_H
