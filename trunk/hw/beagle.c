/*
 * Beagle board emulation. http://beagleboard.org/
 * 
 * Copyright (C) 2008 yajin(yajin@vm-kernel.org)
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

#define BEAGLE_NAND_CS			0

#ifdef DEBUG_BEAGLE
#define BEAGLE_DEBUG(x)    do {  printf x ; } while(0)
#else
#define BEAGLE_DEBUG(x)    do {   } while(0)
#endif
/* Beagle board support */
struct beagle_s {
    struct omap_mpu_state_s *cpu;
    
	target_phys_addr_t nand_base;
    struct nand_flash_s *nand;
};



static struct arm_boot_info beagle_binfo = {
    .ram_size = 0x08000000,
};


static uint32_t beagle_nand_read16(void *opaque, target_phys_addr_t addr)
{
	struct beagle_s *s = (struct beagle_s *) opaque;
	target_phys_addr_t offset;
    offset = addr-s->nand_base;
    BEAGLE_DEBUG(("beagle_nand_read16 offset %x\n",offset));

	switch (offset)
	{
		case 0x0: /*NAND_COMMAND*/
		case 0x4: /*NAND_ADDRESS*/
			omap_badwidth_read16(s,addr);
			break;
		case 0x8: /*NAND_DATA*/
			return nand_read_data16(s->nand);
			break;
		default:
			omap_badwidth_read16(s,addr);
			break;
	}
    return 0;
}

static void beagle_nand_write16(void *opaque, target_phys_addr_t addr,
                uint32_t value)
{
	struct beagle_s *s = (struct beagle_s *) opaque;
	target_phys_addr_t offset;
    offset = addr- s->nand_base;
    switch (offset)
	{
		case 0x0: /*NAND_COMMAND*/
			nand_write_command(s->nand,value);
			break;
		case 0x4: /*NAND_ADDRESS*/
			nand_write_address(s->nand,value);
			break;
		case 0x8: /*NAND_DATA*/
			nand_write_data16(s->nand,value);
			break;
		default:
			omap_badwidth_write16(s,addr,value);
			break;
	}
}


CPUReadMemoryFunc *beagle_nand_readfn[] = {
        beagle_nand_read16,
        beagle_nand_read16,
        omap_badwidth_read32,
};
    CPUWriteMemoryFunc *beagle_nand_writefn[] = {
        beagle_nand_write16,
        beagle_nand_write16,
        omap_badwidth_write32,
};

/*
void beagle_nand_base_update(void *opaque, target_phys_addr_t new)
{
    struct beagle_s *s = (struct beagle_s *) opaque;
    int iomemtype;

    printf("beagle_nand_base_update base %x \n",new);

    s->nand_base = new;

	iomemtype = cpu_register_io_memory(0, beagle_nand_readfn,
                    beagle_nand_writefn, s);
    cpu_register_physical_memory(s->nand_base, 0xc, iomemtype);
}

void beagle_nand_base_unmap(void *opaque)
{
    struct beagle_s *s = (struct beagle_s *) opaque;
    cpu_register_physical_memory(s->nand_base,0xc, IO_MEM_UNASSIGNED);
}
*/
static void beagle_nand_setup(struct beagle_s *s)
{
	int iomemtype;
	
	/*MT29F2G16ABC*/
	s->nand = nand_init(NAND_MFR_MICRON,0xba);
	/*wp=1, no write protect!!! */
	nand_set_wp(s->nand, 1);
	s->nand_base = 0x6e00007c ;

	iomemtype = cpu_register_io_memory(0, beagle_nand_readfn,
                    beagle_nand_writefn, s);
    cpu_register_physical_memory(s->nand_base, 0xc, iomemtype);

}

static int beagle_nand_read_page(struct beagle_s *s,uint8_t *buf, uint16_t page_addr)
{
	uint16_t *p;
	int i;

	p=(uint16_t *)buf;

	/*send command 0x0*/
	beagle_nand_write16(s,s->nand_base,0);
	/*send page address */
	beagle_nand_write16(s,s->nand_base+4,page_addr&0xff);
	beagle_nand_write16(s,s->nand_base+4,(page_addr>>8)&0x7);
	beagle_nand_write16(s,s->nand_base+4,(page_addr>>11)&0xff);
	beagle_nand_write16(s,s->nand_base+4,(page_addr>>19)&0xff);
	beagle_nand_write16(s,s->nand_base+4,(page_addr>>27)&0xff);
	/*send command 0x30*/
	beagle_nand_write16(s,s->nand_base,0x30);

	for (i=0;i<0x800/2;i++)
	{
		*p++ = beagle_nand_read16(s,s->nand_base+8);
	}
	return 1;
}


/*read the xloader from NAND Flash into internal RAM*/
 void beagle_rom_emu(struct beagle_s *s)
{
	uint32_t	loadaddr, len;
	uint8_t nand_page[0x800],*load_dest;
	uint32_t nand_pages,i;

	/* The first two words(8 bytes) in first nand flash page have special meaning.
		First word:x-loader len
		Second word: x-load address in internal ram */
	beagle_nand_read_page(s,nand_page,0);
	len = *((uint32_t*)nand_page);
	loadaddr =  *((uint32_t*)(nand_page+4));

	/*put the first page into internal ram*/
	load_dest = phys_ram_base +beagle_binfo.ram_size;
	load_dest += loadaddr-OMAP3_SRAM_BASE;
	
	BEAGLE_DEBUG(("load_dest %x phys_ram_base %x \n",load_dest,phys_ram_base));
	
	memcpy(load_dest,nand_page+8,0x800-8);
	load_dest += 0x800-8;

	nand_pages = len/0x800;
	if (len%0x800!=0)
		nand_pages++;

	for (i=1;i<nand_pages;i++)
	{
		beagle_nand_read_page(s,nand_page,i*0x800);
		memcpy(load_dest,nand_page,0x800);
		load_dest += 0x800;
	}
		s->cpu->env->regs[15] = loadaddr;
}

static void beagle_init(ram_addr_t ram_size, int vga_ram_size,
                const char *boot_device, DisplayState *ds,
                const char *kernel_filename, const char *kernel_cmdline,
                const char *initrd_filename, const char *cpu_model)
{
    struct beagle_s *s = (struct beagle_s *) qemu_mallocz(sizeof(*s));
    int sdram_size = beagle_binfo.ram_size;

   	if (ram_size < sdram_size +  OMAP3530_SRAM_SIZE) {
        fprintf(stderr, "This architecture uses %i bytes of memory\n",
                        sdram_size + OMAP3530_SRAM_SIZE);
        exit(1);
    }
   	s->cpu = omap3530_mpu_init(sdram_size, NULL, NULL);
   	beagle_nand_setup(s);
   	beagle_rom_emu(s);
}



QEMUMachine beagle_machine = {
    .name = "beagle",
    .desc =     "Beagle board (OMAP3530)",
    .init =     beagle_init,
    .ram_require =     (0x08000000 +  OMAP3530_SRAM_SIZE) | RAMSIZE_FIXED,
};

