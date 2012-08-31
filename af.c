// PWM example, based on code from http://elinux.org/RPi_Low-level_peripherals for the mmap part
// and http://www.raspberrypi.org/phpBB3/viewtopic.php?t=8467&p=124620 for PWM initialization
//
// compile with "gcc pwm.c -o pwm", test with "./pwm" (needs to be root for /dev/mem access)
//
// Frank Buss, 2012

#define BCM2708_PERI_BASE	0x20000000
#define GPIO_BASE		(BCM2708_PERI_BASE + 0x200000) /* GPIO controller */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>

#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)

// I/O access
volatile unsigned *gpio;

// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))

#define GPIO_BANK(g) (*(gpio+(((g)/10))))
#define GPIO_FSET(g,a) (GPIO_BANK(g) =  \
    (GPIO_BANK(g) & (~(7<<(((g)%10)*3)))) | ((a)<<(((g)%10)*3)))
#define GPIO_FGET(g) (GPIO_BANK(g) >> ((((g)%10)*3)) & 7)

#define GPIO_SET *(gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0

// map 4k register memory for direct access from user space and return a user space pointer to it
static volatile unsigned *mapRegisterMemory(int base)
{
	static int mem_fd = 0;
	char *mem, *map;
	
	/* open /dev/mem */
	if (!mem_fd) {
		if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
			printf("can't open /dev/mem \n");
			exit (-1);
		}
	}
	
	/* mmap register */
	
	// Allocate MAP block
	if ((mem = malloc(BLOCK_SIZE + (PAGE_SIZE-1))) == NULL) {
		printf("allocation error \n");
		exit (-1);
	}
	
	// Make sure pointer is on 4K boundary
	if ((unsigned long)mem % PAGE_SIZE)
		mem += PAGE_SIZE - ((unsigned long)mem % PAGE_SIZE);
	
	// Now map it
	map = (char *)mmap(
		(caddr_t)mem,
		BLOCK_SIZE,
		PROT_READ|PROT_WRITE,
		MAP_SHARED|MAP_FIXED,
		mem_fd,
		base
	);
	
	if ((long)map < 0) {
		printf("mmap error %d\n", (int)map);
		exit (-1);
	}
	
	// Always use volatile pointer!
	return (volatile unsigned *)map;
}

// set up a memory regions to access GPIO, PWM and the clock manager
static void setupRegisterMemoryMappings()
{
	gpio = mapRegisterMemory(GPIO_BASE);
}


static const char *mapping[] = {
    "Input",
    "Output",
    "AF5",
    "AF4",
    "AF0",
    "AF1",
    "AF2",
    "AF3",
};

int main(int argc, char **argv)
{ 
    int i;
	// init PWM module for GPIO pin 18 with 50 Hz frequency
	setupRegisterMemoryMappings();

    if (argc == 3) {
        int pin = strtoul(argv[1], NULL, 0);
        int af;
        if (argv[2][0] == 'I' || argv[2][0] == 'i')
            af = 0;
        else if (argv[2][0] == 'O' || argv[2][0] == 'o')
            af = 1;
        else if (argv[2][0] == '5')
            af = 2;
        else if (argv[2][0] == '4')
            af = 3;
        else if (argv[2][0] == '0')
            af = 4;
        else if (argv[2][0] == '1')
            af = 5;
        else if (argv[2][0] == '2')
            af = 6;
        else if (argv[2][0] == '3')
            af = 7;
        else {
            fprintf(stderr, "Unrecognized function: %c.  Must be one of {io012345}",
                    argv[2][0]);
            return 1;
        }

        if (pin < 0 || pin > 54) {
            fprintf(stderr, "Pin %d out of range!  Only 54 pins present.\n", pin);
            return 1;
        }

        printf("Setting GPIO%d to %s...\n", pin, mapping[af]);
        GPIO_FSET(pin, af);
    }

    for (i=0; i<54; i++) {
        printf("GPIO%d: %s\n", i, mapping[GPIO_FGET(i)]);
    }

	
	return 0;
}
