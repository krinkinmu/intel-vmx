OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(start32)

SECTIONS
{
	. = 1M + SIZEOF_HEADERS;

	kernel_phys_begin = .;
	text_phys_begin = .;
	.bootstrap : { *(.bootstrap) }
	.text : { *(.text) *(.text.*) }
	.rodata : { *(.rodata) *(.rodata.*) }
	.data : { *(.data) *(.data.*) }

	.tdata : {
		percpu_phys_begin = .;
		*(.tdata) *(.tdata.*)
	}
	data_phys_end = .;

	.tbss : {
		*(.tbss) *(.tbss.)
		percpu_phys_end = .;
	}

	.bss : { *(.bss) *(.bss.*) }
	bss_phys_end = .;
	kernel_phys_end = .;
}
