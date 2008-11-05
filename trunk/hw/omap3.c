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

	[ 84] = { 0x318000, 0x1000, 32 | 16    },      /* WDTIMER1  */
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
    uint32_t ret;

     switch (offset) 
     {
     	case 0x1270:
     		return s->prm_clksrc_ctrl;
     	default:
     		printf("omap3_prm_read addr %x \n",addr);
     		exit(-1);
     }
}

static void omap3_prm_write(void *opaque, target_phys_addr_t addr,
                uint32_t value)
{
    struct omap3_prm_s *s = (struct omap3_prm_s *) opaque;
    int offset = addr - s->base;

    switch (offset) 
     {
     	case 0x1270:
     		s->prm_clksrc_ctrl = value &(0xd8);
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


static void omap3_cm_reset(struct omap3_cm_s *s)
{
	s->cm_fclken_iva2 = 0x0;
	s->cm_clken_pll_iva2 = 0x11;
	s->cm_idlest_iva2 = 0x1;
	s->cm_idlest_pll_iva2 = 0x0;
	s->cm_autoidle_pll_iva2 = 0x0;
	s->cm_clksel1_pll_iva2 = 0x8000;
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

     switch (offset) 
     {
     	//case 0x1270:
     	//	return s->prm_clksrc_ctrl;
     	default:
     		printf("omap3_cm_read addr %x \n",addr);
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
     	//case 0x1270:
     	//	s->prm_clksrc_ctrl = value &(0xd8);
     	//	break;
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






struct omap_mpu_state_s *omap3530_mpu_init(unsigned long sdram_size,
                DisplayState *ds, const char *core)
{
	struct omap_mpu_state_s *s = (struct omap_mpu_state_s *)
            qemu_mallocz(sizeof(struct omap_mpu_state_s));
	ram_addr_t sram_base, q2_base;
	

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

    printf("sram_base %x\n",sram_base);

    s->l4 = omap_l4_init(OMAP3_L4_BASE, sizeof(omap3_l4_agent_info));

    s->omap3_prm = omap3_prm_init(omap3_l4ta_get(s->l4, 2),
                   NULL, NULL, NULL, s);
    s->omap3_cm = omap3_cm_init(omap3_l4ta_get(s->l4, 1),
                   NULL, NULL, NULL, s);

    return s;    
}

