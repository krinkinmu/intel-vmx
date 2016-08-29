#include <time.h>
#include <acpi.h>
#include <debug.h>
#include <stdint.h>
#include <limits.h>
#include <balloc.h>
#include <ints.h>
#include <cpu.h>


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

static struct hpet_block *hpet_block;
static int hpet_blocks;
static struct hpet_block *default_block;
static int default_timer, default_timer_level;
static unsigned long long ticks_elapsed;


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

static void hpet_block_setup(uintptr_t addr)
{
	const unsigned long long gen_cap = hpet_read64(addr, 0);
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

	if (gsi < 16) {
		new_hpet_conf |= HPET_EDGE;
		default_timer_level = 0;
		register_irq(irq, gsi, INT_EDGE, INT_ACTIVE_HIGH);
	} else {
		new_hpet_conf |= HPET_LEVEL;
		default_timer_level = 1;
		register_irq(irq, gsi, INT_LEVEL, INT_ACTIVE_LOW);
	}

	hpet_write(block->addr, TN_CONF(timer), new_hpet_conf);
	hpet_write64(block->addr, TN_CMP(timer),
				hpet_read64(block->addr, 0xf0) + ticks);
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

static int hpet_timers(const struct hpet_block *block)
{
	return 1 + ((hpet_read(block->addr, 0) >> 8) & 0xf);
}

static void hpet_stop(struct hpet_block *block)
{
	hpet_write(block->addr, 0x10, 0);
}

static void hpet_start(struct hpet_block *block)
{
	hpet_write(block->addr, 0x10, 1);
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

	hpet_stop(default_block);
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
			clk = hpet_read64(timer->addr, 0xf0);
			if (clk > until || clk < from)
				break;
			cpu_relax();
		}
	} else {
		while (1) {
			clk = hpet_read64(timer->addr, 0xf0);
			if (clk < from && clk > until)
				break;
			cpu_relax();
		}
	}
}

static void __udelay(const struct hpet_block *timer, unsigned long usec)
{
	const unsigned long long mask = (1ull << (timer->cnt_width - 1));
	unsigned long long start = hpet_read64(timer->addr, 0xf0);

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

void time_setup(void)
{
	hpet_setup();
}
