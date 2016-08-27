#include <time.h>
#include <acpi.h>
#include <debug.h>
#include <stdint.h>
#include <limits.h>
#include <balloc.h>
#include <cpu.h>


struct acpi_hpet {
	ACPI_TABLE_HEADER hdr;
	uint32_t hpet_block_id;
	ACPI_GENERIC_ADDRESS addr;
	uint8_t hpet_seq_num;
	uint16_t min_clock_ticks;
	uint8_t attrs;
} __attribute__((packed));

struct timer {
	unsigned long long period;
	unsigned long long overflow;
	uintptr_t addr;
	int cnt_width;
	int cmp_width;
	int index;
};

static struct timer *timers;
static int timers_size;
static struct timer *delay_timer;

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

/*
static void hpet_write64(uintptr_t addr, int reg, unsigned long long val)
{
	BUG_ON(reg % 8 != 0);
	volatile uint64_t *ptr = (volatile uint64_t *)(addr + reg);

	*ptr = val;
}
*/

static void hpet_realloc(int new_size)
{
	if (new_size <= timers_size)
		return;

	struct timer *old_timers = timers;

	timers = (struct timer *)balloc_alloc(sizeof(*old_timers) * new_size,
				/* from = */0x1000, /* to = */UINTPTR_MAX);
	BUG_ON((uintptr_t)timers == UINTPTR_MAX);

	memcpy(timers, old_timers, sizeof(*old_timers) * timers_size);
	balloc_free((uintptr_t)old_timers,
				(uintptr_t)(old_timers + timers_size));
	timers_size = new_size;
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
	const int count = 1 + ((gen_cap >> 8) & 0xf);
	const int index = timers_size;

	hpet_realloc(timers_size + count);

	for (int i = 0; i != count; ++i) {
		struct timer *timer = &timers[index + i];
		const int conf_reg = 0x100 + 0x20 * i;
		const unsigned long timer_conf = hpet_read(addr, conf_reg);
		const int cmp_width = timer_conf & (1 << 5) ? 64 : 32;

		if (timer_conf & (1 << 2))
			hpet_write(addr, conf_reg, timer_conf & ~(1ul << 2));

		printf("HPET timer width %d, period %lu fs\n",
					cmp_width, period);
		timer->period = period;
		timer->overflow = overflow;
		timer->addr = addr;
		timer->cnt_width = cnt_width;
		timer->cmp_width = cmp_width;
		timer->index = i;
	}

	hpet_write(addr, 0x10, 1);
	if (!delay_timer && cnt_width == 64)
		delay_timer = &timers[index];
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
	BUG_ON(!delay_timer);
}

static void wait_loop(const struct timer *timer, unsigned long long from,
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

static void __udelay(const struct timer *timer, unsigned long usec)
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
	__udelay(delay_timer, usec);
}

void time_setup(void)
{
	hpet_setup();
}
