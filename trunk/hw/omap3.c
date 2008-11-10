/*
 * TI OMAP3 processors emulation.
 *
 * Copyright (C) 2008 yajin <yajin@vm-kernel.org>
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
 
#include "hw.h"
#include "arm-misc.h"
#include "omap.h"
#include "sysemu.h"
#include "qemu-timer.h"
#include "qemu-char.h"
#include "flash.h"
#include "soc_dma.h"
#include "audio/audio.h"

#define OMAP3_DEBUG(x)    do {  printf x ; } while(0)

static uint32_t omap3_l4ta_read(void *opaque, target_phys_addr_t addr)
{
    struct omap_target_agent_s *s = (struct omap_target_agent_s *) opaque;
    target_phys_addr_t reg = addr - s->base;

    switch (reg) {
    //case 0x00:	/* COMPONENT */
    //    return s->component;
    case 0x20:	/* AGENT_CONTROL */
        return s->control;
        
   case 0x24:	/* AGENT_CONTROL_H */
        return s->control_h;

    case 0x28:	/* AGENT_STATUS */
        return s->status;
    }

    OMAP_BAD_REG(addr);
    return 0;
}

static void omap3_l4ta_write(void *opaque, target_phys_addr_t addr,
                uint32_t value)
{
    struct omap_target_agent_s *s = (struct omap_target_agent_s *) opaque;
    target_phys_addr_t reg = addr - s->base;

    switch (reg) {
    //case 0x00:	/* COMPONENT */
    //	OMAP_RO_REG(addr);
     //	break;
    case 0x20:	/* AGENT_CONTROL */
        s->control = value & 0x00000700;
        break;
    case 0x24:  	/* AGENT_CONTROL_H */
    	s->control_h = value & 0x100;
    	break;
    case 0x28:	/* AGENT_STATUS */
        if (value & 0x100)
        	s->status &= ~0x100;				/* REQ_TIMEOUT */
        break;
    default:
        OMAP_BAD_REG(addr);
    }
}

static CPUReadMemoryFunc *omap3_l4ta_readfn[] = {
    omap_badwidth_read16,
    omap3_l4ta_read,
    omap_badwidth_read16,
};

static CPUWriteMemoryFunc *omap3_l4ta_writefn[] = {
    omap_badwidth_write32,
    omap_badwidth_write32,
    omap3_l4ta_write,
};



static struct omap_l4_region_s omap3_l4_region[ ] = 
{
	[  1] = { 0x40800,  0x800, 32          }, /* Initiator agent */
	[  2] = { 0x41000, 0x1000, 32          }, /* Link agent */
	[  0] = { 0x40000,  0x800, 32          }, /* Address and protection */
	
 	[  3] = { 0x002000, 0x1000, 32 | 16 | 8 }, /* System Control module */
 	[  4] = { 0x003000, 0x1000, 32 | 16 | 8 }, /* L4TA1 */
 	
 	[  5] = { 0x004000, 0x2000, 32},              /*CM Region A*/
 	[  6] = { 0x006000, 0x0800, 32},              /*CM Region B*/
 	[  7] = { 0x007000, 0x1000, 32 | 16 | 8},    /*  L4TA2 */

	[  8] = { 0x050000, 0x0400, 32},          /*Display subsystem top*/
	[  9] = { 0x050400, 0x0400, 32},          /*Display controller*/
	[ 10] = { 0x050800, 0x0400, 32},          /*RFBI*/
	[ 11] = { 0x050c00, 0x0400, 32},          /*Video encoder*/
	[ 12] = { 0x051000, 0x1000, 32 | 16 | 8},    /*  L4TA3 */

	[ 13] = { 0x056000, 0x1000, 32},                 /*  SDMA */
	[ 14] = { 0x057000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[ 15] = { 0x060000, 0x1000, 16 | 8},          /*  I2C3 */
	[ 16] = { 0x061000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[ 17] = { 0x062000, 0x1000, 32},                 /*  USBTLL */
	[ 18] = { 0x063000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[ 19] = { 0x064000, 0x1000, 32},                 /* HS USB HOST */
	[ 20] = { 0x065000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[ 21] = { 0x06a000, 0x1000, 32 | 16 | 8},    /* UART1 */
	[ 22] = { 0x06b000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[ 23] = { 0x06c000, 0x1000, 32 | 16 | 8},    /* UART2 */
	[ 24] = { 0x06d000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[ 25] = { 0x070000, 0x1000,   16 | 8},    /*  I2C1 */
	[ 26] = { 0x071000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[ 27] = { 0x072000, 0x1000, 16 | 8},    /*  I2C1 */
	[ 28] = { 0x073000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[ 29] = { 0x074000, 0x1000, 32 },               /*  mcbsp1 */
	[ 30] = { 0x075000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[ 31] = { 0x086000, 0x1000, 32 | 16 },         /*  GPTIMER10 */
	[ 32] = { 0x087000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */
	
	[ 33] = { 0x088000, 0x1000, 32 | 16 },         /*  GPTIMER11 */
	[ 34] = { 0x089000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[ 35] = { 0x094000, 0x1000, 32 | 16 | 8 },    /*  MAILBOX */
	[ 36] = { 0x095000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[ 37] = { 0x096000, 0x1000, 32 },               /*  mcbsp5 */
	[ 38] = { 0x097000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[ 39] = { 0x098000, 0x1000, 32 | 16 | 8 },    /*  MCSPI1 */
	[ 40] = { 0x099000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[ 41] = { 0x09a000, 0x1000, 32 | 16 | 8 },    /*  MCSPI2 */
	[ 42] = { 0x09b000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[ 43] = { 0x09c000, 0x1000, 32  },              /*  MMC/SD/SDIO */
	[ 44] = { 0x09d000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[ 45] = { 0x09e000, 0x1000, 32  },              /*  MS-PRO */
	[ 46] = { 0x09f000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[ 47] = { 0x09e000, 0x1000, 32  },              /*  MS-PRO */
	[ 48] = { 0x09f000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[ 49] = { 0x0ab000, 0x1000, 32  },              /*  HS USB OTG */
	[ 50] = { 0x0ac000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[ 51] = { 0x0ad000, 0x1000, 32  },              /*  MMC/SD/SDIO3 */
	[ 52] = { 0x0ae000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[ 53] = { 0x0b0000, 0x1000, 32 |16 },         /*  MG */
	[ 54] = { 0x0b1000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */
	
	[ 55] = { 0x0b2000, 0x1000, 32},                 /*  HDQ/1-WIRE */
	[ 56] = { 0x0b3000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[ 57] = { 0x0b4000, 0x1000, 32},                 /*  MMC/SD/SDIO2 */
	[ 58] = { 0x0b5000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[ 59] = { 0x0b6000, 0x1000, 32},                 /*  icr mpu  */
	[ 60] = { 0x0b7000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[ 61] = { 0x0b8000, 0x1000, 32 | 16 | 8},    /*  MCSPI3  */
	[ 62] = { 0x0b9000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[ 63] = { 0x0ba000, 0x1000, 32 | 16 | 8},    /*  MCSPI4  */
	[ 64] = { 0x0bb000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[ 65] = { 0x0bc000, 0x4000, 32 | 16 | 8},    /*  CAMERA ISP  */
	[ 66] = { 0x0c0000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[ 67] = { 0x0c7000, 0x1000, 32 | 16  },       /*  MODEM  */
	[ 68] = { 0x0c8000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[ 69] = { 0x0c9000, 0x1000, 32 | 16 |8 },   /*  SR1  */
	[ 70] = { 0x0ca000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[ 71] = { 0x0cb000, 0x1000, 32 | 16 |8 },   /*  SR2  */
	[ 72] = { 0x0cc000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[ 73] = { 0x0cd000, 0x1000, 32  },               /*  ICR MODEM  */
	[ 74] = { 0x0ce000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[ 75] = { 0x30a000, 0x1000, 32 | 16 | 8  },  /*  CONTRL MODULE ID  */
	[ 76] = { 0x30b000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	/*L4 WAKEUP MEMORY SPACE*/
	[ 77] = { 0x306000, 0x2000, 32   },              /* PRM REGION A  */
	[ 78] = { 0x308000, 0x800, 32   },                /* PRM REGION B  */
	[ 79] = { 0x309000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[ 80] = { 0x310000, 0x1000, 32 | 16 | 8 },     /* GPIO1  */
	[ 81] = { 0x311000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[ 82] = { 0x314000, 0x1000, 32 | 16    },      /* WDTIMER2  */
	[ 83] = { 0x315000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[ 84] = { 0x318000, 0x1000, 32 | 16    },      /* GPTIMER1  */
	[ 85] = { 0x319000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */
	
 	[ 86] = { 0x320000, 0x1000, 32 | 16     }, /* 32K Timer */
 	[ 87] = { 0x321000, 0x1000, 32 | 16 | 8 }, /*  L4TA2 */

	[ 88] = { 0x328000, 0x800, 32 | 16 | 8    },      /* AP  */
	[ 89] = { 0x328800, 0x800, 32 | 16 | 8   },      /* IP  */
	[ 90] = { 0x329000, 0x1000, 32 | 16 | 8   },      /* LA  */
	[ 91] = { 0x32a000, 0x800, 32 | 16  | 8  },      /* LA  */
	[ 92] = { 0x340000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	/*L4 Peripheral MEMORY SPACE*/
	[ 93] = { 0x1000000, 0x800, 32 | 16 | 8    },      /* AP  */
	[ 94] = { 0x1000800, 0x800, 32 | 16 | 8   },      /* IP  */
	[ 95] = { 0x1001000, 0x1000, 32 | 16 | 8   },      /* LA  */

	[ 96] = { 0x1020000, 0x1000, 32 | 16 | 8},    /* UART3 */
	[ 97] = { 0x1021000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[ 98] = { 0x1022000, 0x1000, 32 },                /* MCBSP 2 */
	[ 99] = { 0x1023000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[100] = { 0x1024000, 0x1000, 32 },                /* MCBSP 3 */
	[101] = { 0x1025000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[102] = { 0x1026000, 0x1000, 32 },                /* MCBSP 4 */
	[103] = { 0x1027000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[104] = { 0x1028000, 0x1000, 32 },                /* MCBSP 2 (sidetone) */
	[105] = { 0x1029000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[106] = { 0x102a000, 0x1000, 32 },                /* MCBSP 3 (sidetone) */
	[107] = { 0x102b000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[108] = { 0x1030000, 0x1000, 32 | 16    },      /* WDTIMER3  */
	[109] = { 0x1031000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[110] = { 0x1032000, 0x1000, 32 | 16    },      /* GPTIMER2 */
	[111] = { 0x1033000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[112] = { 0x1034000, 0x1000, 32 | 16    },      /* GPTIMER3 */
	[113] = { 0x1035000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[114] = { 0x1036000, 0x1000, 32 | 16    },      /* GPTIMER4 */
	[115] = { 0x1037000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[116] = { 0x1038000, 0x1000, 32 | 16    },      /* GPTIMER5 */
	[117] = { 0x1039000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[118] = { 0x103a000, 0x1000, 32 | 16    },      /* GPTIMER6 */
	[119] = { 0x103b000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[120] = { 0x103c000, 0x1000, 32 | 16    },      /* GPTIMER7 */
	[121] = { 0x103d000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[122] = { 0x103e000, 0x1000, 32 | 16    },      /* GPTIMER8 */
	[123] = { 0x103f000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[124] = { 0x1040000, 0x1000, 32 | 16    },      /* GPTIMER9 */
	[125] = { 0x1041000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[126] = { 0x1050000, 0x1000, 32 | 16 |8   },  /* GPIO2 */
	[127] = { 0x1051000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[128] = { 0x1052000, 0x1000, 32 | 16 |8   },  /* GPIO3 */
	[129] = { 0x1053000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[130] = { 0x1054000, 0x1000, 32 | 16 |8   },  /* GPIO4 */
	[131] = { 0x1055000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[132] = { 0x1056000, 0x1000, 32 | 16 |8   },  /* GPIO5 */
	[133] = { 0x1057000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[134] = { 0x1058000, 0x1000, 32 | 16 |8   },  /* GPIO6 */
	[135] = { 0x1059000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	/*L4 Emulation MEMORY SPACE*/
	[136] = { 0xc006000, 0x800, 32 | 16 | 8    },      /* AP  */
	[137] = { 0xc006800, 0x800, 32 | 16 | 8   },      /* IP  */
	[138] = { 0xc007000, 0x1000, 32 | 16 | 8   },      /* LA  */
	[139] = { 0xc008000, 0x800, 32 | 16 | 8   },      /* DAP  */

	[140] = { 0xc010000, 0x8000, 32 | 16 |8   },  /* MPU Emulation */
	[141] = { 0xc018000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[142] = { 0xc019000, 0x8000, 32    },             /* TPIU */
	[143] = { 0xc01a000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[144] = { 0xc01b000, 0x8000, 32    },             /* ETB */
	[145] = { 0xc01c000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[146] = { 0xc01d000, 0x8000, 32    },             /* DAOCTL*/
	[147] = { 0xc01e000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[148] = { 0xc706000, 0x2000, 32    },             /* PR Region A*/
	[149] = { 0xc706800, 0x800, 32    },               /* PR Region B*/
	[150] = { 0xc709000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[151] = { 0xc710000, 0x1000, 32 | 16 | 8    }, /* GPIO1*/
	[152] = { 0xc711000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[153] = { 0xc714000, 0x1000, 32 | 16     },    /* WDTIMER 2*/
	[154] = { 0xc715000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[155] = { 0xc718000, 0x1000, 32 | 16 |8  },   /* GPTIMER 1*/
	[156] = { 0xc719000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[157] = { 0xc720000, 0x1000, 32 | 16    },   /* 32k timer*/
	[158] = { 0xc721000, 0x1000, 32 | 16 | 8},    /*  L4TA4 */

	[159] = { 0xc728000, 0x800, 32 | 16 | 8    },      /* AP  */
	[160] = { 0xc728800, 0x800, 32 | 16 | 8   },      /* IP  */
	[161] = { 0xc729000, 0x1000, 32 | 16 | 8   },      /* LA  */
	[162] = { 0xc72a000, 0x800, 32 | 16 | 8   },      /* DAP  */

};
static struct omap_l4_agent_info_s omap3_l4_agent_info[ ] = 
{
	{0, 0, 2, 1 }, 			/* System Control module */
	{1, 5, 3, 2 }, 			/* CM */
	{2, 77, 3, 2 }, 			/* PRM */
	{3, 82, 2, 1 }, 			/* WDTIMER 2 */
	{4, 3, 2, 1 }, 			   /* SCM */
	{5, 84, 2, 1 }, 			/* GP TIMER 1 */
	{6, 86, 2, 1 }, 			/* 32K Sync timer */
	{7, 21, 2, 1 }, 			/* uart1 */
	{8, 23, 2, 1 }, 			/* uart2 */
	{9, 96, 2, 1 }, 			/* uart3 */
};

struct omap_target_agent_s *omap3_l4ta_get(struct omap_l4_s *bus, int cs)
{
    int i, iomemtype;
    struct omap_target_agent_s *ta = 0;
    struct omap_l4_agent_info_s *info = 0;

    for (i = 0; i < bus->ta_num; i ++)
        if (omap3_l4_agent_info[i].ta == cs) {
            ta = &bus->ta[i];
            info = &omap3_l4_agent_info[i];
            break;
        }
    if (!ta) {
        fprintf(stderr, "%s: bad target agent (%i)\n", __FUNCTION__, cs);
        exit(-1);
    }

    ta->bus = bus;
    ta->start = &omap3_l4_region[info->region];
    ta->regions = info->regions;

    ta->component = ('Q' << 24) | ('E' << 16) | ('M' << 8) | ('U' << 0);
    ta->status = 0x00000000;
    ta->control = 0x00000200;	/* XXX 01000200 for L4TAO */

    iomemtype = l4_register_io_memory(0, omap3_l4ta_readfn,
                    omap3_l4ta_writefn, ta);
    ta->base = omap_l4_attach(ta, info->ta_region, iomemtype);

    return ta;
}


struct omap3_prm_s {
	target_phys_addr_t base;
    qemu_irq irq[3];
    struct omap_mpu_state_s *mpu;
    
	/*IVA2_PRM Register*/
	uint32_t rm_rstctrl_iva2;               /*0x4830 6050*/
	uint32_t rm_rstst_iva2;                 /*0x4830 6058*/
	uint32_t pm_wkdep_iva2;              /*0x4830 60C8*/
	uint32_t pm_pwstctrl_iva2;            /*0x4830 60E0*/
	uint32_t pm_pwstst_iva2;               /*0x4830 60E4*/
	uint32_t pm_prepwstst_iva2;         /*0x4830 60E8*/
	uint32_t pm_irqstatus_iva2;           /*0x4830 60F8*/
	uint32_t pm_irqenable_iva2;          /*0x4830 60FC*/

	/*OCP_System_Reg_PRM Registerr*/
	uint32_t prm_revision;                 /*0x4830 6804*/
	uint32_t prm_sysconfig;                /*0x4830 6814*/
	uint32_t prm_irqstatus_mpu;       /*0x4830 6818*/
	uint32_t prm_irqenable_mpu;       /*0x4830 681c */

	/*MPU_PRM Register*/
	uint32_t rm_rstst_mpu;                        /*0x4830 6958*/
	uint32_t pm_wkdep_mpu;                     /*0x4830 69c8*/
	uint32_t pm_evgenctrl_mpu;                /*0x4830 69d4*/
	uint32_t pm_evgenontim_mpu;            /*0x4830 69d8*/
	uint32_t pm_evgenofftim_mpu;            /*0x4830 69dc*/
	uint32_t pm_pwstctrl_mpu;                  /*0x4830 69e0*/
	uint32_t pm_pwstst_mpu;                    /*0x4830 69e4*/
	uint32_t pm_perpwstst_mpu;               /*0x4830 69e8*/

	/*CORE_PRM Register*/
	uint32_t rm_rstst_core;                       /*0x4830 6a58*/
	uint32_t pm_wken1_core;                   /*0x4830 6aa0*/
	uint32_t pm_mpugrpsel1_core;           /*0x4830 6aa4*/
	uint32_t pm_iva2grpsel1_core;           /*0x4830 6aa8*/
	uint32_t pm_wkst1_core;                      /*0x4830 6ab0*/
	uint32_t pm_wkst3_core;                       /*0x4830 6ab8*/
	uint32_t pm_pwstctrl_core;                    /*0x4830 6ae0*/
	uint32_t pm_pwstst_core;                        /*0x4830 6ae4*/
	uint32_t pm_prepwstst_core;                 /*0x4830 6ae8*/
	uint32_t pm_wken3_core;                       /*0x4830 6af0*/
	uint32_t pm_iva2grpsel3_core;              /*0x4830 6af4*/
	uint32_t pm_mpugrpsel3_core;               /*0x4830 6af8*/

	/*SGX_PRM Register*/
	uint32_t rm_rstst_sgx;                              /*0x4830 6b58*/
	uint32_t pm_wkdep_sgx;                           /*0x4830 6bc8*/
	uint32_t pm_pwstctrl_sgx;                         /*0x4830 6be0*/
	uint32_t pm_pwstst_sgx;                            /*0x4830 6be4*/
	uint32_t pm_prepwstst_sgx;                      /*0x4830 6be8*/

	/*WKUP_PRM Register*/
	uint32_t pm_wken_wkup;                          /*0x4830 6ca0*/
	uint32_t pm_mpugrpsel_wkup;                 /*0x4830 6ca4*/
	uint32_t pm_iva2grpsel_wkup;                 /*0x4830 6ca8*/
	uint32_t pm_wkst_wkup;                           /*0x4830 6cb0*/

	/*Clock_Control_Reg_PRM Register*/
	uint32_t prm_clksel;                                   /*0x4830 6D40*/
	uint32_t prm_clkout_ctrl;                           /*0x4830 6D70*/

	/*DSS_PRM Register*/
	uint32_t rm_rstst_dss;                               /*0x4830 6e58*/
	uint32_t pm_wken_dss;                              /*0x4830 6ea0*/
	uint32_t pm_wkdep_dss;                           /*0x4830 6ec8*/  
	uint32_t pm_pwstctrl_dss;                        /*0x4830 6ee0*/ 
	uint32_t pm_pwstst_dss;                           /*0x4830 6ee4*/
	uint32_t pm_prepwstst_dss;                      /*0x4830 6ee8*/

	/*CAM_PRM Register*/
	uint32_t rm_rstst_cam;                               /*0x4830 6f58*/
	uint32_t pm_wken_cam;                              /*0x4830 6fc8*/
	uint32_t pm_pwstctrl_cam;                        /*0x4830 6fe0*/ 
	uint32_t pm_pwstst_cam;                           /*0x4830 6fe4*/
	uint32_t pm_prepwstst_cam;                      /*0x4830 6fe8*/

	/*PER_PRM Register*/
	uint32_t rm_rstst_per;                                 /*0x4830 7058*/
	uint32_t pm_wken_per;                                /*0x4830 70a0*/
	uint32_t pm_mpugrpsel_per;                       /*0x4830 70a4*/
	uint32_t pm_iva2grpsel_per;                       /*0x4830 70a8*/
	uint32_t pm_wkst_per;                                 /*0x4830 70b0*/
	uint32_t pm_wkdep_per;                              /*0x4830 70c8*/
	uint32_t pm_pwstctrl_per;                           /*0x4830 70e0*/
	uint32_t pm_pwstst_per;                              /*0x4830 70e4*/
	uint32_t pm_perpwstst_per;                         /*0x4830 70e8*/

	/*EMU_PRM Register*/
	uint32_t rm_rstst_emu;                                 /*0x4830 7158*/
	uint32_t pm_pwstst_emu;                              /*0x4830 71e4*/

	/*Global_Reg_PRM Register*/
	uint32_t prm_vc_smps_sa;                           /*0x4830 7220*/
	uint32_t prm_vc_smps_vol_ra;                    /*0x4830 7224*/
	uint32_t prm_vc_smps_cmd_ra;                  /*0x4830 7228*/
	uint32_t prm_vc_cmd_val_0;                       /*0x4830 722c*/
	uint32_t prm_vc_cmd_val_1;                       /*0x4830 7230*/
	uint32_t prm_vc_hc_conf;                            /*0x4830 7234*/
	uint32_t prm_vc_i2c_cfg;                             /*0x4830 7238*/
	uint32_t prm_vc_bypass_val;                      /*0x4830 723c*/
	uint32_t prm_rstctrl;                                    /*0x4830 7250*/
	uint32_t prm_rsttimer;                                 /*0x4830 7254*/
	uint32_t prm_rstst;                                       /*0x4830 7258*/
	uint32_t prm_voltctrl;                                  /*0x4830 7260*/
	uint32_t prm_sram_pcharge;                      /*0x4830 7264*/
	uint32_t prm_clksrc_ctrl;                             /*0x4830 7270*/
	uint32_t prm_obs;                                        /*0x4830 7280*/
	uint32_t prm_voltsetup1;                             /*0x4830 7290*/
	uint32_t prm_voltoffset;                              /*0x4830 7294*/
	uint32_t prm_clksetup;                                /*0x4830 7298*/
	uint32_t prm_polctrl;                                    /*0x4830 729c*/
	uint32_t prm_voltsetup2;                             /*0x4830 72a0*/

	/*NEON_PRM Register*/
	uint32_t rm_rstst_neon;                              /*0x4830 7358*/
	uint32_t pm_wkdep_neon;                          /*0x4830 73c8*/
	uint32_t pm_pwstctrl_neon;                        /*0x4830 73e0*/
	uint32_t pm_pwstst_neon;                          /*0x4830 73e4*/
	uint32_t pm_prepwstst_neon;                     /*0x4830 73e8*/

	/*USBHOST_PRM Register*/
	uint32_t rm_rstst_usbhost;                           /*0x4830 7458*/
	uint32_t rm_wken_usbhost;                          /*0x4830 74a0*/
	uint32_t rm_mpugrpsel_usbhost;                 /*0x4830 74a4*/
	uint32_t rm_iva2grpsel_usbhost;                 /*0x4830 74a8*/
	uint32_t rm_wkst_usbhost;                           /*0x4830 74b0*/
	uint32_t rm_wkdep_usbhost;                       /*0x4830 74c8*/
	uint32_t rm_pwstctrl_usbhost;                    /*0x4830 74e0*/
	uint32_t rm_pwstst_usbhost;                      /*0x4830 74e4*/
	uint32_t rm_prepwstst_usbhost;                /*0x4830 74e8*/
	
};

static void omap3_prm_reset(struct omap3_prm_s *s)
{
	s->rm_rstctrl_iva2 = 0x7;
	s->rm_rstst_iva2 = 0x1;
	s->pm_wkdep_iva2 = 0xb3;
	s->pm_pwstctrl_iva2 = 0xff0f07;
	s->pm_pwstst_iva2 = 0xff7;
	s->pm_prepwstst_iva2 = 0x0;
	s->pm_irqstatus_iva2 = 0x0;
	s->pm_irqenable_iva2 = 0x0;

	s->prm_revision = 0x10;
	s->prm_sysconfig = 0x1;
	s->prm_irqstatus_mpu = 0x0;
	s->prm_irqenable_mpu = 0x0;
	
	s->rm_rstst_mpu = 0x1;
	s->pm_wkdep_mpu = 0xa5;
	s->pm_evgenctrl_mpu = 0x12;
	s->pm_evgenontim_mpu = 0x0;
	s->pm_evgenofftim_mpu = 0x0;
	s->pm_pwstctrl_mpu = 0x30107;
	s->pm_pwstst_mpu = 0xc7;
	s->pm_pwstst_mpu = 0x0;

	s->rm_rstst_core = 0x1;
	s->pm_wken1_core = 0xc33ffe18;
	s->pm_mpugrpsel1_core = 0xc33ffe18;
	s->pm_iva2grpsel1_core = 0xc33ffe18;
	s->pm_wkst1_core = 0x0;
	s->pm_wkst3_core = 0x0;
	s->pm_pwstctrl_core = 0xf0307;
	s->pm_pwstst_core = 0xf7;
	s->pm_prepwstst_core = 0x0;
	s->pm_wken3_core = 0x4;
	s->pm_iva2grpsel3_core = 0x4;
	s->pm_mpugrpsel3_core = 0x4;

	s->rm_rstst_sgx = 0x1;
	s->pm_wkdep_sgx = 0x16;
	s->pm_pwstctrl_sgx = 0x30107;
	s->pm_pwstst_sgx = 0x3;
	s->pm_prepwstst_sgx = 0x0;

	s->pm_wken_wkup = 0x3cb;
	s->pm_mpugrpsel_wkup = 0x3cb;
	s->pm_iva2grpsel_wkup = 0x0;
	s->pm_wkst_wkup = 0x0;

	s->prm_clksel = 0x4;
	s->prm_clkout_ctrl = 0x80;

	s->rm_rstst_dss = 0x1;
	s->pm_wken_dss = 0x1;
	s->pm_wkdep_dss = 0x16;
	s->pm_pwstctrl_dss = 0x30107;
	s->pm_pwstst_dss = 0x3;
	s->pm_prepwstst_dss = 0x0;

	s->rm_rstst_cam = 0x1;
	s->pm_wken_cam = 0x16;
	s->pm_pwstctrl_cam = 0x30107;
	s->pm_pwstst_cam = 0x3;
	s->pm_prepwstst_cam = 0x0;

	s->rm_rstst_per = 0x1;
	s->pm_wken_per = 0x3efff;
	s->pm_mpugrpsel_per = 0x3efff;
	s->pm_iva2grpsel_per = 0x3efff;
	s->pm_wkst_per = 0x0;
	s->pm_wkdep_per = 0x17;
	s->pm_pwstctrl_per = 0x30107;
	s->pm_pwstst_per = 0x7;
	s->pm_perpwstst_per = 0x0;

	s->rm_rstst_emu = 0x1;
	s->pm_pwstst_emu = 0x13;
	
	s->prm_vc_smps_sa = 0x0;
	s->prm_vc_smps_vol_ra = 0x0;
	s->prm_vc_smps_cmd_ra = 0x0;
	s->prm_vc_cmd_val_0 = 0x0;
	s->prm_vc_cmd_val_1 = 0x0;
	s->prm_vc_hc_conf = 0x0;
	s->prm_vc_i2c_cfg = 0x18;
	s->prm_vc_bypass_val = 0x0;
	s->prm_rstctrl = 0x0;
	s->prm_rsttimer = 0x1006;
	s->prm_rstst = 0x1;
	s->prm_voltctrl = 0x0;
	s->prm_sram_pcharge = 0x50;
	s->prm_clksrc_ctrl = 0x43;
	s->prm_obs = 0x0;
	s->prm_voltsetup1 = 0x0;
	s->prm_voltoffset = 0x0;
	s->prm_clksetup = 0x0;
	s->prm_polctrl = 0xa;
	s->prm_voltsetup2 = 0x0;
	
	s->rm_rstst_neon = 0x1;
	s->pm_wkdep_neon = 0x2;
	s->pm_pwstctrl_neon = 0x7;
	s->pm_pwstst_neon = 0x3;
	s->pm_prepwstst_neon = 0x0;

	s->rm_rstst_usbhost = 0x1;
	s->rm_wken_usbhost = 0x1;
	s->rm_mpugrpsel_usbhost = 0x1;
	s->rm_iva2grpsel_usbhost = 0x1;
	s->rm_wkst_usbhost = 0x0;
	s->rm_wkdep_usbhost = 0x17;
	s->rm_pwstctrl_usbhost = 0x30107;
	s->rm_pwstst_usbhost = 0x3;
	s->rm_prepwstst_usbhost = 0x0;
	
}

static uint32_t omap3_prm_read(void *opaque, target_phys_addr_t addr)
{
    struct omap3_prm_s *s = (struct omap3_prm_s *) opaque;
    int offset = addr - s->base;
    //uint32_t ret;

     switch (offset) 
     {
     	case 0xd40:
     		return s->prm_clksel ;
     	case 0x1270:
     		return s->prm_clksrc_ctrl;
     	default:
     		exit(-1);
     }
}

static inline void omap3_prm_clksrc_ctrl_update(struct omap3_prm_s *s,
                uint32_t value)
{
	if ((value & 0xd0)==0x40)
		omap_clk_setrate(omap_findclk(s->mpu, "omap3_sys_clk"),1,1);
	else if ((value & 0xd0)==0x80)
		omap_clk_setrate(omap_findclk(s->mpu, "omap3_sys_clk"),2,1);
	//OMAP3_DEBUG(("omap3_sys_clk %d \n",omap_clk_getrate(omap_findclk(s->mpu, "omap3_sys_clk"))));
}
static void omap3_prm_write(void *opaque, target_phys_addr_t addr,
                uint32_t value)
{
    struct omap3_prm_s *s = (struct omap3_prm_s *) opaque;
    int offset = addr - s->base;

    switch (offset) 
     {
     	case 0xd40:
     		s->prm_clksel = value & 0x7;
     		break;
     	case 0x1270:
     		s->prm_clksrc_ctrl = value &(0xd8);
     		omap3_prm_clksrc_ctrl_update(s,s->prm_clksrc_ctrl);
     		//OMAP3_DEBUG(("s->prm_clksrc_ctrl  %x \n",s->prm_clksrc_ctrl ));
     		//OMAP3_DEBUG(("RATE %d \n",omap_clk_getrate(omap_findclk(s->mpu, "omap3_sys_clk"))));
     		break;
     	
     	default:
     		printf("omap3_prm_write addr %x value %x \n",addr,value);
     		exit(-1);
     }
}


static CPUReadMemoryFunc *omap3_prm_readfn[] = {
    omap_badwidth_read32,
    omap_badwidth_read32,
    omap3_prm_read,
};

static CPUWriteMemoryFunc *omap3_prm_writefn[] = {
    omap_badwidth_write32,
    omap_badwidth_write32,
    omap3_prm_write,
};

struct omap3_prm_s *omap3_prm_init(struct omap_target_agent_s *ta,
                qemu_irq mpu_int, qemu_irq dsp_int, qemu_irq iva_int,
                struct omap_mpu_state_s *mpu)
{
    int iomemtype;
    struct omap3_prm_s *s = (struct omap3_prm_s *)qemu_mallocz(sizeof(*s));

    s->irq[0] = mpu_int;
    s->irq[1] = dsp_int;
    s->irq[2] = iva_int;
    s->mpu = mpu;
    omap3_prm_reset(s);

    iomemtype = l4_register_io_memory(0, omap3_prm_readfn,
                    omap3_prm_writefn, s);
    s->base = omap_l4_attach(ta, 0, iomemtype);
    omap_l4_attach(ta, 1, iomemtype);

    return s;
}


struct omap3_cm_s {
	target_phys_addr_t base;
    qemu_irq irq[3];
    struct omap_mpu_state_s *mpu;

	/*IVA2_CM Register*/
    uint32_t cm_fclken_iva2;                    /*0x4800 4000*/
    uint32_t cm_clken_pll_iva2;               /*0x4800 4004*/
    uint32_t cm_idlest_iva2;                    /*0x4800 4020*/
    uint32_t cm_idlest_pll_iva2;              /*0x4800 4024*/
    uint32_t cm_autoidle_pll_iva2;         /*0x4800 4034*/
    uint32_t cm_clksel1_pll_iva2;           /*0x4800 4040*/
    uint32_t cm_clksel2_pll_iva2;          /*0x4800 4044*/
    uint32_t cm_clkstctrl_iva2;              /*0x4800 4048*/
    uint32_t cm_clkstst_iva2;                 /*0x4800 404c*/

	/*OCP_System_Reg_CM*/
	uint32_t cm_revision;                         /*0x4800 4800*/
	uint32_t cm_sysconfig;                        /*0x4800 4810*/

	/*MPU_CM Register*/
	uint32_t cm_clken_pll_mpu;                /*0x4800 4904*/
	uint32_t cm_idlest_mpu;                      /*0x4800 4920*/
	uint32_t cm_idlest_pll_mpu;                 /*0x4800 4924*/
	uint32_t cm_autoidle_pll_mpu;            /*0x4800 4934*/
	uint32_t cm_clksel1_pll_mpu;               /*0x4800 4940*/
	uint32_t cm_clksel2_pll_mpu;             /*0x4800 4944*/
	uint32_t cm_clkstctrl_mpu;                /*0x4800 4948*/
	uint32_t cm_clkstst_mpu;                  /*0x4800 494c*/

	/*CORE_CM Register*/ 
	uint32_t cm_fclken1_core;                 /*0x4800 4a00*/
	uint32_t cm_fclken3_core;                 /*0x4800 4a08*/
	uint32_t cm_iclken1_core;                 /*0x4800 4a10*/
	uint32_t cm_iclken2_core;                 /*0x4800 4a14*/
	uint32_t cm_iclken3_core;                 /*0x4800 4a18*/
	uint32_t cm_idlest1_core;                  /*0x4800 4a20*/
	uint32_t cm_idlest2_core;                  /*0x4800 4a24*/
	uint32_t cm_idlest3_core;                 /*0x4800 4a28*/
	uint32_t cm_autoidle1_core;            /*0x4800 4a30*/
	uint32_t cm_autoidle2_core;             /*0x4800 4a34*/
	uint32_t cm_autoidle3_core;             /*0x4800 4a38*/         
	uint32_t cm_clksel_core;                    /*0x4800 4a40*/
	uint32_t cm_clkstctrl_core;                 /*0x4800 4a48*/
	uint32_t cm_clkstst_core;                   /*0x4800 4a4c*/

	/*SGX_CM Register*/
	uint32_t cm_fclken_sgx;                    /*0x4800 4b00*/
	uint32_t cm_iclken_sgx;                    /*0x4800 4b10*/
	uint32_t cm_idlest_sgx;                     /*0x4800 4b20*/
	uint32_t cm_clksel_sgx;                    /*0x4800 4b40*/
	uint32_t cm_sleepdep_sgx;               /*0x4800 4b44*/
	uint32_t cm_clkstctrl_sgx;                /*0x4800 4b48*/
	uint32_t cm_clkstst_sgx;                   /*0x4800 4b4c*/

	/*WKUP_CM Register*/
	uint32_t cm_fclken_wkup;                /*0x4800 4c00*/
	uint32_t cm_iclken_wkup;                /*0x4800 4c10*/
	uint32_t cm_idlest_wkup;                 /*0x4800 4c20*/
	uint32_t cm_autoidle_wkup;             /*0x4800 4c30*/
	uint32_t cm_clksel_wkup;                /*0x4800 4c40*/

	/*Clock_Control_Reg_CM Register*/
	uint32_t cm_clken_pll;                       /*0x4800 4d00*/
	uint32_t cm_clken2_pll;                     /*0x4800 4d04*/
	uint32_t cm_idlest_ckgen;                 /*0x4800 4d20*/
	uint32_t cm_idlest2_ckgen;               /*0x4800 4d24*/
	uint32_t cm_autoidle_pll;                   /*0x4800 4d30*/
	uint32_t cm_autoidle2_pll;                 /*0x4800 4d34*/
	uint32_t cm_clksel1_pll;                    /*0x4800 4d40*/           
	uint32_t cm_clksel2_pll;                     /*0x4800 4d44*/
	uint32_t cm_clksel3_pll;                     /*0x4800 4d48*/
	uint32_t cm_clksel4_pll;                     /*0x4800 4d4c*/
	uint32_t cm_clksel5_pll;                     /*0x4800 4d50*/
	uint32_t cm_clkout_ctrl;                     /*0x4800 4d70*/
	
	/*DSS_CM Register*/
	uint32_t cm_fclken_dss;                     /*0x4800 4e00*/
	uint32_t cm_iclken_dss;                     /*0x4800 4e10*/
	uint32_t cm_idlest_dss;                      /*0x4800 4e20*/
	uint32_t cm_autoidle_dss;                  /*0x4800 4e30*/
	uint32_t cm_clksel_dss;                       /*0x4800 4e40*/
	uint32_t cm_sleepdep_dss;                  /*0x4800 4e44*/
	uint32_t cm_clkstctrl_dss;                    /*0x4800 4e48*/
	uint32_t cm_clkstst_dss;                      /*0x4800 4e4c*/


	/*CAM_CM Register*/
	uint32_t cm_fclken_cam;                        /*0x4800 4f00*/
	uint32_t cm_iclken_cam;                        /*0x4800 4f10*/
	uint32_t cm_idlest_cam;                          /*0x4800 4f20*/
	uint32_t cm_autoidle_cam;                      /*0x4800 4f30*/
	uint32_t cm_clksel_cam;                           /*0x4800 4f40*/
	uint32_t cm_sleepdep_cam;                      /*0x4800 4f44*/
	uint32_t cm_clkstctrl_cam;                        /*0x4800 4f48*/
	uint32_t cm_clkstst_cam;                           /*0x4800 4f4c*/

	/*PER_CM Register*/
	uint32_t cm_fclken_per;                        /*0x4800 5000*/
	uint32_t cm_iclken_per;                        /*0x4800 5010*/
	uint32_t cm_idlest_per;                          /*0x4800 5020*/
	uint32_t cm_autoidle_per;                      /*0x4800 5030*/
	uint32_t cm_clksel_per;                           /*0x4800 5040*/
	uint32_t cm_sleepdep_per;                      /*0x4800 5044*/
	uint32_t cm_clkstctrl_per;                        /*0x4800 5048*/
	uint32_t cm_clkstst_per;                           /*0x4800 504c*/

	/*EMU_CM Register*/
	uint32_t cm_clksel1_emu;                        /*0x4800 5140*/
	uint32_t cm_clkstctrl_emu;                       /*0x4800 5148*/
	uint32_t cm_clkstst_emu;                          /*0x4800 514c*/
	uint32_t cm_clksel2_emu;                         /*0x4800 5150*/
	uint32_t cm_clksel3_emu;                         /*0x4800 5154*/

	/*Global_Reg_CM Register*/
	uint32_t cm_polctrl;                                /*0x4800 529c*/

	/*NEON_CM Register*/
	uint32_t cm_idlest_neon;                         /*0x4800 5320*/
	uint32_t cm_clkstctrl_neon;                      /*0x4800 5348*/

	/*USBHOST_CM Register*/
	uint32_t cm_fclken_usbhost;                        /*0x4800 5400*/
	uint32_t cm_iclken_usbhost;                        /*0x4800 5410*/
	uint32_t cm_idlest_usbhost;                          /*0x4800 5420*/
	uint32_t cm_autoidle_usbhost;                      /*0x4800 5430*/
	uint32_t cm_sleepdep_usbhost;                      /*0x4800 5444*/
	uint32_t cm_clkstctrl_usbhost;                        /*0x4800 5448*/
	uint32_t cm_clkstst_usbhost;                            /*0x4800 544c*/

};

/*
static inline void omap3_cm_fclken_wkup_update(struct omap3_cm_s *s,
                uint32_t value)
{
	
	if (value & 0x28)
     	omap_clk_onoff(omap_findclk(s->mpu,"omap3_wkup_32k_fclk"), 1);
    else
    	omap_clk_onoff(omap_findclk(s->mpu,"omap3_wkup_32k_fclk"), 0);

    if (value &0x1)
    	omap_clk_onoff(omap_findclk(s->mpu,"omap3_gp1_fclk"), 1);
    else
    	omap_clk_onoff(omap_findclk(s->mpu,"omap3_gp1_fclk"), 0);

}
static inline void omap3_cm_iclken_wkup_update(struct omap3_cm_s *s,
                uint32_t value)
{
	
	if (value & 0x3f)
     	omap_clk_onoff(omap_findclk(s->mpu,"omap3_wkup_l4_iclk"), 1);
    else
    	omap_clk_onoff(omap_findclk(s->mpu,"omap3_wkup_l4_iclk"), 0);

}
*/
static inline void omap3_cm_clksel_wkup_update(struct omap3_cm_s *s,
                uint32_t value)
{
	 omap_clk gp1_fclk = omap_findclk(s->mpu, "omap3_gp1_fclk");
	 
	if (value & 0x1)
		omap_clk_reparent(gp1_fclk,omap_findclk(s->mpu, "omap3_sys_clk"));
	else
		omap_clk_reparent(gp1_fclk,omap_findclk(s->mpu, "omap3_32k_fclk"));
	/*Tell GPTIMER to generate new clk rate*/
	omap_gp_timer_chage_clk(s->mpu->gptimer[0]);

	//OMAP3_DEBUG(("omap3_gp1_fclk %d \n",omap_clk_getrate(omap_findclk(s->mpu, "omap3_gp1_fclk"))));
	
	/*TODO:CM_USIM_CLK CLKSEL_RM*/	
}

static inline void omap3_cm_mpu_update(struct omap3_cm_s *s)
{
	uint32_t m,n,divide,m2,cm_clken_pll_mpu;
	uint32_t bypass = 1;
	
	cm_clken_pll_mpu = s->cm_clken_pll_mpu;
	omap_clk mpu_clk = omap_findclk(s->mpu, "omap3_mpu_clk");
	 
	if ((cm_clken_pll_mpu & 0x7)==0x5)
	{	
		bypass = 1;
	}
	else if ((cm_clken_pll_mpu & 0x7)==0x7)
	{
		m = (s->cm_clksel1_pll_mpu & 0x7ff00)>>8;
		if ((m==0)||(m==1))
			bypass = 1;
		else
			bypass = 0;
	}
	if (bypass==1)
	{
		/*BYPASS Model*/
		divide = (s->cm_clksel1_pll_mpu & 0x380000)>>19;
		//OMAP3_DEBUG(("divide %d\n",divide));
		omap_clk_reparent(mpu_clk,omap_findclk(s->mpu, "omap3_core_clk"));
		omap_clk_setrate(mpu_clk, divide, 1);

	}
	else
	{
		n = (s->cm_clksel1_pll_mpu & 0x7F);
		m2 = (s->cm_clksel2_pll_mpu & 0x1F);
		//OMAP3_DEBUG(("M  %d N %d M2 %d \n",m,n,m2 ));
		omap_clk_reparent(mpu_clk,omap_findclk(s->mpu, "omap3_sys_clk"));
		omap_clk_setrate(mpu_clk, (n+1)*m2, m);
		//OMAP3_DEBUG(("mpu %d \n",omap_clk_getrate(mpu_clk)));

	}

}
static inline void omap3_cm_iva2_update(struct omap3_cm_s *s)
{
	uint32_t m,n,divide,m2,cm_clken_pll_iva2;
	uint32_t bypass = 1;
	
	cm_clken_pll_iva2 = s->cm_clken_pll_iva2;
	omap_clk iva2_clk = omap_findclk(s->mpu, "omap3_iva2_clk");
	 
	if (((cm_clken_pll_iva2 & 0x7)==0x5)|| ((cm_clken_pll_iva2 & 0x7)==0x1))
	{	
		bypass = 1;
	}
	else if ((cm_clken_pll_iva2 & 0x7)==0x7)
	{
		m = (s->cm_clksel1_pll_iva2 & 0x7ff00)>>8;
		if ((m==0)||(m==1))
			bypass = 1;
		else
			bypass = 0;
	}
	if (bypass==1)
	{
		/*BYPASS Model*/
		divide = (s->cm_clksel1_pll_iva2 & 0x380000)>>19;
		//OMAP3_DEBUG(("divide %d\n",divide));
		omap_clk_reparent(iva2_clk,omap_findclk(s->mpu, "omap3_core_clk"));
		omap_clk_setrate(iva2_clk, divide, 1);

	}
	else
	{
		n = (s->cm_clksel1_pll_iva2 & 0x7F);
		m2 = (s->cm_clksel2_pll_iva2 & 0x1F);
		//OMAP3_DEBUG(("M  %d N %d M2 %d \n",m,n,m2 ));
		omap_clk_reparent(iva2_clk,omap_findclk(s->mpu, "omap3_sys_clk"));
		omap_clk_setrate(iva2_clk, (n+1)*m2, m);
		//OMAP3_DEBUG(("iva2_clk %d \n",omap_clk_getrate(iva2_clk)));

	}

}

static inline void omap3_cm_dpll3_update(struct omap3_cm_s *s)
{
	uint32_t m,n,m2,m3,cm_clken_pll;
	uint32_t bypass =1 ;
	
	cm_clken_pll = s->cm_clken_pll;
	
	/*dpll3 bypass mode. parent clock is always omap3_sys_clk*/
	if (((cm_clken_pll & 0x7)==0x5)||((cm_clken_pll & 0x7)==0x6))
	{
		bypass =1;
	}
	else if ((cm_clken_pll & 0x7)==0x7)
	{
		m = (s->cm_clksel1_pll & 0x7ff0000)>>16;
		if ((m==0)||(m==1))
			bypass =1;
		else
			bypass =0;
	}
	if (bypass==1)
	{
		omap_clk_setrate(omap_findclk(s->mpu, "omap3_core_clk"),1,1);
		omap_clk_setrate(omap_findclk(s->mpu, "omap3_core2_clk"),1,1);
		omap_clk_setrate(omap_findclk(s->mpu, "omap3_emu_core_alwon_clk"),1,1);
	}
	else
	{
		n = (s->cm_clksel1_pll & 0x3f00)>>8;
		m2 = (s->cm_clksel1_pll & 0xf8000000)>>27;
		m3 = (s->cm_clksel1_emu & 0x1f0000)>>16;

		//OMAP3_DEBUG(("dpll3 cm_clksel1_pll %x m  %d n %d m2 %d  m3 %d\n",s->cm_clksel1_pll,m,n,m2,m3 ));
		omap_clk_setrate(omap_findclk(s->mpu, "omap3_core_clk"), (n+1)*m2, m);
		omap_clk_setrate(omap_findclk(s->mpu, "omap3_core2_clk"), (n+1)*m2, m*2);
		omap_clk_setrate(omap_findclk(s->mpu, "omap3_emu_core_alwon_clk"), (n+1)*m3, m*2);
		//OMAP3_DEBUG(("coreclk %d \n",omap_clk_getrate(omap_findclk(s->mpu, "omap3_core_clk"))));
	}

	
}


static inline void omap3_cm_dpll4_update(struct omap3_cm_s *s)
{
	uint32_t m,n,m2,m3,m4,m5,m6,cm_clken_pll;
	cm_clken_pll = s->cm_clken_pll;
	uint32_t bypass = 1;
	
	/*dpll3 bypass mode. parent clock is always omap3_sys_clk*/
	/*DPLL4*/
	 if ((cm_clken_pll & 0x70000)==0x10000)
	 {
	 	bypass = 1;
	 }
	 else if ((cm_clken_pll & 0x70000)==0x70000)
	 {
	 	m = (s->cm_clksel2_pll & 0x7ff00)>>8;
	 	if ((m==0)||(m==1))
	 		bypass = 1;
	 	else
	 		bypass = 0;
	 }
	if (bypass==1)
	{
		 omap_clk_setrate(omap_findclk(s->mpu, "omap3_96m_fclk"),1,1);
	 	 omap_clk_setrate(omap_findclk(s->mpu, "omap3_54m_fclk"),1,1);
	 	 omap_clk_setrate(omap_findclk(s->mpu, "omap3_dss1_alwon_fclk"),1,1); 
	 	 omap_clk_setrate(omap_findclk(s->mpu, "omap3_cam_mclk"),1,1);
	 	 omap_clk_setrate(omap_findclk(s->mpu, "omap3_per_alwon_clk"),1,1);
	}
	else
	{
		n = (s->cm_clksel2_pll & 0x7f);
	 	m2 = s->cm_clksel3_pll & 0x1f;
	 	m3 = (s->cm_clksel_dss & 0x1f00)>>8;
	 	m4 = s->cm_clksel_dss & 0x1f;
	 	m5 = s->cm_clksel_cam & 0x1f;
	 	m6 = (s->cm_clksel1_emu & 0x1f000000)>>24;

	 	//OMAP3_DEBUG(("dpll4 m  %d n %d m2 %d  m3 %d m4 %d m5 %d m6 %d \n",m,n,m2,m3,m4,m5,m6 ));
	 	omap_clk_setrate(omap_findclk(s->mpu, "omap3_96m_fclk"),(n+1)*m2, m*2);
	 	omap_clk_setrate(omap_findclk(s->mpu, "omap3_54m_fclk"),(n+1)*m3, m*2);
	 	omap_clk_setrate(omap_findclk(s->mpu, "omap3_dss1_alwon_fclk"),(n+1)*m4, m*2); 
	 	omap_clk_setrate(omap_findclk(s->mpu, "omap3_cam_mclk"),(n+1)*m5, m*2);
	 	omap_clk_setrate(omap_findclk(s->mpu, "omap3_per_alwon_clk"),(n+1)*m6, m*2);

	 	//OMAP3_DEBUG(("omap3_96m_fclk %d \n",omap_clk_getrate(omap_findclk(s->mpu, "omap3_96m_fclk"))));
	 	//OMAP3_DEBUG(("omap3_54m_fclk %d \n",omap_clk_getrate(omap_findclk(s->mpu, "omap3_54m_fclk"))));
	 	//OMAP3_DEBUG(("omap3_dss1_alwon_fclk %d \n",omap_clk_getrate(omap_findclk(s->mpu, "omap3_dss1_alwon_fclk"))));
	 	//OMAP3_DEBUG(("omap3_cam_mclk %d \n",omap_clk_getrate(omap_findclk(s->mpu, "omap3_cam_mclk"))));
	 	//OMAP3_DEBUG(("omap3_per_alwon_clk %d \n",omap_clk_getrate(omap_findclk(s->mpu, "omap3_per_alwon_clk"))));
		//OMAP3_DEBUG(("omap3_48m_fclk %d \n",omap_clk_getrate(omap_findclk(s->mpu, "omap3_48m_fclk"))));
		//OMAP3_DEBUG(("omap3_12m_fclk %d \n",omap_clk_getrate(omap_findclk(s->mpu, "omap3_12m_fclk"))));

	}
	 
}

static inline void omap3_cm_48m_update(struct omap3_cm_s *s)
{
	if (s->cm_clksel1_pll & 0x8)
	{
		/*parent is sysaltclk*/
		omap_clk_reparent(omap_findclk(s->mpu, "omap3_48m_fclk"), 
											 omap_findclk(s->mpu, "omap3_sys_altclk"));
		omap_clk_reparent(omap_findclk(s->mpu, "omap3_12m_fclk"), 
											 omap_findclk(s->mpu, "omap3_sys_altclk"));
		/*TODO:need to set rate ?*/
		
	}
	else
	{
		omap_clk_reparent(omap_findclk(s->mpu, "omap3_12m_fclk"), 
											 omap_findclk(s->mpu, "omap3_96m_fclk"));
		omap_clk_reparent(omap_findclk(s->mpu, "omap3_48m_fclk"), 
											 omap_findclk(s->mpu, "omap3_96m_fclk"));
		omap_clk_setrate(omap_findclk(s->mpu, "omap3_48m_fclk"),2,1);
		omap_clk_setrate(omap_findclk(s->mpu, "omap3_12m_fclk"),8,1);

	}
		
}
static inline void omap3_cm_gp10_update(struct omap3_cm_s *s)
{
	omap_clk gp10_fclk = omap_findclk(s->mpu, "omap3_gp10_fclk");
	 
	if (s->cm_clksel_core & 0x40)
		omap_clk_reparent(gp10_fclk,omap_findclk(s->mpu, "omap3_sys_clk"));
	else
		omap_clk_reparent(gp10_fclk,omap_findclk(s->mpu, "omap3_32k_fclk"));
	
	/*Tell GPTIMER10 to generate new clk rate*/
	//omap_gp_timer_chage_clk(s->mpu->gptimer[0]);

	//	OMAP3_DEBUG(("omap3_gp10_fclk %d \n",omap_clk_getrate(omap_findclk(s->mpu, "omap3_gp10_fclk"))));


}
static inline void omap3_cm_gp11_update(struct omap3_cm_s *s)
{
	omap_clk gp11_fclk = omap_findclk(s->mpu, "omap3_gp11_fclk");
	 
	if (s->cm_clksel_core & 0x80)
		omap_clk_reparent(gp11_fclk,omap_findclk(s->mpu, "omap3_sys_clk"));
	else
		omap_clk_reparent(gp11_fclk,omap_findclk(s->mpu, "omap3_32k_fclk"));
	/*Tell GPTIMER10 to generate new clk rate*/
	//omap_gp_timer_chage_clk(s->mpu->gptimer[0]);
	//OMAP3_DEBUG(("omap3_gp11_fclk %d \n",omap_clk_getrate(omap_findclk(s->mpu, "omap3_gp11_fclk"))));
}
static inline void omap3_cm_l3clk_update(struct omap3_cm_s *s)
{
	omap_clk l3_iclk = omap_findclk(s->mpu, "omap3_l3_iclk");
	if ((s->cm_clksel_core & 0x3)==0x1)
		omap_clk_setrate(l3_iclk,1, 1);
	else if ((s->cm_clksel_core & 0x3)==0x2)
		omap_clk_setrate(l3_iclk,2, 1);
}
static inline void omap3_cm_l4clk_update(struct omap3_cm_s *s)
{
	omap_clk l4_iclk = omap_findclk(s->mpu, "omap3_l4_iclk");
	if ((s->cm_clksel_core & 0xc)==0x4)
		omap_clk_setrate(l4_iclk,1, 1);
	else if ((s->cm_clksel_core & 0xc)==0x8)
		omap_clk_setrate(l4_iclk,2, 1);
}
static inline void omap3_cm_per_gptimer_update(struct omap3_cm_s *s)
{
	uint32_t cm_clksel_per = s->cm_clksel_per;
	
	if (cm_clksel_per&0x1)
		omap_clk_reparent(omap_findclk(s->mpu, "omap3_gp2_fclk")
										   ,omap_findclk(s->mpu, "omap3_sys_clk"));
	else
		omap_clk_reparent(omap_findclk(s->mpu, "omap3_gp2_fclk")
										   ,omap_findclk(s->mpu, "omap3_32k_fclk"));

	if (cm_clksel_per&0x2)
		omap_clk_reparent(omap_findclk(s->mpu, "omap3_gp3_fclk")
										   ,omap_findclk(s->mpu, "omap3_sys_clk"));
	else
		omap_clk_reparent(omap_findclk(s->mpu, "omap3_gp3_fclk")
										   ,omap_findclk(s->mpu, "omap3_32k_fclk"));

	if (cm_clksel_per&0x4)
		omap_clk_reparent(omap_findclk(s->mpu, "omap3_gp4_fclk")
										   ,omap_findclk(s->mpu, "omap3_sys_clk"));
	else
		omap_clk_reparent(omap_findclk(s->mpu, "omap3_gp4_fclk")
										   ,omap_findclk(s->mpu, "omap3_32k_fclk"));

	if (cm_clksel_per&0x8)
		omap_clk_reparent(omap_findclk(s->mpu, "omap3_gp5_fclk")
										   ,omap_findclk(s->mpu, "omap3_sys_clk"));
	else
		omap_clk_reparent(omap_findclk(s->mpu, "omap3_gp5_fclk")
										   ,omap_findclk(s->mpu, "omap3_32k_fclk"));

	if (cm_clksel_per&0x10)
		omap_clk_reparent(omap_findclk(s->mpu, "omap3_gp6_fclk")
										   ,omap_findclk(s->mpu, "omap3_sys_clk"));
	else
		omap_clk_reparent(omap_findclk(s->mpu, "omap3_gp6_fclk")
										   ,omap_findclk(s->mpu, "omap3_32k_fclk"));
	if (cm_clksel_per&0x20)
		omap_clk_reparent(omap_findclk(s->mpu, "omap3_gp7_fclk")
										   ,omap_findclk(s->mpu, "omap3_sys_clk"));
	else
		omap_clk_reparent(omap_findclk(s->mpu, "omap3_gp7_fclk")
										   ,omap_findclk(s->mpu, "omap3_32k_fclk"));


	if (cm_clksel_per&0x40)
		omap_clk_reparent(omap_findclk(s->mpu, "omap3_gp8_fclk")
										   ,omap_findclk(s->mpu, "omap3_sys_clk"));
	else
		omap_clk_reparent(omap_findclk(s->mpu, "omap3_gp8_fclk")
										   ,omap_findclk(s->mpu, "omap3_32k_fclk"));
	if (cm_clksel_per&0x80)
		omap_clk_reparent(omap_findclk(s->mpu, "omap3_gp9_fclk")
										   ,omap_findclk(s->mpu, "omap3_sys_clk"));
	else
		omap_clk_reparent(omap_findclk(s->mpu, "omap3_gp9_fclk")
										   ,omap_findclk(s->mpu, "omap3_32k_fclk"));

	/*TODO:Tell GPTIMER to generate new clk rate*/
	
	//OMAP3_DEBUG(("omap3_gp9_fclk %d \n",omap_clk_getrate(omap_findclk(s->mpu, "omap3_gp9_fclk"))));
	//OMAP3_DEBUG(("omap3_gp8_fclk %d \n",omap_clk_getrate(omap_findclk(s->mpu, "omap3_gp8_fclk"))));

	
}
static void omap3_cm_reset(struct omap3_cm_s *s)
{
	s->cm_fclken_iva2 = 0x0;
	s->cm_clken_pll_iva2 = 0x11;
	s->cm_idlest_iva2 = 0x1;
	s->cm_idlest_pll_iva2 = 0x0;
	s->cm_autoidle_pll_iva2 = 0x0;
	s->cm_clksel1_pll_iva2 = 0x80000;
	s->cm_clksel2_pll_iva2 = 0x1;
	s->cm_clkstctrl_iva2 = 0x0;
	s->cm_clkstst_iva2 = 0x0;

	s->cm_revision = 0x10;
	s->cm_sysconfig = 0x1;

	s->cm_clken_pll_mpu = 0x15;
	s->cm_idlest_mpu = 0x1;
	s->cm_idlest_pll_mpu = 0x0;
	s->cm_autoidle_pll_mpu = 0x0;
	s->cm_clksel1_pll_mpu = 0x80000;
	s->cm_clksel2_pll_mpu = 0x1;
	s->cm_clkstctrl_mpu = 0x0;
	s->cm_clkstst_mpu = 0x0;

	s->cm_fclken1_core = 0x0;
	s->cm_fclken3_core = 0x0;
	s->cm_iclken1_core = 0x42;
	s->cm_iclken2_core = 0x0;
	s->cm_iclken3_core = 0x0;
	s->cm_idlest1_core = 0xffffffff;
	s->cm_idlest2_core = 0x1f;
	s->cm_idlest3_core = 0xf;
	s->cm_autoidle1_core = 0x0;
	s->cm_autoidle2_core = 0x0;
	s->cm_autoidle3_core = 0x0;
	s->cm_clksel_core = 0x105;
	s->cm_clkstctrl_core = 0x0;
	s->cm_clkstst_core = 0x0;

	s->cm_fclken_sgx = 0x0;
	s->cm_iclken_sgx = 0x0;
	s->cm_idlest_sgx = 0x1;
	s->cm_clksel_sgx = 0x0;
	s->cm_sleepdep_sgx = 0x0;
	s->cm_clkstctrl_sgx = 0x0;
	s->cm_clkstst_sgx = 0x0;

	s->cm_fclken_wkup = 0x0;
	s->cm_iclken_wkup = 0x0;
	s->cm_idlest_wkup = 0x2ff;
	s->cm_autoidle_wkup = 0x0;
	s->cm_clksel_wkup = 0x12;

	s->cm_clken_pll = 0x110015;
	s->cm_clken2_pll = 0x11;
	s->cm_idlest_ckgen = 0x0;
	s->cm_idlest2_ckgen = 0x0;
	s->cm_autoidle_pll = 0x0;
	s->cm_autoidle2_pll = 0x0;
	s->cm_clksel1_pll = 0x8000040;
	s->cm_clksel2_pll = 0x0;
	s->cm_clksel3_pll = 0x1;
	s->cm_clksel4_pll = 0x0;
	s->cm_clksel5_pll = 0x1;
	s->cm_clkout_ctrl = 0x3;


	s->cm_fclken_dss = 0x0;
	s->cm_iclken_dss = 0x0;
	s->cm_idlest_dss = 0x3;
	s->cm_autoidle_dss = 0x0;
	s->cm_clksel_dss = 0x1010;
	s->cm_sleepdep_dss = 0x0;
	s->cm_clkstctrl_dss = 0x0;
	s->cm_clkstst_dss = 0x0;

	s->cm_fclken_cam = 0x0;
	s->cm_iclken_cam = 0x0;
	s->cm_idlest_cam = 0x1;
	s->cm_autoidle_cam = 0x0;
	s->cm_clksel_cam = 0x10;
	s->cm_sleepdep_cam = 0x0;
	s->cm_clkstctrl_cam = 0x0;
	s->cm_clkstst_cam = 0x0;

	s->cm_fclken_per = 0x0;
	s->cm_iclken_per = 0x0;
	s->cm_idlest_per = 0x3ffff;
	s->cm_autoidle_per = 0x0;
	s->cm_clksel_per = 0x0;
	s->cm_sleepdep_per = 0x0;
	s->cm_clkstctrl_per = 0x0;
	s->cm_clkstst_per = 0x0;

	s->cm_clksel1_emu = 0x10100a50;
	s->cm_clkstctrl_emu = 0x2;
	s->cm_clkstst_emu = 0x0;
	s->cm_clksel2_emu = 0x0;
	s->cm_clksel3_emu = 0x0;

	s->cm_polctrl = 0x0;

	s->cm_idlest_neon = 0x1;
	s->cm_clkstctrl_neon = 0x0;

	s->cm_fclken_usbhost = 0x0;
	s->cm_iclken_usbhost = 0x0;
	s->cm_idlest_usbhost = 0x3;
	s->cm_autoidle_usbhost = 0x0;
	s->cm_sleepdep_usbhost = 0x0;
	s->cm_clkstctrl_usbhost = 0x0;
	s->cm_clkstst_usbhost = 0x0;	
}

static uint32_t omap3_cm_read(void *opaque, target_phys_addr_t addr)
{
    struct omap3_cm_s *s = (struct omap3_cm_s *) opaque;
    int offset = addr - s->base;
    uint32_t ret;
    uint32_t bypass,m;

     switch (offset) 
     {
     	case 0x04:
     		return s->cm_clken_pll_iva2;
     	case 0x24:
     		if (((s->cm_clken_pll_iva2 & 0x7)==0x5)|| ((s->cm_clken_pll_iva2 & 0x7)==0x1))
			{	
				bypass = 1;
			}
			else if ((s->cm_clken_pll_iva2 & 0x7)==0x7)
			{
				m = (s->cm_clksel1_pll_iva2 & 0x7ff00)>>8;
				if ((m==0)||(m==1))
					bypass = 1;
				else
					bypass = 0;
			}
			if (bypass)
				return 0;
			else 
				return 1;
     	case 0x40:
     		return s->cm_clksel1_pll_iva2;
     	case 0x44:
     		return s->cm_clksel2_pll_iva2;
     	case 0xa40:  /*CM_CLKSEL_CORE*/
			return s->cm_clksel_core;
		case 0xb40:  /*CM_CLKSEL_SGX*/
			return s->cm_clksel_sgx;
     	case 0x904:    /*CM_CLKEN_PLL_MPU*/
       	return s->cm_clken_pll_mpu;
       case 0x940:
       	return s->cm_clksel1_pll_mpu;
       case 0x944:
       	return s->cm_clksel2_pll_mpu;
       case 0x924:
       	if ((s->cm_clken_pll_mpu & 0x7)==0x5)
			{	
				bypass = 1;
			}
			else if ((s->cm_clken_pll_mpu & 0x7)==0x7)
			{
				m = (s->cm_clksel1_pll_mpu & 0x7ff00)>>8;
				if ((m==0)||(m==1))
					bypass = 1;
				else
					bypass = 0;
			}
			if (bypass)
				return 0;
			else
				return 1;
	   case 0xa00:
       	return s->cm_fclken1_core;
      case 0xa10:
       	return s->cm_iclken1_core;
     	case 0xc00:   /*CM_FCLKEN_WKUP*/
     		return s->cm_fclken_wkup;
     	case 0xc10:    /*CM_ICLKEN_WKUP*/
     		return s->cm_iclken_wkup;
     	case 0xc20:    /*CM_IDLEST_WKUP*/
     		/*TODO: Check whether the timer can be accessed.*/
     		return 0x0;
     	case 0xc40:
     		return s->cm_clksel_wkup;
     	case 0xd00:   /*CM_CLKEN_PLL*/
     		return s->cm_clken_pll;
     	case 0xd40:   /*CM_CLKSEL1_PLL*/
     		return s->cm_clksel1_pll;
     	case 0xd44:
     		return s->cm_clksel2_pll ;
     	case 0xd48:   /*CM_CLKSEL3_PLL*/
     		return s->cm_clksel3_pll ;
     	case 0xd20:  /*CM_IDLEST_CKGEN*/
     		/*FIXME: all clock is active. we do not care it.*/
     		ret = 0x3ffff;
     		/*DPLL3 BYPASS*/
     		if (((s->cm_clken_pll & 0x7)==0x5)||((s->cm_clken_pll & 0x7)==0x6))
     			ret |= 0x3fffe;
     		/*DPLL4 BYPASS*/
     		 if ((s->cm_clken_pll & 0x70000)==0x10000)
     			ret |= 0x3fffd;
     		return ret; 
       case 0xe40:
     		return s->cm_clksel_dss;
     	case 0xf40:
     		return s->cm_clksel_cam;
     	case 0x1000:
     		return s->cm_fclken_per;
     	case 0x1010:
     		return s->cm_iclken_per;
     	case 0x1040:
     		return s->cm_clksel_per;
     	case 0x1140:  /*CM_CLKSEL1_EMU*/
     		return s->cm_clksel1_emu;

     	default:
     		printf("omap3_cm_read addr %x offset %x \n",addr,offset);
     		exit(-1);
     }
}


static void omap3_cm_write(void *opaque, target_phys_addr_t addr,
                uint32_t value)
{
    struct omap3_cm_s *s = (struct omap3_cm_s *) opaque;
    int offset = addr - s->base;

    switch (offset) 
     {
     	case 0xc20:   /*CM_IDLEST_WKUP*/
     		OMAP_RO_REG(addr);
	        exit(-1);
        	 break;
	  case 0x4:   /*CM_CLKEN_PLL_IVA2*/
	  	  s->cm_clken_pll_iva2 = value &0x7ff;
	  	  omap3_cm_iva2_update(s);
         break;
     case 0x40:
     		s->cm_clksel1_pll_iva2 = value & 0x3fff7f;
     		//printf("value %x s->cm_clksel1_pll_iva2 %x \n",value,s->cm_clksel1_pll_iva2);
      		omap3_cm_iva2_update(s);
       	break;
     case 0x44:
     		s->cm_clksel2_pll_iva2 = value & 0x1f;
      		omap3_cm_iva2_update(s);
      		break;
     case 0x904:    /*CM_CLKEN_PLL_MPU*/
       	s->cm_clken_pll_mpu = value &0x7ff;
       	omap3_cm_mpu_update(s);
       	break;
      case 0x940:
      	   //printf("s->cm_clksel1_pll_mpu  %x\n",s->cm_clksel1_pll_mpu );
      		s->cm_clksel1_pll_mpu = value & 0x3fff7f;
      		omap3_cm_mpu_update(s);
       	break;
      case 0x944:
      		s->cm_clksel2_pll_mpu = value & 0x1f;
      		omap3_cm_mpu_update(s);
      		break;
       case 0xa00:
       	s->cm_fclken1_core = value & 0x43fffe00;
       case 0xa10:
       	s->cm_iclken1_core = value & 0x7ffffed2;

     	case 0xa40:  /*CM_CLKSEL_CORE*/
     		s->cm_clksel_core = (value &0xcf);
     		s->cm_clksel_core |= 0x700;
     		omap3_cm_gp10_update(s);
     		omap3_cm_gp11_update(s);
     		omap3_cm_l3clk_update(s);
     		omap3_cm_l4clk_update(s);
     		break;
     	case 0xb40:  /*CM_CLKSEL_SGX*/
     		/*TODO: SGX Clock!!*/
			s->cm_clksel_sgx = value;
     		break;
     	case 0xc00:   /*CM_FCLKEN_WKUP*/
     		s->cm_fclken_wkup = value & 0x2e9;
     		//omap3_cm_fclken_wkup_update(s,s->cm_fclken_wkup);
     		break;
     	case 0xc10:    /*CM_ICLKEN_WKUP*/
     		s->cm_iclken_wkup = value & 0x2ff;
     		//omap3_cm_iclken_wkup_update(s,s->cm_iclken_wkup);
     		break;
     	case 0xc40:      /*CM_CLKSEL_WKUP*/
     		s->cm_clksel_wkup = value & 0x7f;
     		omap3_cm_clksel_wkup_update(s,s->cm_clksel_wkup);
     		break;
     	case 0xd00:   /*CM_CLKEN_PLL*/
     		s->cm_clken_pll = value & 0xffff17ff;
     		omap3_cm_dpll3_update(s);
     		omap3_cm_dpll4_update(s);
     		break;
     	case 0xd40:   /*CM_CLKSEL1_PLL*/
     		//OMAP3_DEBUG(("WD40 value %x \n",value));
     		s->cm_clksel1_pll = value & 0xffffbffc;
     		//OMAP3_DEBUG(("WD40 value %x \n",value));
     		omap3_cm_dpll3_update(s);
     		omap3_cm_48m_update(s);
     		break;
     	case 0xd44:
     		s->cm_clksel2_pll = value & 0x7ff7f;
     		omap3_cm_dpll4_update(s);
     		break;
     	case 0xd48:   /*CM_CLKSEL3_PLL*/
     		s->cm_clksel3_pll = value & 0x1f;
     		omap3_cm_dpll4_update(s);
     		break;
     	case 0xe40:
     		s->cm_clksel_dss= value & 0x1f1f;
     		omap3_cm_dpll4_update(s);
     		break;
     	case 0xf40:
     		s->cm_clksel_cam= value & 0x1f;
     		omap3_cm_dpll4_update(s);
     		break;
     	case 0x1000:
     		s->cm_fclken_per = value & 0x3ffff;
     		break;
     	case 0x1010:
     		s->cm_iclken_per = value & 0x3ffff;
     		break;
     	case 0x1040:
     		s->cm_clksel_per = value & 0xff;
     		omap3_cm_per_gptimer_update(s);
     		break;
     	case 0x1140:  /*CM_CLKSEL1_EMU*/
     		s->cm_clksel1_emu = value & 0x1f1f3fff;
     		//printf("cm_clksel1_emu %x\n",s->cm_clksel1_emu);
     		omap3_cm_dpll3_update(s);
     		omap3_cm_dpll4_update(s);
     		break;

     	default:
     		printf("omap3_cm_write addr %x value %x \n",addr,value);
     		exit(-1);
     }
}



static CPUReadMemoryFunc *omap3_cm_readfn[] = {
    omap_badwidth_read32,
    omap_badwidth_read32,
    omap3_cm_read,
};

static CPUWriteMemoryFunc *omap3_cm_writefn[] = {
    omap_badwidth_write32,
    omap_badwidth_write32,
    omap3_cm_write,
};

struct omap3_cm_s *omap3_cm_init(struct omap_target_agent_s *ta,
                qemu_irq mpu_int, qemu_irq dsp_int, qemu_irq iva_int,
                struct omap_mpu_state_s *mpu)
{
    int iomemtype;
    struct omap3_cm_s *s = (struct omap3_cm_s *)qemu_mallocz(sizeof(*s));

    s->irq[0] = mpu_int;
    s->irq[1] = dsp_int;
    s->irq[2] = iva_int;
    s->mpu = mpu;
    omap3_cm_reset(s);

    iomemtype = l4_register_io_memory(0, omap3_cm_readfn,
                    omap3_cm_writefn, s);
    s->base = omap_l4_attach(ta, 0, iomemtype);
    omap_l4_attach(ta, 1, iomemtype);

    return s;
}

#define OMAP3_SEC_WDT          1
#define OMAP3_MPU_WDT         2
#define OMAP3_IVA2_WDT        3
/*omap3 watchdog timer*/
struct omap3_wdt_s {
	target_phys_addr_t base;
    qemu_irq irq;          /*IVA2 IRQ*/
    struct omap_mpu_state_s *mpu;
    omap_clk clk;
    QEMUTimer *timer;
    
    int active;
    int64_t rate;
    int64_t time;
    //int64_t ticks_per_sec;

    uint32_t wd_sysconfig;
    uint32_t wd_sysstatus;
    uint32_t wisr;
    uint32_t wier;
    uint32_t wclr;
    uint32_t wcrr;
    uint32_t wldr;
    uint32_t wtgr;
    uint32_t wwps;
    uint32_t wspr;

	/*pre and ptv in wclr*/
    uint32_t pre;
    uint32_t ptv;
    //uint32_t val;

    uint16_t writeh;	/* LSB */
    uint16_t readh;  	/* MSB */

};





static inline void omap3_wdt_timer_update(struct omap3_wdt_s *wdt_timer)
{
	 int64_t expires;
	 if (wdt_timer->active)
	 {
        expires = muldiv64(0xffffffffll - wdt_timer->wcrr,
                        ticks_per_sec, wdt_timer->rate);
        qemu_mod_timer(wdt_timer->timer, wdt_timer->time + expires);
	 }
	 else
	 	qemu_del_timer(wdt_timer->timer);
}
static void omap3_wdt_clk_setup(struct omap3_wdt_s *timer)
{
	/*TODO: Add irq as user to clk*/
}

static inline uint32_t omap3_wdt_timer_read(struct omap3_wdt_s *timer)
{
    uint64_t distance;

    if (timer->active) {
        distance = qemu_get_clock(vm_clock) - timer->time;
        distance = muldiv64(distance, timer->rate, ticks_per_sec);

        if (distance >= 0xffffffff - timer->wcrr)
            return 0xffffffff;
        else
            return timer->wcrr + distance;
    } else
        return timer->wcrr;
}
/*
static inline void omap3_wdt_timer_sync(struct omap3_wdt_s *timer)
{
    if (timer->active) {
        timer->val = omap3_wdt_timer_read(timer);
        timer->time = qemu_get_clock(vm_clock);
    }
}*/
	
static void omap3_wdt_reset(struct omap3_wdt_s *s,int wdt_index)
{
	s->wd_sysconfig = 0x0;
	s->wd_sysstatus = 0x0;
	s->wisr = 0x0;
	s->wier = 0x0;
	s->wclr = 0x20;
	s->wcrr = 0x0;
	switch (wdt_index)
	{
		case OMAP3_MPU_WDT:
		case OMAP3_IVA2_WDT:
			s->wldr = 0xfffb0000;
			break;
		case OMAP3_SEC_WDT:
			s->wldr = 0xffa60000;
			break;
	}
	s->wtgr= 0x0;
	s->wwps = 0x0;
	s->wspr = 0x0;

	switch (wdt_index)
	{
		case OMAP3_SEC_WDT:
		case OMAP3_MPU_WDT:
			s->active = 1;
			break;
		case OMAP3_IVA2_WDT:
			s->active = 0;
			break;
	}
	s->pre = s->wclr & (1<<5);
	s->ptv = (s->wclr & 0x1c)>>2;
	s->rate  = omap_clk_getrate(s->clk)>>(s->pre ? s->ptv : 0);

	s->active = 1;
	s->time = qemu_get_clock(vm_clock);
	omap3_wdt_timer_update(s);
}

static uint32_t omap3_wdt_read32(void *opaque, target_phys_addr_t addr,int wdt_index)
{
	struct omap3_wdt_s *s = (struct omap3_wdt_s *) opaque;
    int offset = addr - s->base;
    //uint32_t ret;
	//printf("omap3_wdt_read32 addr %x \n",addr);
     switch (offset) 
     {
     	case 0x10:   /*WD_SYSCONFIG*/
     		return s->wd_sysconfig;
     	case 0x14:    /*WD_SYSSTATUS*/
     		return s->wd_sysstatus;
     	case 0x18:    /*WISR*/
     		return s->wisr & 0x1;
     	case 0x1c:   /*WIER*/
     		return s->wier & 0x1;
     	case 0x24:     /*WCLR*/
     		return s->wclr & 0x3c;
     	case 0x28:      /*WCRR*/
			s->wcrr = omap3_wdt_timer_read(s);
			s->time = qemu_get_clock(vm_clock);
			return s->wcrr;
		case 0x2c:     /*WLDR*/
			return s->wldr;
		case 0x30:     /*WTGR*/
			 return s->wtgr;
		case 0x34:     /*WWPS*/
			return s->wwps;
		case 0x48:     /*WSPR*/
			return s->wspr;
     	default:      
     		printf("omap3_wdt_read32 addr %x \n",addr);
     		exit(-1);
     }
}
static uint32_t omap3_mpu_wdt_read16(void *opaque, target_phys_addr_t addr)
{	
	 struct omap3_wdt_s *s = (struct omap3_wdt_s *) opaque;
    uint32_t ret;

    if (addr & 2)
        return s->readh;
    else {
        ret = omap3_wdt_read32(opaque, addr,OMAP3_MPU_WDT);
        s->readh = ret >> 16;
        return ret & 0xffff;
    }
}
static uint32_t omap3_mpu_wdt_read32(void *opaque, target_phys_addr_t addr)
{
	return omap3_wdt_read32(opaque,addr,OMAP3_MPU_WDT);
}

static void omap3_wdt_write32(void *opaque, target_phys_addr_t addr,
                uint32_t value,int wdt_index)
{
	struct omap3_wdt_s *s = (struct omap3_wdt_s *) opaque;
    int offset = addr - s->base;
    //uint32_t ret;

     //printf("omap3_wdt_write32 addr %x value %x \n",addr,value);
     switch (offset) 
     {
     	 case 0x14:    /*WD_SYSSTATUS*/
     	 case 0x34:     /*WWPS*/	
	        OMAP_RO_REG(addr);
	        exit(-1);
        	 break;
     	case 0x10:   /*WD_SYSCONFIG*/
     		s->wd_sysconfig = value & 0x33f;
     		break;
     	case 0x18:    /*WISR*/
     		s->wisr = value & 0x1;
     		break;
     	case 0x1c:   /*WIER*/
     		s->wier = value & 0x1;
     		break;
     	case 0x24:     /*WCLR*/
     		s->wclr = value & 0x3c;
     		break;
     	case 0x28:      /*WCRR*/
			s->wcrr = value;
			s->time = qemu_get_clock(vm_clock);
			omap3_wdt_timer_update(s);
			break;
		case 0x2c:     /*WLDR*/
			s->wldr = value;   /*It will take effect after next overflow*/
			break;
		case 0x30:     /*WTGR*/
			 if (value != s->wtgr)
			 {
			 	s->wcrr = s->wldr;
			 	s->pre = s->wclr & (1<<5);
				s->ptv = (s->wclr & 0x1c)>>2;
				s->rate  = omap_clk_getrate(s->clk)>>(s->pre ? s->ptv : 0);
				s->time = qemu_get_clock(vm_clock);
				omap3_wdt_timer_update(s);
			 }
			 s->wtgr = value;
			 break;
		case 0x48:     /*WSPR*/
			if (((value&0xffff)==0x5555)&&((s->wspr&0xffff)==0xaaaa))
			{
				s->active = 0;
				s->wcrr = omap3_wdt_timer_read(s);
				omap3_wdt_timer_update(s);
			}
			if (((value&0xffff)==0x4444)&&((s->wspr&0xffff)==0xbbbb))
			{
				s->active = 1;
				s->time = qemu_get_clock(vm_clock);
				omap3_wdt_timer_update(s);
			}
			s->wspr = value;
			break;
     	default:      
     		printf("omap3_wdt_write32 addr %x \n",addr);
     		exit(-1);
     }
}

static void omap3_mpu_wdt_write16(void *opaque, target_phys_addr_t addr,
                uint32_t value)
{
	    struct omap3_wdt_s *s = (struct omap3_wdt_s *) opaque;

    if (addr & 2)
        return omap3_wdt_write32(opaque,addr,(value << 16) | s->writeh,OMAP3_MPU_WDT); 
    else
        s->writeh = (uint16_t) value;
}
static void omap3_mpu_wdt_write32(void *opaque, target_phys_addr_t addr,
                uint32_t value)
{
	omap3_wdt_write32(opaque,addr,value,OMAP3_MPU_WDT);
}


static CPUReadMemoryFunc *omap3_mpu_wdt_readfn[] = {
    omap_badwidth_read32,
    omap3_mpu_wdt_read16,
    omap3_mpu_wdt_read32,
};

static CPUWriteMemoryFunc *omap3_mpu_wdt_writefn[] = {
    omap_badwidth_write32,
    omap3_mpu_wdt_write16,
    omap3_mpu_wdt_write32,
};



static void omap3_mpu_wdt_timer_tick(void *opaque)
{
    struct omap3_wdt_s *wdt_timer = (struct omap3_wdt_s *) opaque;

    /*TODO:Sent reset pulse to PRCM*/
    wdt_timer->wcrr = wdt_timer->wldr;
    
    /*after overflow, generate the new wdt_timer->rate */
    wdt_timer->pre = wdt_timer->wclr & (1<<5);
	wdt_timer->ptv = (wdt_timer->wclr & 0x1c)>>2;
	wdt_timer->rate  = omap_clk_getrate(wdt_timer->clk)>>(wdt_timer->pre ? wdt_timer->ptv : 0);

	wdt_timer->time = qemu_get_clock(vm_clock);
	omap3_wdt_timer_update(wdt_timer);
}

struct omap3_wdt_s *omap3_mpu_wdt_init(struct omap_target_agent_s *ta, 
	 						qemu_irq irq, omap_clk fclk, omap_clk iclk,struct omap_mpu_state_s *mpu)
{
	 int iomemtype;
    struct omap3_wdt_s *s = (struct omap3_wdt_s *)
            qemu_mallocz(sizeof(*s));

    s->irq = irq;
    s->clk = fclk;
    s->timer = qemu_new_timer(vm_clock, omap3_mpu_wdt_timer_tick, s);

    omap3_wdt_reset(s,OMAP3_MPU_WDT);
    if (irq!=NULL)
    	omap3_wdt_clk_setup(s);

    iomemtype = l4_register_io_memory(0, omap3_mpu_wdt_readfn,
                    omap3_mpu_wdt_writefn, s);
    s->base = omap_l4_attach(ta, 0, iomemtype);

    return s;

}


/*dummy system control module*/
struct omap3_scm_s {
	target_phys_addr_t base;
    struct omap_mpu_state_s *mpu;

	uint32_t control_status;                /*0x4800 22F0*/
};

static void omap3_scm_reset(struct omap3_scm_s *s)
{
	s->control_status = 0x300;
}

static uint32_t omap3_scm_read8(void *opaque, target_phys_addr_t addr)
{	
	struct omap3_scm_s *s = (struct omap3_scm_s *) opaque;
    int offset = addr - s->base;

    switch (offset)
    {
    	case 0x2f0:
    		return s->control_status & 0xff;
    	case 0x2f1:
    		return (s->control_status & 0xff00)>>8;;
    	case 0x2f2:
    		return (s->control_status & 0xff0000)>>16;;
    	case 0x2f3:
    		return (s->control_status & 0xff000000)>>24;;

    	default:
    		printf("omap3_scm_read8 addr %x \n",addr);
     		exit(-1);
    }
}

static uint32_t omap3_scm_read16(void *opaque, target_phys_addr_t addr)
{
    uint32_t v;
    v = omap3_scm_read8(opaque, addr);
    v |= omap3_scm_read8(opaque, addr + 1) << 8;
    return v;
}
static uint32_t omap3_scm_read32(void *opaque, target_phys_addr_t addr)
{
    uint32_t v;
    v = omap3_scm_read8(opaque, addr);
    v |= omap3_scm_read8(opaque, addr + 1) << 8;
    v |= omap3_scm_read8(opaque, addr + 2) << 16;
    v |= omap3_scm_read8(opaque, addr + 3) << 24;
    return v;
}

static void omap3_scm_write8(void *opaque, target_phys_addr_t addr,
                uint32_t value)
{
	struct omap3_scm_s *s = (struct omap3_scm_s *) opaque;
    int offset = addr - s->base;

    switch (offset)
    {
    	case 0x30 ... 0x33:   /*sdrc_d0 sdrc_d1*/
    	case 0x34 ... 0x37:   /*sdrc_d2 sdrc_d3*/
    	case 0x38 ... 0x3b:   /*sdrc_d4 sdrc_d5*/
    	case 0x3c ... 0x3f:   /*sdrc_d6 sdrc_d7*/
    	case 0x40 ... 0x43:   /*sdrc_d8 sdrc_d9*/
    	case 0x44 ... 0x47:   /*sdrc_d10 sdrc_d11*/
    	case 0x48 ... 0x4b:   /*sdrc_d12 sdrc_d13*/
    	case 0x4c ... 0x4f:   /*sdrc_d14 sdrc_d15*/
    	case 0x50 ... 0x53:   /*sdrc_d16 sdrc_d17*/
    	case 0x54 ... 0x57:   /*sdrc_d18 sdrc_d19*/
    	case 0x58 ... 0x5b:   /*sdrc_d20 sdrc_d21*/
    	case 0x5c ... 0x5f:   /*sdrc_d22 sdrc_d23*/
    	case 0x60 ... 0x63:   /*sdrc_d24 sdrc_d25*/
    	case 0x64 ... 0x67:   /*sdrc_d26 sdrc_d27*/
    	case 0x68 ... 0x6b:   /*sdrc_d28 sdrc_d29*/
    	case 0x6c ... 0x6f:   /*sdrc_d30 sdrc_d31*/
    	case 0x70 ... 0x73:   /*sdrcclk sdrcdqs0*/
    	case 0x74 ... 0x77:   /*sdrcdqs1 sdrcdqs2*/
    	case 0x78 ... 0x7b:   /*sdrcdqs3 gpmca1*/
    	case 0x7c ... 0x7f:   /*gpmca2 gpmca3*/
    	case 0x80 ... 0x83:   /*gpmca4 gpmca5*/
    	case 0x84 ... 0x87:   /*gpmca6 gpmca7*/
    	case 0x88 ... 0x8b:   /*gpmca8 gpmca9*/
    	case 0x8c ... 0x8f:   /*gpmca10 gpmcd0*/
    	case 0x90 ... 0x93:   /*gpmcd1 gpmcd2*/
    	case 0x94 ... 0x97:   /*gpmcd3 gpmcd4*/
    	case 0x98 ... 0x9b:   /*gpmcd5 gpmcd6*/
    	case 0x9c ... 0x9f:   /*gpmcd7 gpmcd8*/
    	case 0xa0 ... 0xa3:   /*gpmcd9 gpmcd10*/
    	case 0xa4 ... 0xa7:   /*gpmcd11 gpmcd12*/
    	case 0xa8 ... 0xab:   /*gpmcd13 gpmcd14*/
    	case 0xac ... 0xaf:   /*gpmcd15 gpmcncs0*/
    	case 0xb0 ... 0xb3:   /*gpmcncs1 gpmcncs2*/
    	case 0xb4 ... 0xb7:   /*gpmcncs3 gpmcncs4*/
    	case 0xb8 ... 0xbb:   /*gpmcncs5 gpmcncs6*/
    	case 0xbc ... 0xbf:   /*gpmcncs7 gpmcclk*/
    	case 0xc0 ... 0xc3:  
    	case 0xc4 ... 0xc7: 
    	case 0xc8 ... 0xcb:   /*gpmcnbe1 gpmcnwp*/
    	case 0xcc ... 0xcf:   /*gpmcwait0 gpmcwait1*/
    	case 0xd0 ... 0xd3:   /*gpmcwait2 gpmcwait3*/

    	case 0x100 ... 0x103:   /*dssdata18 dssdata19*/
    	case 0x104 ... 0x107:   /*dssdata20 dssdata21*/
    	case 0x130 ... 0x133:   /*camwen camstrobe*/
    		

    	case 0x17c ... 0x17f:   /*uart1 tx uart1rts*/
    	case 0x180 ... 0x183:   /*uart1 cts uart1 rx*/
    	case 0x190 ... 0x193:   /*mcbsp1_dx mcbsp1_dr*/
    	case 0x198 ... 0x19b:   
    	case 0x19c ... 0x19f:   	
    	case 0x1a0 ... 0x1a3:   
		case 0x1e0 ... 0x1e3:    /*sys nirq sys_clkout2*/

		case 0x260 ... 0x263:    /*sys nirq sys_clkout2*/
		case 0x264 ... 0x267:    /*sys nirq sys_clkout2*/
    	
    	case 0xa04 ... 0xa07:   
    	case 0xa08 ... 0xa0b:    /*sys_boot0*/
    	case 0xa0c ... 0xa0f:    /*sys_boot1 sys_boot2*/
    	case 0xa10 ... 0xa13:    /*sys_boot3 sys_boot4*/
    	case 0xa14 ... 0xa17:    /*sys_boot5 sys_boot6*/
    	case 0xa1c ... 0xa1f:    /*JTAG_nTRST JTAG_TCK*/
    	case 0xa20 ... 0xa23:    /*JTAG_TMS JTAG_TDI*/
    	case 0xa24 ... 0xa27:    /*JTAG_EMU0 JTAG_EMU1*/
    	case 0xa28 ... 0xa2b:   
    	case 0xa2c ... 0xa2f:   
    	case 0xa30 ... 0xa33:   
    	case 0xa40 ... 0xa43:
    	case 0xa44 ... 0xa47:
    	case 0xa48 ... 0xa4b:

	   		break;
    	default:
    		printf("omap3_scm_write8 addr %x \n",addr);
     		exit(-1);
    }
}
static void omap3_scm_write16(void *opaque, target_phys_addr_t addr,
                uint32_t value)
{
	omap3_scm_write8(opaque, addr + 0, (value ) & 0xff);
   omap3_scm_write8(opaque, addr + 1, (value >> 8) & 0xff);
}
static void omap3_scm_write32(void *opaque, target_phys_addr_t addr,
                uint32_t value)
{
	omap3_scm_write8(opaque, addr + 0, (value ) & 0xff);
   omap3_scm_write8(opaque, addr + 1, (value >> 8) & 0xff);
   omap3_scm_write8(opaque, addr + 2, (value >>16) & 0xff);
   omap3_scm_write8(opaque, addr + 3, (value >> 24) & 0xff);
}

static CPUReadMemoryFunc *omap3_scm_readfn[] = {
    omap3_scm_read8,
    omap3_scm_read16,
    omap3_scm_read32,
};

static CPUWriteMemoryFunc *omap3_scm_writefn[] = {
    omap3_scm_write8,
    omap3_scm_write16,
    omap3_scm_write32,
};

struct omap3_scm_s *omap3_scm_init(struct omap_target_agent_s *ta, 
	 						                                        struct omap_mpu_state_s *mpu)
{
	 int iomemtype;
    struct omap3_scm_s *s = (struct omap3_scm_s *)
            qemu_mallocz(sizeof(*s));

    s->mpu = mpu;

    omap3_scm_reset(s);

    iomemtype = l4_register_io_memory(0, omap3_scm_readfn,
                    omap3_scm_writefn, s);
    s->base = omap_l4_attach(ta, 0, iomemtype);
    return s;
}


/*dummy port protection*/
struct omap3_pm_s {
	 target_phys_addr_t base;
	 uint32_t size;
    struct omap_mpu_state_s *mpu;

	uint32_t l3_pm_rt_error_log;                            /*0x6801 0020*/
	uint32_t l3_pm_rt_control;                                 /*0x6801 0028*/
	uint32_t l3_pm_rt_error_clear_single;             /*0x6801 0030*/
	uint32_t l3_pm_rt_error_clear_multi;             /*0x6801 0038*/
	uint32_t l3_pm_rt_req_info_permission[2];     /*0x6801 0048 + (0x20*i)*/
	uint32_t l3_pm_rt_read_permission[2];            /*0x6801 0050 + (0x20*i)*/
	uint32_t l3_pm_rt_write_permission[2];           /*0x6801 0058 + (0x20*i)*/
	uint32_t l3_pm_rt_addr_match[1];                    /*0x6801 0060 + (0x20*k)*/

	uint32_t l3_pm_gpmc_error_log;                            /*0x6801 2420*/
	uint32_t l3_pm_gpmc_control;                                 /*0x6801 2428*/
	uint32_t l3_pm_gpmc_error_clear_single;             /*0x6801 2430*/
	uint32_t l3_pm_gpmc_error_clear_multi;             /*0x6801 2438*/
	uint32_t l3_pm_gpmc_req_info_permission[8];     /*0x6801 2448 + (0x20*i)*/
	uint32_t l3_pm_gpmc_read_permission[8];            /*0x6801 2450 + (0x20*i)*/
	uint32_t l3_pm_gpmc_write_permission[8];           /*0x6801 2458 + (0x20*i)*/
	uint32_t l3_pm_gpmc_addr_match[7];                    /*0x6801 2460 + (0x20*k)*/

	uint32_t l3_pm_ocmram_error_log;                            /*0x6801 2820*/
	uint32_t l3_pm_ocmram_control;                                 /*0x6801 2828*/
	uint32_t l3_pm_ocmram_error_clear_single;             /*0x6801 2830*/
	uint32_t l3_pm_ocmram_error_clear_multi;             /*0x6801 2838*/
	uint32_t l3_pm_ocmram_req_info_permission[8];     /*0x6801 2848 + (0x20*i)*/
	uint32_t l3_pm_ocmram_read_permission[8];            /*0x6801 2850 + (0x20*i)*/
	uint32_t l3_pm_ocmram_write_permission[8];           /*0x6801 2858 + (0x20*i)*/
	uint32_t l3_pm_ocmram_addr_match[7];                    /*0x6801 2860 + (0x20*k)*/

	uint32_t l3_pm_ocmrom_error_log;                            /*0x6801 2c20*/
	uint32_t l3_pm_ocmrom_control;                                 /*0x6801 2c28*/
	uint32_t l3_pm_ocmrom_error_clear_single;             /*0x6801 2c30*/
	uint32_t l3_pm_ocmrom_error_clear_multi;             /*0x6801 2c38*/
	uint32_t l3_pm_ocmrom_req_info_permission[2];     /*0x6801 2c48 + (0x20*i)*/
	uint32_t l3_pm_ocmrom_read_permission[2];            /*0x6801 2c50 + (0x20*i)*/
	uint32_t l3_pm_ocmrom_write_permission[2];           /*0x6801 2c58 + (0x20*i)*/
	uint32_t l3_pm_ocmrom_addr_match[1];                    /*0x6801 2c60 + (0x20*k)*/

	uint32_t l3_pm_mad2d_error_log;                            /*0x6801 3020*/
	uint32_t l3_pm_mad2d_control;                                 /*0x6801 3028*/
	uint32_t l3_pm_mad2d_error_clear_single;             /*0x6801 3030*/
	uint32_t l3_pm_mad2d_error_clear_multi;             /*0x6801 3038*/
	uint32_t l3_pm_mad2d_req_info_permission[8];     /*0x6801 3048 + (0x20*i)*/
	uint32_t l3_pm_mad2d_read_permission[8];            /*0x6801 3050 + (0x20*i)*/
	uint32_t l3_pm_mad2d_write_permission[8];           /*0x6801 3058 + (0x20*i)*/
	uint32_t l3_pm_mad2d_addr_match[7];                    /*0x6801 3060 + (0x20*k)*/

	uint32_t l3_pm_iva_error_log;                            /*0x6801 4020*/
	uint32_t l3_pm_iva_control;                                 /*0x6801 4028*/
	uint32_t l3_pm_iva_error_clear_single;             /*0x6801 4030*/
	uint32_t l3_pm_iva_error_clear_multi;             /*0x6801 4038*/
	uint32_t l3_pm_iva_req_info_permission[4];     /*0x6801 4048 + (0x20*i)*/
	uint32_t l3_pm_iva_read_permission[4];            /*0x6801 4050 + (0x20*i)*/
	uint32_t l3_pm_iva_write_permission[4];           /*0x6801 4058 + (0x20*i)*/
	uint32_t l3_pm_iva_addr_match[3];                    /*0x6801 4060 + (0x20*k)*/

	
};
static void omap3_pm_reset(struct omap3_pm_s *s)
{
	int i;
	
	s->l3_pm_rt_control = 0x3000000;
	s->l3_pm_gpmc_control = 0x3000000;
	s->l3_pm_ocmram_control = 0x3000000;
	s->l3_pm_ocmrom_control = 0x3000000;
	s->l3_pm_mad2d_control = 0x3000000;
	s->l3_pm_iva_control = 0x3000000;

	s->l3_pm_rt_req_info_permission[0] = 0xffff;
	s->l3_pm_rt_req_info_permission[1] = 0x0;
	for (i=3;i<8;i++)
		s->l3_pm_gpmc_req_info_permission[i] = 0xffff; 
	for (i=1;i<8;i++)
		s->l3_pm_ocmram_req_info_permission[i] = 0xffff; 
	s->l3_pm_ocmrom_req_info_permission[1] = 0xffff;
	for (i=1;i<8;i++)
		s->l3_pm_mad2d_req_info_permission[i] = 0xffff; 
	for (i=1;i<4;i++)
		s->l3_pm_iva_req_info_permission[i] = 0xffff; 

	s->l3_pm_rt_read_permission[0] = 0x1406;
	s->l3_pm_rt_read_permission[1] = 0x1406;
	s->l3_pm_rt_write_permission[0] = 0x1406;
	s->l3_pm_rt_write_permission[1] = 0x1406;
	for (i=0;i<8;i++)
	{
		s->l3_pm_gpmc_read_permission[i] = 0x563e;
		s->l3_pm_gpmc_write_permission[i] = 0x563e;
	}
	for (i=0;i<8;i++)
	{
		s->l3_pm_ocmram_read_permission[i] = 0x5f3e;
		s->l3_pm_ocmram_write_permission[i] = 0x5f3e;
	}
	for (i=0;i<2;i++)
	{
		s->l3_pm_ocmrom_read_permission[i] = 0x1002;
		s->l3_pm_ocmrom_write_permission[i] = 0x1002;
	}
		
	for (i=0;i<8;i++)
	{
		s->l3_pm_mad2d_read_permission[i] = 0x5f1e;
		s->l3_pm_mad2d_write_permission[i] = 0x5f1e;
	}
		
	for (i=0;i<4;i++)
	{
		s->l3_pm_iva_read_permission[i] = 0x140e;
		s->l3_pm_iva_write_permission[i] = 0x140e;
	}
		

	s->l3_pm_rt_addr_match[0] = 0x10230;
	
	s->l3_pm_gpmc_addr_match[0] = 0x10230;


}

static uint32_t omap3_pm_read8(void *opaque, target_phys_addr_t addr)
{	
	struct omap3_pm_s *s = (struct omap3_pm_s *) opaque;
    int offset = addr - s->base;

    switch (offset)
    {

    	default:
    		printf("omap3_pm_read8 addr %x \n",addr);
     		exit(-1);
    }
}



static uint32_t omap3_pm_read16(void *opaque, target_phys_addr_t addr)
{
    uint32_t v;
    v = omap3_pm_read8(opaque, addr);
    v |= omap3_pm_read8(opaque, addr + 1) << 8;
    return v;
}
static uint32_t omap3_pm_read32(void *opaque, target_phys_addr_t addr)
{
    uint32_t v;
    v = omap3_pm_read8(opaque, addr);
    v |= omap3_pm_read8(opaque, addr + 1) << 8;
    v |= omap3_pm_read8(opaque, addr + 2) << 16;
    v |= omap3_pm_read8(opaque, addr + 3) << 24;
    return v;
}

static void omap3_pm_write8(void *opaque, target_phys_addr_t addr,
                uint32_t value)
{
	struct omap3_pm_s *s = (struct omap3_pm_s *) opaque;
    int offset = addr - s->base;
    int i;

    switch (offset)
    {
    	case 0x48 ... 0x4b:
    	case 0x68 ... 0x6b:
    		i = (offset-0x48)/0x20;
    		s->l3_pm_rt_req_info_permission[i] &= (~(0xff<<((offset-0x48-i*0x20)*8)));
    		s->l3_pm_rt_req_info_permission[i] |= (value<<(offset-0x48-i*0x20)*8);
    		break;
    	case 0x50 ...  0x53:
    	case 0x70 ... 0x73:
    		i = (offset-0x50)/0x20;
    		s->l3_pm_rt_read_permission[i] &= (~(0xff<<((offset-0x50-i*0x20)*8)));
    		s->l3_pm_rt_read_permission[i] |= (value<<(offset-0x50-i*0x20)*8);
    		break;
    	case 0x58 ...  0x5b:
    	case 0x78 ... 0x7b:
    		i = (offset-0x58)/0x20;
    		s->l3_pm_rt_write_permission[i] &= (~(0xff<<((offset-0x58-i*0x20)*8)));
    		s->l3_pm_rt_write_permission[i] |= (value<<(offset-0x58-i*0x20)*8);
    		break;
    	case 0x60 ... 0x63:
    		s->l3_pm_rt_addr_match[0] &= (~(0xff<<((offset-0x60)*8)));
    		s->l3_pm_rt_addr_match[0] |= (value<<(offset-0x60)*8);
    		break;
    	case 0x2448 ... 0x244b:
    	case 0x2468 ... 0x246b:
    	case 0x2488 ... 0x248b:
    	case 0x24a8 ... 0x24ab:
    	case 0x24c8 ... 0x24cb:
    	case 0x24e8 ... 0x24eb:
    	case 0x2508 ... 0x250b:
    	case 0x2528 ... 0x252b:
    		i = (offset-0x2448)/0x20;
    		s->l3_pm_gpmc_req_info_permission[i] &= (~(0xff<<((offset-0x2448-i*0x20)*8)));
    		s->l3_pm_gpmc_req_info_permission[i] |= (value<<(offset-0x2448-i*0x20)*8);
    		break;
    	case 0x2450 ...  0x2453:
    	case 0x2470 ... 0x2473:
    	case 0x2490 ... 0x2493:
    	case 0x24b0 ... 0x24b3:
    	case 0x24d0 ... 0x24d3:
    	case 0x24f0 ... 0x24f3:
    	case 0x2510 ... 0x2513:
    	case 0x2530 ... 0x2533:
    		i = (offset-0x2450)/0x20;
    		s->l3_pm_gpmc_read_permission[i] &= (~(0xff<<((offset-0x2450-i*0x20)*8)));
    		s->l3_pm_gpmc_read_permission[i] |= (value<<(offset-0x2450-i*0x20)*8);
    		break;
    	case 0x2458 ... 0x245b:
    	case 0x2478 ... 0x247b:
    	case 0x2498 ... 0x249b:
    	case 0x24b8 ... 0x24bb:
    	case 0x24d8 ... 0x24db:
    	case 0x24f8 ... 0x24fb:
    	case 0x2518 ... 0x251b:
    	case 0x2538 ... 0x253b:
    		i = (offset-0x2458)/0x20;
    		s->l3_pm_gpmc_write_permission[i] &= (~(0xff<<((offset-0x2458-i*0x20)*8)));
    		s->l3_pm_gpmc_write_permission[i] |= (value<<(offset-0x2458-i*0x20)*8);
    		break;
    	case 0x2848 ... 0x284b:
    	case 0x2868 ... 0x286b:
    	case 0x2888 ... 0x288b:
    	case 0x28a8 ... 0x28ab:
    	case 0x28c8 ... 0x28cb:
    	case 0x28e8 ... 0x28eb:
    	case 0x2908 ... 0x290b:
    	case 0x2928 ... 0x292b:
    		i = (offset-0x2848)/0x20;
    		s->l3_pm_ocmram_req_info_permission[i] &= (~(0xff<<((offset-0x2848-i*0x20)*8)));
    		s->l3_pm_ocmram_req_info_permission[i] |= (value<<(offset-0x2848-i*0x20)*8);
    		break;
    	case 0x2850 ... 0x2853:
    	case 0x2870 ... 0x2873:
    	case 0x2890 ... 0x2893:
    	case 0x28b0 ... 0x28b3:
    	case 0x28d0 ... 0x28d3:
    	case 0x28f0 ... 0x28f3:
    	case 0x2910 ... 0x2913:
    	case 0x2930 ... 0x2933:
    		i = (offset-0x2850)/0x20;
    		s->l3_pm_ocmram_read_permission[i] &= (~(0xff<<((offset-0x2850-i*0x20)*8)));
    		s->l3_pm_ocmram_read_permission[i] |= (value<<(offset-0x2850-i*0x20)*8);
    		break;
    	case 0x2858 ... 0x285b:
    	case 0x2878 ... 0x287b:
    	case 0x2898 ... 0x289b:
    	case 0x28b8 ... 0x28bb:
    	case 0x28d8 ... 0x28db:
    	case 0x28f8 ... 0x28fb:
    	case 0x2918 ... 0x291b:
    	case 0x2938 ... 0x293b:
    		i = (offset-0x2858)/0x20;
    		s->l3_pm_ocmram_write_permission[i] &= (~(0xff<<((offset-0x2858-i*0x20)*8)));
    		s->l3_pm_ocmram_write_permission[i] |= (value<<(offset-0x2858-i*0x20)*8);
    		break;

    	case 0x2860 ... 0x2863:
    	case 0x2880 ... 0x2883:
    	case 0x28a0 ... 0x28a3:
    	case 0x28c0 ... 0x28c3:
    	case 0x28e0 ... 0x28e3:
    	case 0x2900 ... 0x2903:
    	case 0x2920 ... 0x2923:
    		i = (offset-0x2860)/0x20;
    		s->l3_pm_ocmram_addr_match[i] &= (~(0xff<<((offset-0x2860-i*0x20)*8)));
    		s->l3_pm_ocmram_addr_match[i] |= (value<<(offset-0x2860-i*0x20)*8);
    		break;

    	case 0x4048 ... 0x404b:
    	case 0x4068 ... 0x406b:
    	case 0x4088 ... 0x408b:
    	case 0x40a8 ... 0x40ab:
    		i = (offset-0x4048)/0x20;
    		s->l3_pm_iva_req_info_permission[i] &= (~(0xff<<((offset-0x4048-i*0x20)*8)));
    		s->l3_pm_iva_req_info_permission[i] |= (value<<(offset-0x4048-i*0x20)*8);
    		break;
    	case 0x4050 ...  0x4053:
    	case 0x4070 ... 0x4073:
    	case 0x4090 ... 0x4093:
    	case 0x40b0 ... 0x40b3:
    		i = (offset-0x4050)/0x20;
    		s->l3_pm_iva_read_permission[i] &= (~(0xff<<((offset-0x4050-i*0x20)*8)));
    		s->l3_pm_iva_read_permission[i] |= (value<<(offset-0x4050-i*0x20)*8);
    		break;
    	case 0x4058 ...  0x405b:
    	case 0x4078 ... 0x407b:
    	case 0x4098 ... 0x409b:
    	case 0x40b8 ... 0x40bb:
    		i = (offset-0x4058)/0x20;
    		s->l3_pm_iva_write_permission[i] &= (~(0xff<<((offset-0x4058-i*0x20)*8)));
    		s->l3_pm_iva_write_permission[i] |= (value<<(offset-0x4058-i*0x20)*8);
    		break;		
    	default:
    		printf("omap3_pm_write8 addr %x \n",addr);
     		exit(-1);
    }
}
static void omap3_pm_write16(void *opaque, target_phys_addr_t addr,
                uint32_t value)
{
	omap3_pm_write8(opaque, addr + 0, (value ) & 0xff);
   omap3_pm_write8(opaque, addr + 1, (value >> 8) & 0xff);
}
static void omap3_pm_write32(void *opaque, target_phys_addr_t addr,
                uint32_t value)
{
	omap3_pm_write8(opaque, addr + 0, (value ) & 0xff);
   omap3_pm_write8(opaque, addr + 1, (value >> 8) & 0xff);
   omap3_pm_write8(opaque, addr + 2, (value >>16) & 0xff);
   omap3_pm_write8(opaque, addr + 3, (value >> 24) & 0xff);
}

static CPUReadMemoryFunc *omap3_pm_readfn[] = {
    omap3_pm_read8,
    omap3_pm_read16,
    omap3_pm_read32,
};

static CPUWriteMemoryFunc *omap3_pm_writefn[] = {
    omap3_pm_write8,
    omap3_pm_write16,
    omap3_pm_write32,
};

struct omap3_pm_s *omap3_pm_init(struct omap_mpu_state_s *mpu)
{
	 int iomemtype;
    struct omap3_pm_s *s = (struct omap3_pm_s *)qemu_mallocz(sizeof(*s));

    s->mpu = mpu;
    s->base = 0x68010000;
    s->size = 0x4400;

    omap3_pm_reset(s);

    iomemtype = l4_register_io_memory(0, omap3_pm_readfn,
                    omap3_pm_writefn, s);
    cpu_register_physical_memory( s->base, s->size, iomemtype);
    
    return s;
}

/*dummy SDRAM Memory Scheduler emulation*/
struct omap3_sms_s {
	 target_phys_addr_t base;
	 uint32_t size;
    struct omap_mpu_state_s *mpu;

	
};

static uint32_t omap3_sms_read32(void *opaque, target_phys_addr_t addr)
{	
	struct omap3_sms_s *s = (struct omap3_sms_s *) opaque;
    int offset = addr - s->base;

    switch (offset)
    {

    	default:
    		printf("omap3_sms_read32 addr %x \n",addr);
     		exit(-1);
    }
}

static void omap3_sms_write32(void *opaque, target_phys_addr_t addr,
                uint32_t value)
{
	struct omap3_sms_s *s = (struct omap3_sms_s *) opaque;
    int offset = addr - s->base;
    //int i;

    switch (offset)
    {
    	case 0x48:
    		break;
    	default:
    		printf("omap3_sms_write32 addr %x\n",addr);
    		exit(-1);
    }
}

static CPUReadMemoryFunc *omap3_sms_readfn[] = {
    omap_badwidth_read32,
    omap_badwidth_read32,
    omap3_sms_read32,
};

static CPUWriteMemoryFunc *omap3_sms_writefn[] = {
    omap_badwidth_write32,
    omap_badwidth_write32,
    omap3_sms_write32,
};
struct omap3_sms_s *omap3_sms_init(struct omap_mpu_state_s *mpu)
{
	 int iomemtype;
    struct omap3_sms_s *s = (struct omap3_sms_s *)qemu_mallocz(sizeof(*s));

    s->mpu = mpu;
    s->base = 0x6c000000;
    s->size = 0x10000;


    iomemtype = l4_register_io_memory(0, omap3_sms_readfn,
                    omap3_sms_writefn, s);
    cpu_register_physical_memory( s->base, s->size, iomemtype);
    
    return s;
}

static const struct dma_irq_map omap3_dma_irq_map[] = {
    { 0, OMAP_INT_35XX_SDMA_IRQ0 },
    { 0, OMAP_INT_35XX_SDMA_IRQ1 },
    { 0, OMAP_INT_35XX_SDMA_IRQ2 },
    { 0, OMAP_INT_35XX_SDMA_IRQ3 },
};
static int omap3_validate_addr(struct omap_mpu_state_s *s,
                target_phys_addr_t addr)
{
    return 1;
}
struct omap_mpu_state_s *omap3530_mpu_init(unsigned long sdram_size,
                DisplayState *ds, const char *core)
{
	struct omap_mpu_state_s *s = (struct omap_mpu_state_s *)
            qemu_mallocz(sizeof(struct omap_mpu_state_s));
	ram_addr_t sram_base, q2_base;
	qemu_irq *cpu_irq;
	qemu_irq dma_irqs[4];
	int i;
	

    s->mpu_model = omap3530;
    s->env = cpu_init("omap3530");
    if (!s->env) {
        fprintf(stderr, "Unable to find CPU definition\n");
        exit(1);
    }
    s->sdram_size = sdram_size;
    s->sram_size = OMAP3530_SRAM_SIZE;

    /* Clocks */
    omap_clk_init(s);

     /* Memory-mapped stuff */
     q2_base = qemu_ram_alloc(s->sdram_size);
    cpu_register_physical_memory(OMAP3_Q2_BASE, s->sdram_size,
                    (q2_base | IO_MEM_RAM));
    sram_base = qemu_ram_alloc(s->sram_size);
    cpu_register_physical_memory(OMAP3_SRAM_BASE, s->sram_size,
                    ( sram_base| IO_MEM_RAM));

    s->l4 = omap_l4_init(OMAP3_L4_BASE, sizeof(omap3_l4_agent_info));
    
    cpu_irq = arm_pic_init_cpu(s->env);
    s->ih[0] = omap2_inth_init(0x480fe000, 0x1000, 3, &s->irq[0],
                    cpu_irq[ARM_PIC_CPU_IRQ], cpu_irq[ARM_PIC_CPU_FIQ],
                    omap_findclk(s, "omap3_mpu_intc_fclk"),
                    omap_findclk(s, "omap3_mpu_intc_iclk"));

    for (i = 0; i < 4; i ++)
        dma_irqs[i] =
                s->irq[omap3_dma_irq_map[i].ih][omap3_dma_irq_map[i].intr];
    s->dma = omap_dma4_init(0x48056000, dma_irqs, s, 256, 32,
                    omap_findclk(s, "omap3_sdma_fclk"),
                    omap_findclk(s, "omap3_sdma_iclk"));
    s->port->addr_valid = omap3_validate_addr;

    /* Register SDRAM and SRAM ports for fast DMA transfers.  */
    soc_dma_port_add_mem_ram(s->dma, q2_base, OMAP2_Q2_BASE, s->sdram_size);
    soc_dma_port_add_mem_ram(s->dma, sram_base, OMAP2_SRAM_BASE, s->sram_size);


	 s->omap3_cm = omap3_cm_init(omap3_l4ta_get(s->l4, 1),
                   NULL, NULL, NULL, s);

    s->omap3_prm = omap3_prm_init(omap3_l4ta_get(s->l4, 2),
                   NULL, NULL, NULL, s);

    s->omap3_mpu_wdt = omap3_mpu_wdt_init(omap3_l4ta_get(s->l4, 3),
    										NULL,
    										omap_findclk(s,"omap3_wkup_32k_fclk"),
    										omap_findclk(s,"omap3_wkup_l4_iclk"),s);

    s->omap3_scm = omap3_scm_init(omap3_l4ta_get(s->l4, 4),s);

    s->omap3_pm = omap3_pm_init(s);
    s->omap3_sms = omap3_sms_init(s);		

    s->gptimer[0] = omap_gp_timer_init(omap3_l4ta_get(s->l4, 5),
                    s->irq[0][OMAP_INT_35XX_GPTIMER1],
                    omap_findclk(s, "omap3_gp1_fclk"),
                    omap_findclk(s, "omap3_wkup_l4_iclk"));

    omap_synctimer_init(omap3_l4ta_get(s->l4, 6),s,
                    omap_findclk(s, "omap3_sys_32k"),
                    NULL);

    s->sdrc = omap_sdrc_init(0x6d000000);
    //s->gpmc = omap_gpmc_init(0x6e000000, s->irq[0][OMAP_INT_35XX_GPMC_IRQ]);

    s->uart[0] = omap2_uart_init(omap3_l4ta_get(s->l4, 7),
                    s->irq[0][OMAP_INT_35XX_UART1_IRQ],
                    omap_findclk(s, "omap3_uart1_fclk"),
                    omap_findclk(s, "omap3_uart1_iclk"),
                    s->drq[OMAP24XX_DMA_UART1_TX],
                    s->drq[OMAP24XX_DMA_UART1_RX], serial_hds[0]);
    s->uart[1] = omap2_uart_init(omap3_l4ta_get(s->l4, 8),
                    s->irq[0][OMAP_INT_35XX_UART2_IRQ],
                    omap_findclk(s, "omap3_uart2_fclk"),
                    omap_findclk(s, "omap3_uart2_iclk"),
                    s->drq[OMAP24XX_DMA_UART2_TX],
                    s->drq[OMAP24XX_DMA_UART2_RX],
                    serial_hds[0] ? serial_hds[1] : 0);
    s->uart[2] = omap2_uart_init(omap3_l4ta_get(s->l4, 9),
                    s->irq[0][OMAP_INT_35XX_UART3_IRQ],
                    omap_findclk(s, "omap3_uart2_fclk"),
                    omap_findclk(s, "omap3_uart3_iclk"),
                    s->drq[OMAP24XX_DMA_UART3_TX],
                    s->drq[OMAP24XX_DMA_UART3_RX],
                    serial_hds[0] && serial_hds[1] ? serial_hds[2] : 0);
    /*attach serial[0] to uart 2 for beagle board*/
    omap_uart_attach(s->uart[2], serial_hds[0]);

    

    return s;    
}


