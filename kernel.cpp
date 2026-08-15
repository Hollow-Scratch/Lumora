#include <stdint.h>

static uint16_t* const VGA = (uint16_t*)0xB8000;

extern "C" {
void kernel_main(void)
{
    const char* msg = "Welcome to Lumora OS!";
    uint8_t color = 0x0F;

    for (int i = 0; msg[i] != '\0'; i++)
    {
        VGA[i] = ((uint16_t)color << 8) | msg[i];
    }

    while (1)
    {
        __asm__ volatile ("hlt");
    }
}

}
