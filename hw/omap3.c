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
 	[  0] = { 0x002000, 0x1000, 32 | 16 | 8 }, /* System Control module */
 	[  1] = { 0x003000, 0x1000, 32 | 16 | 8 }, /* L4TA1 */
 	[  2] = { 0x320000, 0x1000, 32 | 16     }, /* 32K Timer */
 	[  3] = { 0x321000, 0x1000, 32 | 16 | 8 }, /*  L4TA2 */
 	[  4] = { 0x306000, 0x2000, 32},          /*PRM Region A*/
 	[  5] = { 0x308000, 0x0800, 32},          /*PRM Region B*/
 	[  6] = { 0x309000, 0x1000, 32 | 16 | 8},    /*  L4TA3 */
 	
};
static struct omap_l4_agent_info_s omap3_l4_agent_info[ ] = 
{
	{0, 0, 2, 1 }, 			/* System Control module */
	{1, 2, 2, 1 }, 			/* 32K timer */
	{2, 4, 3, 2 }, 			/* PRM */
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

static void omap3_prm_coldreset(struct omap3_prm_s *s)
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
    omap3_prm_coldreset(s);

    iomemtype = l4_register_io_memory(0, omap3_prm_readfn,
                    omap3_prm_writefn, s);
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

    //omap_synctimer_init(omap3_l4ta_get(s->l4, 1), s,
     //              omap_findclk(s, "omap3_sys_32k"),
      //              NULL);
	

    return s;    
}

