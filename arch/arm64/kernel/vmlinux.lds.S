OUTPUT_ARCH(aarch64)
ENTRY(_text)

SECTIONS
{
	. = 0x40080000;
	_text = .;

	.text : {
		*(.text*)
	}

	. = ALIGN(8);
	.data : {
		*(.data*)
	}

	. = ALIGN(8);
	.bss : {
		*(.bss*)
	}
}
