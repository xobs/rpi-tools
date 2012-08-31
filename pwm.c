// PWM example, based on code from http://elinux.org/RPi_Low-level_peripherals for the mmap part
// and http://www.raspberrypi.org/phpBB3/viewtopic.php?t=8467&p=124620 for PWM initialization
//
// compile with "gcc pwm.c -o pwm", test with "./pwm" (needs to be root for /dev/mem access)
//
// Frank Buss, 2012

#define BCM2708_PERI_BASE	0x20000000
#define GPIO_BASE		(BCM2708_PERI_BASE + 0x200000) /* GPIO controller */
#define PWM_BASE		(BCM2708_PERI_BASE + 0x20C000) /* PWM controller */
#define CLOCK_BASE		(BCM2708_PERI_BASE + 0x101000)

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include <unistd.h>

#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)

struct bits {
    int reserved:1;
    int readable:1;
    int writeable:1;
    int sentinal:1;
    unsigned long reset;
    unsigned long start;
    unsigned long stop;
    char *description;
    char *name;
};

struct reg {
    char *description;
    char *name;
    unsigned long offset;
    int sentinal:1;
    struct bits fields[32];

    /* Some registers have required values */
    unsigned long required;
};

struct regs {
    char *name;
    char *description;
    volatile unsigned long *mem;
    struct reg regs[256];
};



static struct regs clk_regs = {
    .name = "CLK",
    .description = "Clock registers",
    .regs = {
        {
            .name = "PWM_DIV",
            .description = "Divisor for PWM clock",
            .offset = 0xa4,
            .required = 0x5A000000,
            .fields = {
                {
                    .name = "PASS",
                    .description = "Broadcom clock password",
                    .start = 24,
                    .stop = 31,
                    .readable = 1,
                    .writeable = 1,
                    .reset = 0x5a,
                },
                {
                    .name = "DIV",
                    .start = 12,
                    .stop = 23,
                    .description = "PWM divisor",
                    .readable = 1,
                    .writeable = 1,
                },
                {
                    .name = "????",
                    .start = 0,
                    .stop = 11,
                    .description = "No idea",
                    .readable = 1,
                    .writeable = 1,
                },
                { .sentinal = 1, },
            },
        },
        {
            .name = "PWM_CNTL",
            .description = "Control for PWM clock",
            .offset = 0xa0,
            .required = 0x5A000000,
            .fields = {
                {
                    .name = "PASS",
                    .description = "Broadcom clock password",
                    .start = 24,
                    .stop = 31,
                    .readable = 1,
                    .writeable = 1,
                    .reset = 0x5a,
                },
                {
                    .name = "???2",
                    .start = 5,
                    .stop = 23,
                    .description = "Dunno (2)",
                    .readable = 1,
                    .writeable = 1,
                },
                {
                    .name = "SOURCE",
                    .start = 4,
                    .stop = 4,
                    .description = "Source for this particular clock (1=oscillator)",
                    .readable = 1,
                    .writeable = 1,
                },
                {
                    .name = "???1",
                    .start = 1,
                    .stop = 3,
                    .description = "Dunno (1)",
                    .readable = 1,
                    .writeable = 1,
                },
                {
                    .name = "ENABLE",
                    .start = 0,
                    .stop = 0,
                    .description = "Enable this clock",
                    .readable = 1,
                    .writeable = 1,
                },
                { .sentinal = 1, },
            },
        },
        { .sentinal = 1, },
    },
};

static struct regs pwm_regs = {
    .name = "PWM",
    .description = "Pulse Width Modulation registers",
    .regs = {
        {
            .description = "Defines various PWM control channels",
            .name = "CTL",
            .offset = 0x0,
            .fields = {
                {
                    .reserved = 1,
                    .start = 16,
                    .stop = 31,
                },
                {
                    .name = "MSEN2",
                    .description = "Channel 2 M/S Enable (0: PWM algorithm used, 1: M/S transmission used)",
                    .start = 15,
                    .stop = 15,
                    .readable = 1,
                    .writeable = 1,
                    .reset = 0,
                },
                {
                    .reserved = 1,
                    .start = 14,
                    .stop = 14,
                },
                {
                    .name = "USEF2",
                    .start = 13,
                    .stop = 13,
                    .description = "Channel 2 Use Fifo (0: Data register is transmitted, 1: Fifo is used for transmission)",
                    .readable = 1,
                    .writeable = 1,
                },
                {
                    .name = "POLA2",
                    .start = 12,
                    .stop = 12,
                    .description = "Channel 2 Polarity (0: 0=low 1=high, 1: 1=low 0=high)",
                    .readable = 1,
                    .writeable = 1,
                },
                {
                    .name = "SBIT2",
                    .start = 11,
                    .stop = 11,
                    .description = "Channel 2 Silence Bit (Defines the state of the output when no transmission takes place)",
                    .readable = 1,
                    .writeable = 1,
                },
                {
                    .name = "RPTL2",
                    .start = 10,
                    .stop = 10,
                    .description = "Channel 2 Repeat Last Data (0: Transmission interrupts when FIFO is empty 1: Last data in FIFO is transmitted repeatedly until FIFO is not empty)",
                    .readable = 1,
                    .writeable = 1,
                },
                {
                    .name = "MODE2",
                    .start = 9,
                    .stop = 9,
                    .description = "Channel 2 Mode (0: PWM mode 1: Serialiser mode)",
                    .readable = 1,
                    .writeable = 1,
                },
                {
                    .name = "PWEN2",
                    .start = 8,
                    .stop = 8,
                    .description = "Channel 2 Enable (0: Channel is disabled 1: Channel is enabled)",
                    .readable = 1,
                    .writeable = 1,
                },

                {
                    .name = "MSEN1",
                    .description = "Channel 1 M/S Enable (0: PWM algorithm used, 1: M/S transmission used)",
                    .start = 7,
                    .stop = 7,
                    .readable = 1,
                    .writeable = 1,
                    .reset = 0,
                },
                {
                    .name = "CLRF1",
                    .description = "Clear Fifo (1: Clears FIFO 0: Has no effect)",
                    .start = 7,
                    .stop = 7,
                    .readable = 1,
                    .writeable = 0,
                },
                {
                    .name = "USEF1",
                    .start = 5,
                    .stop = 5,
                    .description = "Channel 1 Use Fifo (0: Data register is transmitted, 1: Fifo is used for transmission)",
                    .readable = 1,
                    .writeable = 1,
                },
                {
                    .name = "POLA1",
                    .start = 4,
                    .stop = 4,
                    .description = "Channel 1 Polarity (0: 0=low 1=high, 1: 1=low 0=high)",
                    .readable = 1,
                    .writeable = 1,
                },
                {
                    .name = "SBIT1",
                    .start = 3,
                    .stop = 3,
                    .description = "Channel 1 Silence Bit (Defines the state of the output when no transmission takes place)",
                    .readable = 1,
                    .writeable = 1,
                },
                {
                    .name = "RPTL1",
                    .start = 2,
                    .stop = 2,
                    .description = "Channel 1 Repeat Last Data (0: Transmission interrupts when FIFO is empty 1: Last data in FIFO is transmitted repeatedly until FIFO is not empty)",
                    .readable = 1,
                    .writeable = 1,
                },
                {
                    .name = "MODE1",
                    .start = 1,
                    .stop = 1,
                    .description = "Channel 1 Mode (0: PWM mode 1: Serialiser mode)",
                    .readable = 1,
                    .writeable = 1,
                },
                {
                    .name = "PWEN1",
                    .start = 0,
                    .stop = 0,
                    .description = "Channel 1 Enable (0: Channel is disabled 1: Channel is enabled)",
                    .readable = 1,
                    .writeable = 1,
                },
                { .sentinal = 1, },
            },
        },
        {
            .description = "Displays PWM status",
            .name = "STA",
            .offset = 0x4,
            .fields = {
                {
                    .reserved = 1,
                    .start = 13,
                    .stop = 31,
                },
                {
                    .name = "STA4",
                    .start = 12,
                    .stop = 12,
                    .description = "Channel 4 State",
                    .readable = 1,
                    .writeable = 1,
                },
                {
                    .name = "STA3",
                    .start = 11,
                    .stop = 11,
                    .description = "Channel 3 State",
                    .readable = 1,
                    .writeable = 1,
                },
                {
                    .name = "STA2",
                    .start = 10,
                    .stop = 10,
                    .description = "Channel 2 State",
                    .readable = 1,
                    .writeable = 1,
                },
                {
                    .name = "STA1",
                    .start = 9,
                    .stop = 9,
                    .description = "Channel 1 State",
                    .readable = 1,
                    .writeable = 1,
                },
                {
                    .name = "BERR",
                    .start = 8,
                    .stop = 8,
                    .description = "Bus Error Flag",
                    .readable = 1,
                    .writeable = 1,
                },
                {
                    .name = "GAPO4",
                    .start = 7,
                    .stop = 7,
                    .description = "Channel 4 Gap Occurred Flag",
                    .readable = 1,
                    .writeable = 1,
                },
                {
                    .name = "GAPO3",
                    .start = 6,
                    .stop = 6,
                    .description = "Channel 3 Gap Occurred Flag",
                    .readable = 1,
                    .writeable = 1,
                },
                {
                    .name = "GAPO2",
                    .start = 5,
                    .stop = 5,
                    .description = "Channel 2 Gap Occurred Flag",
                    .readable = 1,
                    .writeable = 1,
                },
                {
                    .name = "GAPO1",
                    .start = 4,
                    .stop = 4,
                    .description = "Channel 1 Gap Occurred Flag",
                    .readable = 1,
                    .writeable = 1,
                },
                {
                    .name = "RERR1",
                    .start = 3,
                    .stop = 3,
                    .description = "Fifo Read Error Flag",
                    .readable = 1,
                    .writeable = 1,
                },
                {
                    .name = "WERR1",
                    .start = 2,
                    .stop = 2,
                    .description = "Fifo Write Error Flag",
                    .readable = 1,
                    .writeable = 1,
                },
                {
                    .name = "EMPT1",
                    .start = 1,
                    .stop = 1,
                    .description = "Fifo Empty Flag",
                    .readable = 1,
                    .writeable = 1,
                },
                {
                    .name = "FULL1",
                    .start = 0,
                    .stop = 0,
                    .description = "Fifo Full Flag",
                    .readable = 1,
                    .writeable = 1,
                },
                { .sentinal = 1, },
            },
        },
        {
            .name = "DMAC",
            .description = "Enables DMA transfer",
            .offset = 0x8,
            .fields = {
                {
                    .name = "ENAB",
                    .start = 31,
                    .stop = 31,
                    .description = "DMA Enable (0: DMA disabled 1: DMA enabled)",
                    .readable = 1,
                    .writeable = 1,
                },
                {
                    .reserved = 1,
                    .start = 16,
                    .stop = 30,
                },
                {
                    .name = "PANIC",
                    .start = 8,
                    .stop = 15,
                    .description = "DMA Threshold for PANIC signal",
                    .readable = 1,
                    .writeable = 1,
                    .reset = 0x7,
                },
                {
                    .name = "DREQ",
                    .start = 0,
                    .stop = 7,
                    .description = "DMA Threshold for DREQ signal",
                    .readable = 1,
                    .writeable = 1,
                    .reset = 0x7,
                },
                { .sentinal = 1, },
            },
        },
        {
            .name = "RNG1",
            .description = "Channel 1 Range",
            .offset = 0x10,
            .fields = {
                {
                    .name = "RNG",
                    .start = 0,
                    .stop = 31,
                    .description = "Channel 1 range",
                    .readable = 1,
                    .writeable = 1,
                    .reset = 0x20,
                },
                { .sentinal = 1, },
            },
        },
        {
            .name = "DAT1",
            .description = "Channel 1 Data",
            .offset = 0x14,
            .fields = {
                {
                    .name = "DAT",
                    .start = 0,
                    .stop = 31,
                    .description = "Channel 1 data",
                    .readable = 1,
                    .writeable = 1,
                },
                { .sentinal = 1, },
            },
        },
        {
            .name = "FIF",
            .description = "PWM fifo register",
            .offset = 0x18,
            .fields = {
                {
                    .name = "FIFO",
                    .start = 0,
                    .stop = 31,
                    .description = "Channel FIFO input",
                    .readable = 1,
                    .writeable = 1,
                },
                { .sentinal = 1, },
            },
        },
        {
            .name = "RNG2",
            .description = "Channel 2 Range",
            .offset = 0x20,
            .fields = {
                {
                    .name = "RNG",
                    .start = 0,
                    .stop = 31,
                    .description = "Channel 2 range",
                    .readable = 1,
                    .writeable = 1,
                    .reset = 0x20,
                },
                { .sentinal = 1, },
            },
        },
        {
            .name = "DAT2",
            .description = "Channel 2 Data",
            .offset = 0x24,
            .fields = {
                {
                    .name = "DAT",
                    .start = 0,
                    .stop = 31,
                    .description = "Channel 2 data",
                    .readable = 1,
                    .writeable = 1,
                },
                { .sentinal = 1, },
            },
        },
        { .sentinal = 1, },
    },
};

struct context {
    struct regs *gpio;
    struct regs *pwm;
    struct regs *clk;
};


// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(ctx, g) *(ctx->gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(ctx, g) *(ctx->gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(ctx, g, a) *(ctx->gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))

#define GPIO_SET *(ctx->gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(ctx->gpio+10) // clears bits which are 1 ignores bits which are 0

// map 4k register memory for direct access from user space and return a user space pointer to it
static volatile unsigned long *map_register_memory(int base)
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
	return (volatile unsigned long *)map;
}

// set up a memory regions to access GPIO, PWM and the clock manager
void map_registers(struct context *ctx)
{
//	ctx->gpio = map_register_memory(GPIO_BASE);
	ctx->pwm->mem = map_register_memory(PWM_BASE);
	ctx->clk->mem = map_register_memory(CLOCK_BASE);
}


static int dump_regs(struct regs *regs) {
    int reg_num;
    int field_num;
    for (reg_num=0; !regs->regs[reg_num].sentinal; reg_num++) {
        struct reg *reg = &regs->regs[reg_num];
        unsigned long reg_val = regs->mem[reg->offset/sizeof(long)];


        printf("%s.%s - %s\n", regs->name, reg->name, reg->description);
        for (field_num=0; !reg->fields[field_num].sentinal; field_num++) {
            struct bits *field = &reg->fields[field_num];
            int field_val = (reg_val>>field->start) &
                (((1<<(field->stop-field->start+1)))-1);
            if (field->reserved)
                printf("\t   (Bits %ld - %ld Reserved)\n", field->start, field->stop);
            else {
                if (field_val > 256) {
                    printf("\t%6s: 0x%08x    %s\n", field->name, field_val,
                            field->description);
                }
                else {
                    printf("\t%6s: %-10d    %s\n", field->name, field_val,
                            field->description);
                }
            }
        }
        printf("\n");
    }
    printf("\n");
    return 0;
}

static int dump_pwm_regs(struct context *ctx) {
    return dump_regs(ctx->pwm);
}

static int dump_clk_regs(struct context *ctx) {
    return dump_regs(ctx->clk);
}


static int set_reg(struct context *ctx, char *desc) {
    struct regs *regs = NULL;
    if (!strncmp(desc, ctx->pwm->name, strlen(ctx->pwm->name)))
        regs = ctx->pwm;
    else if (!strncmp(desc, ctx->clk->name, strlen(ctx->clk->name)))
        regs = ctx->clk;
    else {
        errno = EINVAL;
        printf("Unrecognized register block\n");
        return -1;
    }

    int reg_num, field_num;
    desc += strlen(regs->name)+1;

    /* Look for the correct register */
    for (reg_num=0; !regs->regs[reg_num].sentinal; reg_num++) {
        struct reg *reg = &regs->regs[reg_num];
        if (reg->name && !strncmp(desc, reg->name, strlen(reg->name))) {

            /* Register found */
            unsigned long reg_val = regs->mem[reg->offset/sizeof(long)];
            desc += strlen(reg->name)+1;

            /* Look for the correct field */
            for (field_num=0; !reg->fields[field_num].sentinal; field_num++) {
                struct bits *field = &reg->fields[field_num];
                if (field->name && !strncmp(desc, field->name, strlen(field->name))) {
                    /* Field found */
                    unsigned long newval;
                    desc += strlen(field->name)+1;
                    newval = strtoul(desc, NULL, 0);

                    /* Limit the new value to the correct size */
                    int field_val = newval &
                        (((1<<(field->stop-field->start+1)))-1);
                    /* Move it to the correct bit offset */
                    field_val <<= field->start;

                    /* Clear out the old value */
                    reg_val &=
                        ~((((1<<(field->stop-field->start+1)))-1)<<field->start);

                    reg_val |= field_val;
                    reg_val |= reg->required;
                    printf("Setting field %s.%s.%s to %ld\n",
                            regs->name, reg->name, field->name, newval);
                    regs->mem[reg->offset/sizeof(long)] = reg_val;
                    return 0;
                }
            }
            printf("Unknown field\n");
            errno = EINVAL;
            return -1;
        }
    }
    printf("Unknown register\n");
    errno = EINVAL;
    return -1;
}

int main(int argc, char **argv) { 
    int ch;
    struct context ctx;

    ctx.pwm = &pwm_regs;
    ctx.clk = &clk_regs;
	map_registers(&ctx);

    while ((ch = getopt(argc, argv, "dw:")) != -1) {
        switch (ch) {
        case 'd':
            dump_pwm_regs(&ctx);
            dump_clk_regs(&ctx);
            break;

        case 'w':
            if (set_reg(&ctx, optarg))
                perror("Unable to set register");
            break;

        default:
            printf("Usage: %s [-d]\n", argv[0]);
        }
    }

    argc += optind;
	
	//SET_GPIO_ALT((&ctx), 18, 0);

	return 0;
}
