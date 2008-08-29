/*
 * Beagle board emulation. http://beagleboard.org/
 * 
 * Copyright (C) 2008 yajin
 * Written by yajin <yajin@vm-kernel.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include "qemu-common.h"
#include "sysemu.h"
#include "omap.h"
#include "arm-misc.h"
#include "irq.h"
#include "console.h"
#include "boards.h"
#include "i2c.h"
#include "devices.h"
#include "flash.h"
#include "hw.h"


/* Beagle board support */
struct beagle_s {
    struct omap_mpu_state_s *cpu;
};



static struct arm_boot_info beagle_binfo = {
    .ram_size = 0x08000000,
};
static void beagle_init(ram_addr_t ram_size, int vga_ram_size,
                const char *boot_device, DisplayState *ds,
                const char *kernel_filename, const char *kernel_cmdline,
                const char *initrd_filename, const char *cpu_model)
{
    struct beagle_s *s = (struct beagle_s *) qemu_mallocz(sizeof(*s));
    int sdram_size = beagle_binfo.ram_size;

   	if (ram_size < sdram_size +  OMAP353X_SRAM_SIZE) {
        fprintf(stderr, "This architecture uses %i bytes of memory\n",
                        sdram_size + OMAP353X_SRAM_SIZE);
        exit(1);
    }
   	s->cpu = omap3530_mpu_init(sdram_size, NULL, NULL);

    
}



QEMUMachine beagle_machine = {
    "beagle",
    "Beagle board (OMAP3530)",
    beagle_init,
    (0x08000000 +  OMAP353X_SRAM_SIZE) | RAMSIZE_FIXED,
};



