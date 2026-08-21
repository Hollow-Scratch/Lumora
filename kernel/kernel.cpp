#include "printv.hpp"

extern "C" {
void kernel_main(void)
{
	clear_screen();
	printv("Hello again! \n");
	printv("HAHHA AGAIN");
}
}
