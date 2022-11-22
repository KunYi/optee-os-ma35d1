// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2015, Linaro Limited
 * Copyright (C) 2020, Nuvoton Technology Corporation
 * Copyright (c) 2022, UWINGS Technologies.
 */

#include <console.h>
#include <drivers/nuvoton_uart.h>
#include <kernel/boot.h>
#include <kernel/panic.h>
#include <kernel/pm.h>
#include <mm/core_memprot.h>
#include <platform_config.h>
#include <stdint.h>
#include <tee/entry_std.h>
#include <tee/entry_fast.h>
#include <io.h>
#include <tsi_cmd.h>

#define LOAD_TSI_PATCH

register_phys_mem_pgdir(MEM_AREA_IO_NSEC, UART0_BASE, UART0_REG_SIZE);
register_phys_mem_pgdir(MEM_AREA_IO_SEC, SYS_BASE, SYS_REG_SIZE);
register_phys_mem_pgdir(MEM_AREA_IO_SEC, TRNG_BASE, TRNG_REG_SIZE);
register_phys_mem_pgdir(MEM_AREA_IO_SEC, KS_BASE, KS_REG_SIZE);
register_phys_mem_pgdir(MEM_AREA_IO_SEC, OTP_BASE, OTP_REG_SIZE);
register_phys_mem_pgdir(MEM_AREA_IO_SEC, CRYPTO_BASE, CRYPTO_REG_SIZE);
register_phys_mem_pgdir(MEM_AREA_IO_SEC, TSI_BASE, TSI_REG_SIZE);
register_phys_mem_pgdir(MEM_AREA_IO_SEC, WHC1_BASE, WHC1_REG_SIZE);

#ifdef LOAD_TSI_PATCH
#include "tsi_patch.c"
#endif

static struct nuvoton_uart_data console_data;

void console_init(void)
{
	nuvoton_uart_init(&console_data, CONSOLE_UART_BASE);
	register_serial_console(&console_data.chip);
}

int ma35d1_tsi_init(void)
{
	vaddr_t sys_base = core_mmu_get_va(SYS_BASE, MEM_AREA_IO_SEC, SYS_REG_SIZE);
	int  ret;

	if (!(io_read32(sys_base + SYS_CHIPCFG) & TSIEN)) {
		/*
		 * TSI enabled. Invoke TSI command and return here.
		 */
		uint32_t  version_code;

		/* enable WHC1 clock */
		io_write32(sys_base + 0x208,
			   io_read32(sys_base + 0x208) | (1 << 5));

		ret = TSI_Get_Version(&version_code);
		if (ret == ST_SUCCESS)
			return 0;   /* TSI is ready. */

		while (1) {
			ret = TSI_Get_Version(&version_code);
			if (ret == ST_SUCCESS) {
				EMSG("TSI F/W version: %x\n", version_code);
				break;
			}
			if (ret == ST_WAIT_TSI_SYNC) {
				EMSG("Wait TSI_Sync.\n");
				TSI_Sync();
			}
		}
	}

#ifdef LOAD_TSI_PATCH
	ret = TSI_Load_Image((uint32_t)virt_to_phys((void *)tsi_patch_image),
				   	sizeof(tsi_patch_image));
	if (ret == 0)
		EMSG("Load TSI image successful.\n");
	else
		EMSG("Load TSI image failed!! %d\n", ret);
#endif
	return 0;
}
