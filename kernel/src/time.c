#include <time.h>
#include <acpi.h>
#include <debug.h>
#include <stdint.h>
#include <limits.h>
#include <balloc.h>
#include <scheduler.h>
#include <apic.h>
#include <ints.h>
#include <cpu.h>
#include <rcu.h>


struct acpi_hpet {
	ACPI_TABLE_HEADER hdr;
	uint32_t hpet_block_id;
	ACPI_GENERIC_ADDRESS addr;
	uint8_t hpet_seq_num;
	uint16_t min_clock_ticks;
	uint8_t attrs;
} __attribute__((packed));

struct hpet_block {
	unsigned long long period;
	unsigned long long overflow;
	uintptr_t addr;
	int cnt_width;
};

#define TN_CONF(x)	(0x100 + 0x20 * (x))
#define TN_CMP(x)	(0x108 + 0x20 * (x))
#define HPET_LEVEL	(1ul << 1)
#define HPET_EDGE	(0ul << 1)
#define HPET_ENABLE_INT	(1ul << 2)
#define HPET_PERIODIC	(1ul << 3)
#define HPET_SET_CMP	(1ul << 6)
#define HPET_ROUTE(x)	(((unsigned long)(x) & 31ul) << 9)
#define HPET_CAP	0x00
#define HPET_CONF	0x10
#define HPET_COUNT	0xf0

static struct hpet_block *hpet_block;
static int hpet_blocks;
static struct hpet_block *default_block;
static int default_timer, default_timer_level;
static unsigned long long ticks_elapsed;
static unsigned long apic_ticks;


unsigned long long current_time(void)
{
	return ticks_elapsed;
}

static unsigned long hpet_read(uintptr_t addr, int reg)
{
	BUG_ON(reg % 4 != 0);
	volatile uint32_t *ptr = (volatile uint32_t *)(addr + reg);

	return *ptr;
}

static unsigned long long hpet_read64(uintptr_t addr, int reg)
{
	BUG_ON(reg % 8 != 0);
	volatile uint64_t *ptr = (volatile uint64_t *)(addr + reg);

	return *ptr;
}

static void hpet_write(uintptr_t addr, int reg, unsigned long val)
{
	BUG_ON(reg % 4 != 0);
	volatile uint32_t *ptr = (volatile uint32_t *)(addr + reg);

	*ptr = val;
}

static void hpet_write64(uintptr_t addr, int reg, unsigned long long val)
{
	BUG_ON(reg % 8 != 0);
	volatile uint64_t *ptr = (volatile uint64_t *)(addr + reg);

	*ptr = val;
}

static void hpet_realloc(int new_size)
{
	struct hpet_block *old_hpet_block = hpet_block;
	const size_t size = sizeof(*old_hpet_block) * new_size;
	const size_t old_size = sizeof(*old_hpet_block) * hpet_blocks;

	hpet_block = (struct hpet_block *)balloc_alloc(size,
				/* from = */0x1000, /* to = */UINTPTR_MAX);
	BUG_ON((uintptr_t)hpet_block == UINTPTR_MAX);

	memcpy(hpet_block, old_hpet_block, old_size < size ? old_size : size);
	balloc_free((uintptr_t)old_hpet_block,
				(uintptr_t)old_hpet_block + old_size);
}

static unsigned long long hpet_overflow(int width, unsigned long long period)
{
	BUG_ON(width != 32 && width != 64);

	/* overflow period in us is calculated as:
	 * 2^(width - 1) * period * 10^(-18) * 10^6
	 * = 2^(width - 1) * period * (2*5)^(-12)
	 * = 2^(width - 1) * period / (2*5)^12
	 * = 2^(width - 13) * period / 5^12 */
	if (width == 64)
		return 9223372ull * period;
	return (period << 19) / 244140625;
}

static int hpet_timers(const struct hpet_block *block)
{
	return 1 + ((hpet_read(block->addr, HPET_CAP) >> 8) & 0xf);
}

static void hpet_stop(struct hpet_block *block)
{
	hpet_write(block->addr, HPET_CONF, 0);
}

static void hpet_start(struct hpet_block *block)
{
	hpet_write(block->addr, HPET_CONF, 1);
}

static void hpet_block_reset(struct hpet_block *block)
{
	const int timers = hpet_timers(block); 

	hpet_stop(block);
	hpet_write64(block->addr, HPET_COUNT, 0);

	for (int i = 0; i != timers; ++i)
		/* TODO: this is bad, since we don't check whether zero
		   Tn_INT_ROUTE_CNF is valid or not. */
		hpet_write(block->addr, TN_CONF(i), 0);
}

static void hpet_block_setup(uintptr_t addr)
{
	const unsigned long long gen_cap = hpet_read64(addr, HPET_CAP);
	const unsigned long period = gen_cap >> 32;
	const int cnt_width = (gen_cap ^ (1 << 13)) ? 64 : 32;
	const unsigned long long overflow = hpet_overflow(cnt_width, period);

	hpet_realloc(hpet_blocks + 1);

	struct hpet_block *block = &hpet_block[hpet_blocks++];

	printf("HPET block counter width %d, period %lu fs\n",
				cnt_width, period);
	block->period = period;
	block->overflow = overflow;
	block->addr = addr;
	block->cnt_width = cnt_width;
	hpet_block_reset(block);
}

static struct hpet_block *hpet_default_find(void)
{
	for (int i = 0; i != hpet_blocks; ++i) {
		struct hpet_block *hpet = &hpet_block[i];

		if (hpet->cnt_width == 64)
			return hpet;
	}

	return hpet_block;
}

static void hpet_setup_periodic(struct hpet_block *block, int timer,
			unsigned long long ticks, int irq)
{
	const unsigned long long conf =
				hpet_read64(block->addr, TN_CONF(timer));
	const unsigned long route_cap = conf >> 32;
	int gsi = -1;

	for (int i = 0; i != 32; ++i) {
		if ((1ul << i) & route_cap) {
			gsi = i;
			break;
		}
	}
	BUG_ON(gsi == -1);

	unsigned long new_hpet_conf = HPET_ENABLE_INT | HPET_SET_CMP |
				HPET_PERIODIC | HPET_ROUTE(gsi);
	struct irq_info info;

	memset(&info, 0, sizeof(info));
	info.apic = io_apic_find(gsi);
	info.pin = io_apic_pin(info.apic, gsi);

	if (gsi < 16) {
		new_hpet_conf |= HPET_EDGE;
		default_timer_level = 0;
		info.trigger = INT_EDGE;
		info.polarity = INT_ACTIVE_HIGH;
	} else {
		new_hpet_conf |= HPET_LEVEL;
		default_timer_level = 1;
		info.trigger = INT_LEVEL;
		info.polarity = INT_ACTIVE_LOW;
	}

	register_irq(irq, &info);

	hpet_write(block->addr, TN_CONF(timer), new_hpet_conf);
	hpet_write64(block->addr, TN_CMP(timer),
				hpet_read64(block->addr, HPET_COUNT) + ticks);
	hpet_write64(block->addr, TN_CMP(timer), ticks);
}

static void hpet_default_timer_handler(void)
{
	if (default_timer_level) {
		const unsigned long status =
					hpet_read(default_block->addr, 0x20);
		if ((status & (1ul << default_timer)) == 0)
			return;
	}

	++ticks_elapsed;

	if (default_timer_level)
		hpet_write(default_block->addr, 0x20, (1ul << default_timer));
}

static unsigned long long hpet_ticks(const struct hpet_block *block,
			unsigned long ms)
{
	const unsigned long long period = block->period;

	return (((unsigned long long)ms << 12) * 244140625ull) / period;
}

static void hpet_setup_default(void)
{
	BUG_ON(!(default_block = hpet_default_find()));

	const uintptr_t addr = default_block->addr;
	const int timers = hpet_timers(default_block);
	int width;

	default_timer = -1;

	for (int i = 0; i != timers; ++i) {
		const unsigned long conf = hpet_read(addr, TN_CONF(i));

		if (conf & (1 << 4)) {
			default_timer = i;
			width = (conf & (1 << 5)) ? 64 : 32;
			break;
		}
	}

	BUG_ON(default_timer == -1);

	const unsigned long long ticks = hpet_ticks(default_block, TIMER_TICK);
	BUG_ON(width == 32 && ticks >= (1ull << 32));

	hpet_setup_periodic(default_block, default_timer, ticks, TIMER_IRQ);
	register_irq_handler(TIMER_IRQ, &hpet_default_timer_handler);
	activate_irq(TIMER_IRQ);
	hpet_start(default_block);
}

static void hpet_setup(void)
{
	ACPI_TABLE_HEADER *table;
	ACPI_STATUS status;
	int i = 1;

	while ((status = AcpiGetTable("HPET", i++, &table)) != AE_NOT_FOUND) {
		if (ACPI_FAILURE(status))
			BUG("Failed to get HPET table\n");

		const struct acpi_hpet *hpet = (const struct acpi_hpet *)table;

		BUG_ON(hpet->addr.SpaceId != 0);
		hpet_block_setup(hpet->addr.Address);
	}
	hpet_setup_default();
}

static void wait_loop(const struct hpet_block *timer, unsigned long long from,
			unsigned long long until)
{
	unsigned long long clk;

	if (until > from) {
		while (1) {
			clk = hpet_read64(timer->addr, HPET_COUNT);
			if (clk > until || clk < from)
				break;
			cpu_relax();
		}
	} else {
		while (1) {
			clk = hpet_read64(timer->addr, HPET_COUNT);
			if (clk < from && clk > until)
				break;
			cpu_relax();
		}
	}
}

static void __udelay(const struct hpet_block *timer, unsigned long usec)
{
	const unsigned long long mask = (1ull << (timer->cnt_width - 1));
	unsigned long long start = hpet_read64(timer->addr, HPET_COUNT);

	while (usec >= timer->overflow) {
		wait_loop(timer, start, start ^ mask);
		start ^= mask;
		usec -= timer->overflow;
	}

	if (!usec)
		return;

	const unsigned long long period = timer->period;
	const unsigned long long clks = (usec * 1000000000ull) / period;

	wait_loop(timer, start, start + clks);
}

void udelay(unsigned long usec)
{
	__udelay(default_block, usec);
}

static void apic_timer_calibrate(void)
{
	const unsigned long lvt = IRQ_VECTOR(TIMER_LOCAL_IRQ)
				| APIC_TIMER_ONE_SHOT;

	local_apic_write(APIC_TIMER_INIT, 0);
	local_apic_write(APIC_TIMER_DIV, 11);
	local_apic_write(APIC_TIMER_LVT, lvt);
	local_apic_write(APIC_TIMER_INIT, 0xfffffffful);

	udelay(TIMER_TICK * 1000);

	const unsigned long count = local_apic_read(APIC_TIMER_COUNT);

	local_apic_write(APIC_TIMER_INIT, 0);
	apic_ticks = 0xfffffffful - count;
}

static void apic_timer_handler(void)
{
	schedule();
	rcu_report_qs();
	rcu_tick();
}

static void apic_timer_ints_setup(void)
{
	struct irq_info info;

	memset(&info, 0, sizeof(info));
	register_irq(TIMER_LOCAL_IRQ, &info);
	register_irq_handler(TIMER_LOCAL_IRQ, &apic_timer_handler); 	
}

static void apic_timer_setup(void)
{
	apic_timer_calibrate();
	apic_timer_ints_setup();
}

static void apic_timer_start(void)
{
	const unsigned long lvt = IRQ_VECTOR(TIMER_LOCAL_IRQ)
				| APIC_TIMER_PERIODIC;

	local_apic_write(APIC_TIMER_LVT, lvt);
	local_apic_write(APIC_TIMER_INIT, apic_ticks);
}

void time_setup(void)
{
	hpet_setup();
	apic_timer_setup();
}

void time_cpu_setup(void)
{
	apic_timer_start();
}
