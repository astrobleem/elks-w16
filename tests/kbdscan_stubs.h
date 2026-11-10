/* stand-ins for the ELKS kernel headers kbd-scancode.c includes (host test only: kbdscan_test.c) */
struct pt_regs { int unused; };
typedef unsigned int flag_t;
typedef unsigned short seg_t;
struct timer_list { unsigned long tl_expires; void (*tl_function)(int); int tl_data; };
static unsigned long jiffies;
#define HZ 100
static void add_timer(struct timer_list *t) { (void)t; }
static int del_timer(struct timer_list *t) { (void)t; return 0; }
#define save_flags(x) ((x) = 0)
#define restore_flags(x) ((void)(x))
#define clr_irq() ((void)0)
int stub_inb(int port);
#define inb_p(p) stub_inb(p)
#define inb(p) stub_inb(p)
#define outb_p(v, p) ((void)(v), (void)(p))
#define outb(v, p) ((void)(v), (void)(p))
#define KBD_IO 0x60
#define KBD_CTL 0x61
#define KB_STATUS 0x64
#define KB_IN_FULL 0x02
#define KEYBD 0x60
#define LED_CODE 0xED
#define CAP_KBD_LEDS 0x04
static int sys_caps;
static void ctrl_alt_del(void) { }
#define printk printf
#define INITPROC
#define INT_GENERIC 0
static int request_irq(int irq, void (*h)(int, struct pt_regs *), int flags) { (void)irq; (void)h; (void)flags; return 0; }
typedef unsigned short dev_t;
#define KBD_IRQ 1
#define CONFIG_KEYMAP_US 1
#define panic(...) ((void)0)
