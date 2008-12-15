/*
 *  PowerPC emulation helpers for qemu.
 *
 *  Copyright (c) 2003-2007 Jocelyn Mayer
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "exec.h"
#include "host-utils.h"
#include "helper.h"

#include "helper_regs.h"
#include "op_helper.h"

#define MEMSUFFIX _raw
#include "op_helper.h"
#include "op_helper_mem.h"
#if !defined(CONFIG_USER_ONLY)
#define MEMSUFFIX _user
#include "op_helper.h"
#include "op_helper_mem.h"
#define MEMSUFFIX _kernel
#include "op_helper.h"
#include "op_helper_mem.h"
#define MEMSUFFIX _hypv
#include "op_helper.h"
#include "op_helper_mem.h"
#endif

//#define DEBUG_OP
//#define DEBUG_EXCEPTIONS
//#define DEBUG_SOFTWARE_TLB

/*****************************************************************************/
/* Exceptions processing helpers */

void helper_raise_exception_err (uint32_t exception, uint32_t error_code)
{
    raise_exception_err(env, exception, error_code);
}

void helper_raise_debug (void)
{
    raise_exception(env, EXCP_DEBUG);
}


/*****************************************************************************/
/* Registers load and stores */
target_ulong helper_load_cr (void)
{
    return (env->crf[0] << 28) |
           (env->crf[1] << 24) |
           (env->crf[2] << 20) |
           (env->crf[3] << 16) |
           (env->crf[4] << 12) |
           (env->crf[5] << 8) |
           (env->crf[6] << 4) |
           (env->crf[7] << 0);
}

void helper_store_cr (target_ulong val, uint32_t mask)
{
    int i, sh;

    for (i = 0, sh = 7; i < 8; i++, sh--) {
        if (mask & (1 << sh))
            env->crf[i] = (val >> (sh * 4)) & 0xFUL;
    }
}

#if defined(TARGET_PPC64)
void do_store_pri (int prio)
{
    env->spr[SPR_PPR] &= ~0x001C000000000000ULL;
    env->spr[SPR_PPR] |= ((uint64_t)prio & 0x7) << 50;
}
#endif

target_ulong ppc_load_dump_spr (int sprn)
{
    if (loglevel != 0) {
        fprintf(logfile, "Read SPR %d %03x => " ADDRX "\n",
                sprn, sprn, env->spr[sprn]);
    }

    return env->spr[sprn];
}

void ppc_store_dump_spr (int sprn, target_ulong val)
{
    if (loglevel != 0) {
        fprintf(logfile, "Write SPR %d %03x => " ADDRX " <= " ADDRX "\n",
                sprn, sprn, env->spr[sprn], val);
    }
    env->spr[sprn] = val;
}

/*****************************************************************************/
/* Fixed point operations helpers */
#if defined(TARGET_PPC64)

/* multiply high word */
uint64_t helper_mulhd (uint64_t arg1, uint64_t arg2)
{
    uint64_t tl, th;

    muls64(&tl, &th, arg1, arg2);
    return th;
}

/* multiply high word unsigned */
uint64_t helper_mulhdu (uint64_t arg1, uint64_t arg2)
{
    uint64_t tl, th;

    mulu64(&tl, &th, arg1, arg2);
    return th;
}

uint64_t helper_mulldo (uint64_t arg1, uint64_t arg2)
{
    int64_t th;
    uint64_t tl;

    muls64(&tl, (uint64_t *)&th, arg1, arg2);
    /* If th != 0 && th != -1, then we had an overflow */
    if (likely((uint64_t)(th + 1) <= 1)) {
        env->xer &= ~(1 << XER_OV);
    } else {
        env->xer |= (1 << XER_OV) | (1 << XER_SO);
    }
    return (int64_t)tl;
}
#endif

target_ulong helper_cntlzw (target_ulong t)
{
    return clz32(t);
}

#if defined(TARGET_PPC64)
target_ulong helper_cntlzd (target_ulong t)
{
    return clz64(t);
}
#endif

/* shift right arithmetic helper */
target_ulong helper_sraw (target_ulong value, target_ulong shift)
{
    int32_t ret;

    if (likely(!(shift & 0x20))) {
        if (likely((uint32_t)shift != 0)) {
            shift &= 0x1f;
            ret = (int32_t)value >> shift;
            if (likely(ret >= 0 || (value & ((1 << shift) - 1)) == 0)) {
                env->xer &= ~(1 << XER_CA);
            } else {
                env->xer |= (1 << XER_CA);
            }
        } else {
            ret = (int32_t)value;
            env->xer &= ~(1 << XER_CA);
        }
    } else {
        ret = (int32_t)value >> 31;
        if (ret) {
            env->xer |= (1 << XER_CA);
        } else {
            env->xer &= ~(1 << XER_CA);
        }
    }
    return (target_long)ret;
}

#if defined(TARGET_PPC64)
target_ulong helper_srad (target_ulong value, target_ulong shift)
{
    int64_t ret;

    if (likely(!(shift & 0x40))) {
        if (likely((uint64_t)shift != 0)) {
            shift &= 0x3f;
            ret = (int64_t)value >> shift;
            if (likely(ret >= 0 || (value & ((1 << shift) - 1)) == 0)) {
                env->xer &= ~(1 << XER_CA);
            } else {
                env->xer |= (1 << XER_CA);
            }
        } else {
            ret = (int64_t)value;
            env->xer &= ~(1 << XER_CA);
        }
    } else {
        ret = (int64_t)value >> 63;
        if (ret) {
            env->xer |= (1 << XER_CA);
        } else {
            env->xer &= ~(1 << XER_CA);
        }
    }
    return ret;
}
#endif

target_ulong helper_popcntb (target_ulong val)
{
    val = (val & 0x55555555) + ((val >>  1) & 0x55555555);
    val = (val & 0x33333333) + ((val >>  2) & 0x33333333);
    val = (val & 0x0f0f0f0f) + ((val >>  4) & 0x0f0f0f0f);
    return val;
}

#if defined(TARGET_PPC64)
target_ulong helper_popcntb_64 (target_ulong val)
{
    val = (val & 0x5555555555555555ULL) + ((val >>  1) & 0x5555555555555555ULL);
    val = (val & 0x3333333333333333ULL) + ((val >>  2) & 0x3333333333333333ULL);
    val = (val & 0x0f0f0f0f0f0f0f0fULL) + ((val >>  4) & 0x0f0f0f0f0f0f0f0fULL);
    return val;
}
#endif

/*****************************************************************************/
/* Floating point operations helpers */
uint64_t helper_float32_to_float64(uint32_t arg)
{
    CPU_FloatU f;
    CPU_DoubleU d;
    f.l = arg;
    d.d = float32_to_float64(f.f, &env->fp_status);
    return d.ll;
}

uint32_t helper_float64_to_float32(uint64_t arg)
{
    CPU_FloatU f;
    CPU_DoubleU d;
    d.ll = arg;
    f.f = float64_to_float32(d.d, &env->fp_status);
    return f.l;
}

static always_inline int fpisneg (float64 d)
{
    CPU_DoubleU u;

    u.d = d;

    return u.ll >> 63 != 0;
}

static always_inline int isden (float64 d)
{
    CPU_DoubleU u;

    u.d = d;

    return ((u.ll >> 52) & 0x7FF) == 0;
}

static always_inline int iszero (float64 d)
{
    CPU_DoubleU u;

    u.d = d;

    return (u.ll & ~0x8000000000000000ULL) == 0;
}

static always_inline int isinfinity (float64 d)
{
    CPU_DoubleU u;

    u.d = d;

    return ((u.ll >> 52) & 0x7FF) == 0x7FF &&
        (u.ll & 0x000FFFFFFFFFFFFFULL) == 0;
}

#ifdef CONFIG_SOFTFLOAT
static always_inline int isfinite (float64 d)
{
    CPU_DoubleU u;

    u.d = d;

    return (((u.ll >> 52) & 0x7FF) != 0x7FF);
}

static always_inline int isnormal (float64 d)
{
    CPU_DoubleU u;

    u.d = d;

    uint32_t exp = (u.ll >> 52) & 0x7FF;
    return ((0 < exp) && (exp < 0x7FF));
}
#endif

uint32_t helper_compute_fprf (uint64_t arg, uint32_t set_fprf)
{
    CPU_DoubleU farg;
    int isneg;
    int ret;
    farg.ll = arg;
    isneg = fpisneg(farg.d);
    if (unlikely(float64_is_nan(farg.d))) {
        if (float64_is_signaling_nan(farg.d)) {
            /* Signaling NaN: flags are undefined */
            ret = 0x00;
        } else {
            /* Quiet NaN */
            ret = 0x11;
        }
    } else if (unlikely(isinfinity(farg.d))) {
        /* +/- infinity */
        if (isneg)
            ret = 0x09;
        else
            ret = 0x05;
    } else {
        if (iszero(farg.d)) {
            /* +/- zero */
            if (isneg)
                ret = 0x12;
            else
                ret = 0x02;
        } else {
            if (isden(farg.d)) {
                /* Denormalized numbers */
                ret = 0x10;
            } else {
                /* Normalized numbers */
                ret = 0x00;
            }
            if (isneg) {
                ret |= 0x08;
            } else {
                ret |= 0x04;
            }
        }
    }
    if (set_fprf) {
        /* We update FPSCR_FPRF */
        env->fpscr &= ~(0x1F << FPSCR_FPRF);
        env->fpscr |= ret << FPSCR_FPRF;
    }
    /* We just need fpcc to update Rc1 */
    return ret & 0xF;
}

/* Floating-point invalid operations exception */
static always_inline uint64_t fload_invalid_op_excp (int op)
{
    uint64_t ret = 0;
    int ve;

    ve = fpscr_ve;
    if (op & POWERPC_EXCP_FP_VXSNAN) {
        /* Operation on signaling NaN */
        env->fpscr |= 1 << FPSCR_VXSNAN;
    }
    if (op & POWERPC_EXCP_FP_VXSOFT) {
        /* Software-defined condition */
        env->fpscr |= 1 << FPSCR_VXSOFT;
    }
    switch (op & ~(POWERPC_EXCP_FP_VXSOFT | POWERPC_EXCP_FP_VXSNAN)) {
    case POWERPC_EXCP_FP_VXISI:
        /* Magnitude subtraction of infinities */
        env->fpscr |= 1 << FPSCR_VXISI;
        goto update_arith;
    case POWERPC_EXCP_FP_VXIDI:
        /* Division of infinity by infinity */
        env->fpscr |= 1 << FPSCR_VXIDI;
        goto update_arith;
    case POWERPC_EXCP_FP_VXZDZ:
        /* Division of zero by zero */
        env->fpscr |= 1 << FPSCR_VXZDZ;
        goto update_arith;
    case POWERPC_EXCP_FP_VXIMZ:
        /* Multiplication of zero by infinity */
        env->fpscr |= 1 << FPSCR_VXIMZ;
        goto update_arith;
    case POWERPC_EXCP_FP_VXVC:
        /* Ordered comparison of NaN */
        env->fpscr |= 1 << FPSCR_VXVC;
        env->fpscr &= ~(0xF << FPSCR_FPCC);
        env->fpscr |= 0x11 << FPSCR_FPCC;
        /* We must update the target FPR before raising the exception */
        if (ve != 0) {
            env->exception_index = POWERPC_EXCP_PROGRAM;
            env->error_code = POWERPC_EXCP_FP | POWERPC_EXCP_FP_VXVC;
            /* Update the floating-point enabled exception summary */
            env->fpscr |= 1 << FPSCR_FEX;
            /* Exception is differed */
            ve = 0;
        }
        break;
    case POWERPC_EXCP_FP_VXSQRT:
        /* Square root of a negative number */
        env->fpscr |= 1 << FPSCR_VXSQRT;
    update_arith:
        env->fpscr &= ~((1 << FPSCR_FR) | (1 << FPSCR_FI));
        if (ve == 0) {
            /* Set the result to quiet NaN */
            ret = UINT64_MAX;
            env->fpscr &= ~(0xF << FPSCR_FPCC);
            env->fpscr |= 0x11 << FPSCR_FPCC;
        }
        break;
    case POWERPC_EXCP_FP_VXCVI:
        /* Invalid conversion */
        env->fpscr |= 1 << FPSCR_VXCVI;
        env->fpscr &= ~((1 << FPSCR_FR) | (1 << FPSCR_FI));
        if (ve == 0) {
            /* Set the result to quiet NaN */
            ret = UINT64_MAX;
            env->fpscr &= ~(0xF << FPSCR_FPCC);
            env->fpscr |= 0x11 << FPSCR_FPCC;
        }
        break;
    }
    /* Update the floating-point invalid operation summary */
    env->fpscr |= 1 << FPSCR_VX;
    /* Update the floating-point exception summary */
    env->fpscr |= 1 << FPSCR_FX;
    if (ve != 0) {
        /* Update the floating-point enabled exception summary */
        env->fpscr |= 1 << FPSCR_FEX;
        if (msr_fe0 != 0 || msr_fe1 != 0)
            raise_exception_err(env, POWERPC_EXCP_PROGRAM, POWERPC_EXCP_FP | op);
    }
    return ret;
}

static always_inline uint64_t float_zero_divide_excp (uint64_t arg1, uint64_t arg2)
{
    env->fpscr |= 1 << FPSCR_ZX;
    env->fpscr &= ~((1 << FPSCR_FR) | (1 << FPSCR_FI));
    /* Update the floating-point exception summary */
    env->fpscr |= 1 << FPSCR_FX;
    if (fpscr_ze != 0) {
        /* Update the floating-point enabled exception summary */
        env->fpscr |= 1 << FPSCR_FEX;
        if (msr_fe0 != 0 || msr_fe1 != 0) {
            raise_exception_err(env, POWERPC_EXCP_PROGRAM,
                                POWERPC_EXCP_FP | POWERPC_EXCP_FP_ZX);
        }
    } else {
        /* Set the result to infinity */
        arg1 = ((arg1 ^ arg2) & 0x8000000000000000ULL);
        arg1 |= 0x7FFULL << 52;
    }
    return arg1;
}

static always_inline void float_overflow_excp (void)
{
    env->fpscr |= 1 << FPSCR_OX;
    /* Update the floating-point exception summary */
    env->fpscr |= 1 << FPSCR_FX;
    if (fpscr_oe != 0) {
        /* XXX: should adjust the result */
        /* Update the floating-point enabled exception summary */
        env->fpscr |= 1 << FPSCR_FEX;
        /* We must update the target FPR before raising the exception */
        env->exception_index = POWERPC_EXCP_PROGRAM;
        env->error_code = POWERPC_EXCP_FP | POWERPC_EXCP_FP_OX;
    } else {
        env->fpscr |= 1 << FPSCR_XX;
        env->fpscr |= 1 << FPSCR_FI;
    }
}

static always_inline void float_underflow_excp (void)
{
    env->fpscr |= 1 << FPSCR_UX;
    /* Update the floating-point exception summary */
    env->fpscr |= 1 << FPSCR_FX;
    if (fpscr_ue != 0) {
        /* XXX: should adjust the result */
        /* Update the floating-point enabled exception summary */
        env->fpscr |= 1 << FPSCR_FEX;
        /* We must update the target FPR before raising the exception */
        env->exception_index = POWERPC_EXCP_PROGRAM;
        env->error_code = POWERPC_EXCP_FP | POWERPC_EXCP_FP_UX;
    }
}

static always_inline void float_inexact_excp (void)
{
    env->fpscr |= 1 << FPSCR_XX;
    /* Update the floating-point exception summary */
    env->fpscr |= 1 << FPSCR_FX;
    if (fpscr_xe != 0) {
        /* Update the floating-point enabled exception summary */
        env->fpscr |= 1 << FPSCR_FEX;
        /* We must update the target FPR before raising the exception */
        env->exception_index = POWERPC_EXCP_PROGRAM;
        env->error_code = POWERPC_EXCP_FP | POWERPC_EXCP_FP_XX;
    }
}

static always_inline void fpscr_set_rounding_mode (void)
{
    int rnd_type;

    /* Set rounding mode */
    switch (fpscr_rn) {
    case 0:
        /* Best approximation (round to nearest) */
        rnd_type = float_round_nearest_even;
        break;
    case 1:
        /* Smaller magnitude (round toward zero) */
        rnd_type = float_round_to_zero;
        break;
    case 2:
        /* Round toward +infinite */
        rnd_type = float_round_up;
        break;
    default:
    case 3:
        /* Round toward -infinite */
        rnd_type = float_round_down;
        break;
    }
    set_float_rounding_mode(rnd_type, &env->fp_status);
}

void helper_fpscr_setbit (uint32_t bit)
{
    int prev;

    prev = (env->fpscr >> bit) & 1;
    env->fpscr |= 1 << bit;
    if (prev == 0) {
        switch (bit) {
        case FPSCR_VX:
            env->fpscr |= 1 << FPSCR_FX;
            if (fpscr_ve)
                goto raise_ve;
        case FPSCR_OX:
            env->fpscr |= 1 << FPSCR_FX;
            if (fpscr_oe)
                goto raise_oe;
            break;
        case FPSCR_UX:
            env->fpscr |= 1 << FPSCR_FX;
            if (fpscr_ue)
                goto raise_ue;
            break;
        case FPSCR_ZX:
            env->fpscr |= 1 << FPSCR_FX;
            if (fpscr_ze)
                goto raise_ze;
            break;
        case FPSCR_XX:
            env->fpscr |= 1 << FPSCR_FX;
            if (fpscr_xe)
                goto raise_xe;
            break;
        case FPSCR_VXSNAN:
        case FPSCR_VXISI:
        case FPSCR_VXIDI:
        case FPSCR_VXZDZ:
        case FPSCR_VXIMZ:
        case FPSCR_VXVC:
        case FPSCR_VXSOFT:
        case FPSCR_VXSQRT:
        case FPSCR_VXCVI:
            env->fpscr |= 1 << FPSCR_VX;
            env->fpscr |= 1 << FPSCR_FX;
            if (fpscr_ve != 0)
                goto raise_ve;
            break;
        case FPSCR_VE:
            if (fpscr_vx != 0) {
            raise_ve:
                env->error_code = POWERPC_EXCP_FP;
                if (fpscr_vxsnan)
                    env->error_code |= POWERPC_EXCP_FP_VXSNAN;
                if (fpscr_vxisi)
                    env->error_code |= POWERPC_EXCP_FP_VXISI;
                if (fpscr_vxidi)
                    env->error_code |= POWERPC_EXCP_FP_VXIDI;
                if (fpscr_vxzdz)
                    env->error_code |= POWERPC_EXCP_FP_VXZDZ;
                if (fpscr_vximz)
                    env->error_code |= POWERPC_EXCP_FP_VXIMZ;
                if (fpscr_vxvc)
                    env->error_code |= POWERPC_EXCP_FP_VXVC;
                if (fpscr_vxsoft)
                    env->error_code |= POWERPC_EXCP_FP_VXSOFT;
                if (fpscr_vxsqrt)
                    env->error_code |= POWERPC_EXCP_FP_VXSQRT;
                if (fpscr_vxcvi)
                    env->error_code |= POWERPC_EXCP_FP_VXCVI;
                goto raise_excp;
            }
            break;
        case FPSCR_OE:
            if (fpscr_ox != 0) {
            raise_oe:
                env->error_code = POWERPC_EXCP_FP | POWERPC_EXCP_FP_OX;
                goto raise_excp;
            }
            break;
        case FPSCR_UE:
            if (fpscr_ux != 0) {
            raise_ue:
                env->error_code = POWERPC_EXCP_FP | POWERPC_EXCP_FP_UX;
                goto raise_excp;
            }
            break;
        case FPSCR_ZE:
            if (fpscr_zx != 0) {
            raise_ze:
                env->error_code = POWERPC_EXCP_FP | POWERPC_EXCP_FP_ZX;
                goto raise_excp;
            }
            break;
        case FPSCR_XE:
            if (fpscr_xx != 0) {
            raise_xe:
                env->error_code = POWERPC_EXCP_FP | POWERPC_EXCP_FP_XX;
                goto raise_excp;
            }
            break;
        case FPSCR_RN1:
        case FPSCR_RN:
            fpscr_set_rounding_mode();
            break;
        default:
            break;
        raise_excp:
            /* Update the floating-point enabled exception summary */
            env->fpscr |= 1 << FPSCR_FEX;
                /* We have to update Rc1 before raising the exception */
            env->exception_index = POWERPC_EXCP_PROGRAM;
            break;
        }
    }
}

void helper_store_fpscr (uint64_t arg, uint32_t mask)
{
    /*
     * We use only the 32 LSB of the incoming fpr
     */
    uint32_t prev, new;
    int i;

    prev = env->fpscr;
    new = (uint32_t)arg;
    new &= ~0x90000000;
    new |= prev & 0x90000000;
    for (i = 0; i < 7; i++) {
        if (mask & (1 << i)) {
            env->fpscr &= ~(0xF << (4 * i));
            env->fpscr |= new & (0xF << (4 * i));
        }
    }
    /* Update VX and FEX */
    if (fpscr_ix != 0)
        env->fpscr |= 1 << FPSCR_VX;
    else
        env->fpscr &= ~(1 << FPSCR_VX);
    if ((fpscr_ex & fpscr_eex) != 0) {
        env->fpscr |= 1 << FPSCR_FEX;
        env->exception_index = POWERPC_EXCP_PROGRAM;
        /* XXX: we should compute it properly */
        env->error_code = POWERPC_EXCP_FP;
    }
    else
        env->fpscr &= ~(1 << FPSCR_FEX);
    fpscr_set_rounding_mode();
}

void helper_float_check_status (void)
{
#ifdef CONFIG_SOFTFLOAT
    if (env->exception_index == POWERPC_EXCP_PROGRAM &&
        (env->error_code & POWERPC_EXCP_FP)) {
        /* Differred floating-point exception after target FPR update */
        if (msr_fe0 != 0 || msr_fe1 != 0)
            raise_exception_err(env, env->exception_index, env->error_code);
    } else if (env->fp_status.float_exception_flags & float_flag_overflow) {
        float_overflow_excp();
    } else if (env->fp_status.float_exception_flags & float_flag_underflow) {
        float_underflow_excp();
    } else if (env->fp_status.float_exception_flags & float_flag_inexact) {
        float_inexact_excp();
    }
#else
    if (env->exception_index == POWERPC_EXCP_PROGRAM &&
        (env->error_code & POWERPC_EXCP_FP)) {
        /* Differred floating-point exception after target FPR update */
        if (msr_fe0 != 0 || msr_fe1 != 0)
            raise_exception_err(env, env->exception_index, env->error_code);
    }
    RETURN();
#endif
}

#ifdef CONFIG_SOFTFLOAT
void helper_reset_fpstatus (void)
{
    env->fp_status.float_exception_flags = 0;
}
#endif

/* fadd - fadd. */
uint64_t helper_fadd (uint64_t arg1, uint64_t arg2)
{
    CPU_DoubleU farg1, farg2;

    farg1.ll = arg1;
    farg2.ll = arg2;
#if USE_PRECISE_EMULATION
    if (unlikely(float64_is_signaling_nan(farg1.d) ||
                 float64_is_signaling_nan(farg2.d))) {
        /* sNaN addition */
        farg1.ll = fload_invalid_op_excp(POWERPC_EXCP_FP_VXSNAN);
    } else if (likely(isfinite(farg1.d) || isfinite(farg2.d) ||
                      fpisneg(farg1.d) == fpisneg(farg2.d))) {
        farg1.d = float64_add(farg1.d, farg2.d, &env->fp_status);
    } else {
        /* Magnitude subtraction of infinities */
        farg1.ll == fload_invalid_op_excp(POWERPC_EXCP_FP_VXISI);
    }
#else
    farg1.d = float64_add(farg1.d, farg2.d, &env->fp_status);
#endif
    return farg1.ll;
}

/* fsub - fsub. */
uint64_t helper_fsub (uint64_t arg1, uint64_t arg2)
{
    CPU_DoubleU farg1, farg2;

    farg1.ll = arg1;
    farg2.ll = arg2;
#if USE_PRECISE_EMULATION
{
    if (unlikely(float64_is_signaling_nan(farg1.d) ||
                 float64_is_signaling_nan(farg2.d))) {
        /* sNaN subtraction */
        farg1.ll = fload_invalid_op_excp(POWERPC_EXCP_FP_VXSNAN);
    } else if (likely(isfinite(farg1.d) || isfinite(farg2.d) ||
                      fpisneg(farg1.d) != fpisneg(farg2.d))) {
        farg1.d = float64_sub(farg1.d, farg2.d, &env->fp_status);
    } else {
        /* Magnitude subtraction of infinities */
        farg1.ll = fload_invalid_op_excp(POWERPC_EXCP_FP_VXISI);
    }
}
#else
    farg1.d = float64_sub(farg1.d, farg2.d, &env->fp_status);
#endif
    return farg1.ll;
}

/* fmul - fmul. */
uint64_t helper_fmul (uint64_t arg1, uint64_t arg2)
{
    CPU_DoubleU farg1, farg2;

    farg1.ll = arg1;
    farg2.ll = arg2;
#if USE_PRECISE_EMULATION
    if (unlikely(float64_is_signaling_nan(farg1.d) ||
                 float64_is_signaling_nan(farg2.d))) {
        /* sNaN multiplication */
        farg1.ll = fload_invalid_op_excp(POWERPC_EXCP_FP_VXSNAN);
    } else if (unlikely((isinfinity(farg1.d) && iszero(farg2.d)) ||
                        (iszero(farg1.d) && isinfinity(farg2.d)))) {
        /* Multiplication of zero by infinity */
        farg1.ll = fload_invalid_op_excp(POWERPC_EXCP_FP_VXIMZ);
    } else {
        farg1.d = float64_mul(farg1.d, farg2.d, &env->fp_status);
    }
}
#else
    farg1.d = float64_mul(farg1.d, farg2.d, &env->fp_status);
#endif
    return farg1.ll;
}

/* fdiv - fdiv. */
uint64_t helper_fdiv (uint64_t arg1, uint64_t arg2)
{
    CPU_DoubleU farg1, farg2;

    farg1.ll = arg1;
    farg2.ll = arg2;
#if USE_PRECISE_EMULATION
    if (unlikely(float64_is_signaling_nan(farg1.d) ||
                 float64_is_signaling_nan(farg2.d))) {
        /* sNaN division */
        farg1.ll = fload_invalid_op_excp(POWERPC_EXCP_FP_VXSNAN);
    } else if (unlikely(isinfinity(farg1.d) && isinfinity(farg2.d))) {
        /* Division of infinity by infinity */
        farg1.ll = fload_invalid_op_excp(POWERPC_EXCP_FP_VXIDI);
    } else if (unlikely(iszero(farg2.d))) {
        if (iszero(farg1.d)) {
            /* Division of zero by zero */
            farg1.ll fload_invalid_op_excp(POWERPC_EXCP_FP_VXZDZ);
        } else {
            /* Division by zero */
            farg1.ll = float_zero_divide_excp(farg1.d, farg2.d);
        }
    } else {
        farg1.d = float64_div(farg1.d, farg2.d, &env->fp_status);
    }
#else
    farg1.d = float64_div(farg1.d, farg2.d, &env->fp_status);
#endif
    return farg1.ll;
}

/* fabs */
uint64_t helper_fabs (uint64_t arg)
{
    CPU_DoubleU farg;

    farg.ll = arg;
    farg.d = float64_abs(farg.d);
    return farg.ll;
}

/* fnabs */
uint64_t helper_fnabs (uint64_t arg)
{
    CPU_DoubleU farg;

    farg.ll = arg;
    farg.d = float64_abs(farg.d);
    farg.d = float64_chs(farg.d);
    return farg.ll;
}

/* fneg */
uint64_t helper_fneg (uint64_t arg)
{
    CPU_DoubleU farg;

    farg.ll = arg;
    farg.d = float64_chs(farg.d);
    return farg.ll;
}

/* fctiw - fctiw. */
uint64_t helper_fctiw (uint64_t arg)
{
    CPU_DoubleU farg;
    farg.ll = arg;

    if (unlikely(float64_is_signaling_nan(farg.d))) {
        /* sNaN conversion */
        farg.ll = fload_invalid_op_excp(POWERPC_EXCP_FP_VXSNAN | POWERPC_EXCP_FP_VXCVI);
    } else if (unlikely(float64_is_nan(farg.d) || isinfinity(farg.d))) {
        /* qNan / infinity conversion */
        farg.ll = fload_invalid_op_excp(POWERPC_EXCP_FP_VXCVI);
    } else {
        farg.ll = float64_to_int32(farg.d, &env->fp_status);
#if USE_PRECISE_EMULATION
        /* XXX: higher bits are not supposed to be significant.
         *     to make tests easier, return the same as a real PowerPC 750
         */
        farg.ll |= 0xFFF80000ULL << 32;
#endif
    }
    return farg.ll;
}

/* fctiwz - fctiwz. */
uint64_t helper_fctiwz (uint64_t arg)
{
    CPU_DoubleU farg;
    farg.ll = arg;

    if (unlikely(float64_is_signaling_nan(farg.d))) {
        /* sNaN conversion */
        farg.ll = fload_invalid_op_excp(POWERPC_EXCP_FP_VXSNAN | POWERPC_EXCP_FP_VXCVI);
    } else if (unlikely(float64_is_nan(farg.d) || isinfinity(farg.d))) {
        /* qNan / infinity conversion */
        farg.ll = fload_invalid_op_excp(POWERPC_EXCP_FP_VXCVI);
    } else {
        farg.ll = float64_to_int32_round_to_zero(farg.d, &env->fp_status);
#if USE_PRECISE_EMULATION
        /* XXX: higher bits are not supposed to be significant.
         *     to make tests easier, return the same as a real PowerPC 750
         */
        farg.ll |= 0xFFF80000ULL << 32;
#endif
    }
    return farg.ll;
}

#if defined(TARGET_PPC64)
/* fcfid - fcfid. */
uint64_t helper_fcfid (uint64_t arg)
{
    CPU_DoubleU farg;
    farg.d = int64_to_float64(arg, &env->fp_status);
    return farg.ll;
}

/* fctid - fctid. */
uint64_t helper_fctid (uint64_t arg)
{
    CPU_DoubleU farg;
    farg.ll = arg;

    if (unlikely(float64_is_signaling_nan(farg.d))) {
        /* sNaN conversion */
        farg.ll = fload_invalid_op_excp(POWERPC_EXCP_FP_VXSNAN | POWERPC_EXCP_FP_VXCVI);
    } else if (unlikely(float64_is_nan(farg.d) || isinfinity(farg.d))) {
        /* qNan / infinity conversion */
        farg.ll = fload_invalid_op_excp(POWERPC_EXCP_FP_VXCVI);
    } else {
        farg.ll = float64_to_int64(farg.d, &env->fp_status);
    }
    return farg.ll;
}

/* fctidz - fctidz. */
uint64_t helper_fctidz (uint64_t arg)
{
    CPU_DoubleU farg;
    farg.ll = arg;

    if (unlikely(float64_is_signaling_nan(farg.d))) {
        /* sNaN conversion */
        farg.ll = fload_invalid_op_excp(POWERPC_EXCP_FP_VXSNAN | POWERPC_EXCP_FP_VXCVI);
    } else if (unlikely(float64_is_nan(farg.d) || isinfinity(farg.d))) {
        /* qNan / infinity conversion */
        farg.ll = fload_invalid_op_excp(POWERPC_EXCP_FP_VXCVI);
    } else {
        farg.ll = float64_to_int64_round_to_zero(farg.d, &env->fp_status);
    }
    return farg.ll;
}

#endif

static always_inline uint64_t do_fri (uint64_t arg, int rounding_mode)
{
    CPU_DoubleU farg;
    farg.ll = arg;

    if (unlikely(float64_is_signaling_nan(farg.d))) {
        /* sNaN round */
        farg.ll = fload_invalid_op_excp(POWERPC_EXCP_FP_VXSNAN | POWERPC_EXCP_FP_VXCVI);
    } else if (unlikely(float64_is_nan(farg.d) || isinfinity(farg.d))) {
        /* qNan / infinity round */
        farg.ll = fload_invalid_op_excp(POWERPC_EXCP_FP_VXCVI);
    } else {
        set_float_rounding_mode(rounding_mode, &env->fp_status);
        farg.ll = float64_round_to_int(farg.d, &env->fp_status);
        /* Restore rounding mode from FPSCR */
        fpscr_set_rounding_mode();
    }
    return farg.ll;
}

uint64_t helper_frin (uint64_t arg)
{
    return do_fri(arg, float_round_nearest_even);
}

uint64_t helper_friz (uint64_t arg)
{
    return do_fri(arg, float_round_to_zero);
}

uint64_t helper_frip (uint64_t arg)
{
    return do_fri(arg, float_round_up);
}

uint64_t helper_frim (uint64_t arg)
{
    return do_fri(arg, float_round_down);
}

/* fmadd - fmadd. */
uint64_t helper_fmadd (uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
    CPU_DoubleU farg1, farg2, farg3;

    farg1.ll = arg1;
    farg2.ll = arg2;
    farg3.ll = arg3;
#if USE_PRECISE_EMULATION
    if (unlikely(float64_is_signaling_nan(farg1.d) ||
                 float64_is_signaling_nan(farg2.d) ||
                 float64_is_signaling_nan(farg3.d))) {
        /* sNaN operation */
        farg1.ll = fload_invalid_op_excp(POWERPC_EXCP_FP_VXSNAN);
    } else {
#ifdef FLOAT128
        /* This is the way the PowerPC specification defines it */
        float128 ft0_128, ft1_128;

        ft0_128 = float64_to_float128(farg1.d, &env->fp_status);
        ft1_128 = float64_to_float128(farg2.d, &env->fp_status);
        ft0_128 = float128_mul(ft0_128, ft1_128, &env->fp_status);
        ft1_128 = float64_to_float128(farg3.d, &env->fp_status);
        ft0_128 = float128_add(ft0_128, ft1_128, &env->fp_status);
        farg1.d = float128_to_float64(ft0_128, &env->fp_status);
#else
        /* This is OK on x86 hosts */
        farg1.d = (farg1.d * farg2.d) + farg3.d;
#endif
    }
#else
    farg1.d = float64_mul(farg1.d, farg2.d, &env->fp_status);
    farg1.d = float64_add(farg1.d, farg3.d, &env->fp_status);
#endif
    return farg1.ll;
}

/* fmsub - fmsub. */
uint64_t helper_fmsub (uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
    CPU_DoubleU farg1, farg2, farg3;

    farg1.ll = arg1;
    farg2.ll = arg2;
    farg3.ll = arg3;
#if USE_PRECISE_EMULATION
    if (unlikely(float64_is_signaling_nan(farg1.d) ||
                 float64_is_signaling_nan(farg2.d) ||
                 float64_is_signaling_nan(farg3.d))) {
        /* sNaN operation */
        farg1.ll = fload_invalid_op_excp(POWERPC_EXCP_FP_VXSNAN);
    } else {
#ifdef FLOAT128
        /* This is the way the PowerPC specification defines it */
        float128 ft0_128, ft1_128;

        ft0_128 = float64_to_float128(farg1.d, &env->fp_status);
        ft1_128 = float64_to_float128(farg2.d, &env->fp_status);
        ft0_128 = float128_mul(ft0_128, ft1_128, &env->fp_status);
        ft1_128 = float64_to_float128(farg3.d, &env->fp_status);
        ft0_128 = float128_sub(ft0_128, ft1_128, &env->fp_status);
        farg1.d = float128_to_float64(ft0_128, &env->fp_status);
#else
        /* This is OK on x86 hosts */
        farg1.d = (farg1.d * farg2.d) - farg3.d;
#endif
    }
#else
    farg1.d = float64_mul(farg1.d, farg2.d, &env->fp_status);
    farg1.d = float64_sub(farg1.d, farg3.d, &env->fp_status);
#endif
    return farg1.ll;
}

/* fnmadd - fnmadd. */
uint64_t helper_fnmadd (uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
    CPU_DoubleU farg1, farg2, farg3;

    farg1.ll = arg1;
    farg2.ll = arg2;
    farg3.ll = arg3;

    if (unlikely(float64_is_signaling_nan(farg1.d) ||
                 float64_is_signaling_nan(farg2.d) ||
                 float64_is_signaling_nan(farg3.d))) {
        /* sNaN operation */
        farg1.ll = fload_invalid_op_excp(POWERPC_EXCP_FP_VXSNAN);
    } else {
#if USE_PRECISE_EMULATION
#ifdef FLOAT128
        /* This is the way the PowerPC specification defines it */
        float128 ft0_128, ft1_128;

        ft0_128 = float64_to_float128(farg1.d, &env->fp_status);
        ft1_128 = float64_to_float128(farg2.d, &env->fp_status);
        ft0_128 = float128_mul(ft0_128, ft1_128, &env->fp_status);
        ft1_128 = float64_to_float128(farg3.d, &env->fp_status);
        ft0_128 = float128_add(ft0_128, ft1_128, &env->fp_status);
        farg1.d= float128_to_float64(ft0_128, &env->fp_status);
#else
        /* This is OK on x86 hosts */
        farg1.d = (farg1.d * farg2.d) + farg3.d;
#endif
#else
        farg1.d = float64_mul(farg1.d, farg2.d, &env->fp_status);
        farg1.d = float64_add(farg1.d, farg3.d, &env->fp_status);
#endif
        if (likely(!isnan(farg1.d)))
            farg1.d = float64_chs(farg1.d);
    }
    return farg1.ll;
}

/* fnmsub - fnmsub. */
uint64_t helper_fnmsub (uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
    CPU_DoubleU farg1, farg2, farg3;

    farg1.ll = arg1;
    farg2.ll = arg2;
    farg3.ll = arg3;

    if (unlikely(float64_is_signaling_nan(farg1.d) ||
                 float64_is_signaling_nan(farg2.d) ||
                 float64_is_signaling_nan(farg3.d))) {
        /* sNaN operation */
        farg1.ll = fload_invalid_op_excp(POWERPC_EXCP_FP_VXSNAN);
    } else {
#if USE_PRECISE_EMULATION
#ifdef FLOAT128
        /* This is the way the PowerPC specification defines it */
        float128 ft0_128, ft1_128;

        ft0_128 = float64_to_float128(farg1.d, &env->fp_status);
        ft1_128 = float64_to_float128(farg2.d, &env->fp_status);
        ft0_128 = float128_mul(ft0_128, ft1_128, &env->fp_status);
        ft1_128 = float64_to_float128(farg3.d, &env->fp_status);
        ft0_128 = float128_sub(ft0_128, ft1_128, &env->fp_status);
        farg1.d = float128_to_float64(ft0_128, &env->fp_status);
#else
        /* This is OK on x86 hosts */
        farg1.d = (farg1.d * farg2.d) - farg3.d;
#endif
#else
        farg1.d = float64_mul(farg1.d, farg2.d, &env->fp_status);
        farg1.d = float64_sub(farg1.d, farg3.d, &env->fp_status);
#endif
        if (likely(!isnan(farg1.d)))
            farg1.d = float64_chs(farg1.d);
    }
    return farg1.ll;
}


/* frsp - frsp. */
uint64_t helper_frsp (uint64_t arg)
{
    CPU_DoubleU farg;
    farg.ll = arg;

#if USE_PRECISE_EMULATION
    if (unlikely(float64_is_signaling_nan(farg.d))) {
        /* sNaN square root */
       farg.ll = fload_invalid_op_excp(POWERPC_EXCP_FP_VXSNAN);
    } else {
       fard.d = float64_to_float32(farg.d, &env->fp_status);
    }
#else
    farg.d = float64_to_float32(farg.d, &env->fp_status);
#endif
    return farg.ll;
}

/* fsqrt - fsqrt. */
uint64_t helper_fsqrt (uint64_t arg)
{
    CPU_DoubleU farg;
    farg.ll = arg;

    if (unlikely(float64_is_signaling_nan(farg.d))) {
        /* sNaN square root */
        farg.ll = fload_invalid_op_excp(POWERPC_EXCP_FP_VXSNAN);
    } else if (unlikely(fpisneg(farg.d) && !iszero(farg.d))) {
        /* Square root of a negative nonzero number */
        farg.ll = fload_invalid_op_excp(POWERPC_EXCP_FP_VXSQRT);
    } else {
        farg.d = float64_sqrt(farg.d, &env->fp_status);
    }
    return farg.ll;
}

/* fre - fre. */
uint64_t helper_fre (uint64_t arg)
{
    CPU_DoubleU farg;
    farg.ll = arg;

    if (unlikely(float64_is_signaling_nan(farg.d))) {
        /* sNaN reciprocal */
        farg.ll = fload_invalid_op_excp(POWERPC_EXCP_FP_VXSNAN);
    } else if (unlikely(iszero(farg.d))) {
        /* Zero reciprocal */
        farg.ll = float_zero_divide_excp(1.0, farg.d);
    } else if (likely(isnormal(farg.d))) {
        farg.d = float64_div(1.0, farg.d, &env->fp_status);
    } else {
        if (farg.ll == 0x8000000000000000ULL) {
            farg.ll = 0xFFF0000000000000ULL;
        } else if (farg.ll == 0x0000000000000000ULL) {
            farg.ll = 0x7FF0000000000000ULL;
        } else if (isnan(farg.d)) {
            farg.ll = 0x7FF8000000000000ULL;
        } else if (fpisneg(farg.d)) {
            farg.ll = 0x8000000000000000ULL;
        } else {
            farg.ll = 0x0000000000000000ULL;
        }
    }
    return farg.d;
}

/* fres - fres. */
uint64_t helper_fres (uint64_t arg)
{
    CPU_DoubleU farg;
    farg.ll = arg;

    if (unlikely(float64_is_signaling_nan(farg.d))) {
        /* sNaN reciprocal */
        farg.ll = fload_invalid_op_excp(POWERPC_EXCP_FP_VXSNAN);
    } else if (unlikely(iszero(farg.d))) {
        /* Zero reciprocal */
        farg.ll = float_zero_divide_excp(1.0, farg.d);
    } else if (likely(isnormal(farg.d))) {
#if USE_PRECISE_EMULATION
        farg.d = float64_div(1.0, farg.d, &env->fp_status);
        farg.d = float64_to_float32(farg.d, &env->fp_status);
#else
        farg.d = float32_div(1.0, farg.d, &env->fp_status);
#endif
    } else {
        if (farg.ll == 0x8000000000000000ULL) {
            farg.ll = 0xFFF0000000000000ULL;
        } else if (farg.ll == 0x0000000000000000ULL) {
            farg.ll = 0x7FF0000000000000ULL;
        } else if (isnan(farg.d)) {
            farg.ll = 0x7FF8000000000000ULL;
        } else if (fpisneg(farg.d)) {
            farg.ll = 0x8000000000000000ULL;
        } else {
            farg.ll = 0x0000000000000000ULL;
        }
    }
    return farg.ll;
}

/* frsqrte  - frsqrte. */
uint64_t helper_frsqrte (uint64_t arg)
{
    CPU_DoubleU farg;
    farg.ll = arg;

    if (unlikely(float64_is_signaling_nan(farg.d))) {
        /* sNaN reciprocal square root */
        farg.ll = fload_invalid_op_excp(POWERPC_EXCP_FP_VXSNAN);
    } else if (unlikely(fpisneg(farg.d) && !iszero(farg.d))) {
        /* Reciprocal square root of a negative nonzero number */
        farg.ll = fload_invalid_op_excp(POWERPC_EXCP_FP_VXSQRT);
    } else if (likely(isnormal(farg.d))) {
        farg.d = float64_sqrt(farg.d, &env->fp_status);
        farg.d = float32_div(1.0, farg.d, &env->fp_status);
    } else {
        if (farg.ll == 0x8000000000000000ULL) {
            farg.ll = 0xFFF0000000000000ULL;
        } else if (farg.ll == 0x0000000000000000ULL) {
            farg.ll = 0x7FF0000000000000ULL;
        } else if (isnan(farg.d)) {
            farg.ll |= 0x000FFFFFFFFFFFFFULL;
        } else if (fpisneg(farg.d)) {
            farg.ll = 0x7FF8000000000000ULL;
        } else {
            farg.ll = 0x0000000000000000ULL;
        }
    }
    return farg.ll;
}

/* fsel - fsel. */
uint64_t helper_fsel (uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
    CPU_DoubleU farg1, farg2, farg3;

    farg1.ll = arg1;
    farg2.ll = arg2;
    farg3.ll = arg3;

    if (!fpisneg(farg1.d) || iszero(farg1.d))
        return farg2.ll;
    else
        return farg2.ll;
}

uint32_t helper_fcmpu (uint64_t arg1, uint64_t arg2)
{
    CPU_DoubleU farg1, farg2;
    uint32_t ret = 0;
    farg1.ll = arg1;
    farg2.ll = arg2;

    if (unlikely(float64_is_signaling_nan(farg1.d) ||
                 float64_is_signaling_nan(farg2.d))) {
        /* sNaN comparison */
        fload_invalid_op_excp(POWERPC_EXCP_FP_VXSNAN);
    } else {
        if (float64_lt(farg1.d, farg2.d, &env->fp_status)) {
            ret = 0x08UL;
        } else if (!float64_le(farg1.d, farg2.d, &env->fp_status)) {
            ret = 0x04UL;
        } else {
            ret = 0x02UL;
        }
    }
    env->fpscr &= ~(0x0F << FPSCR_FPRF);
    env->fpscr |= ret << FPSCR_FPRF;
    return ret;
}

uint32_t helper_fcmpo (uint64_t arg1, uint64_t arg2)
{
    CPU_DoubleU farg1, farg2;
    uint32_t ret = 0;
    farg1.ll = arg1;
    farg2.ll = arg2;

    if (unlikely(float64_is_nan(farg1.d) ||
                 float64_is_nan(farg2.d))) {
        if (float64_is_signaling_nan(farg1.d) ||
            float64_is_signaling_nan(farg2.d)) {
            /* sNaN comparison */
            fload_invalid_op_excp(POWERPC_EXCP_FP_VXSNAN |
                                  POWERPC_EXCP_FP_VXVC);
        } else {
            /* qNaN comparison */
            fload_invalid_op_excp(POWERPC_EXCP_FP_VXVC);
        }
    } else {
        if (float64_lt(farg1.d, farg2.d, &env->fp_status)) {
            ret = 0x08UL;
        } else if (!float64_le(farg1.d, farg2.d, &env->fp_status)) {
            ret = 0x04UL;
        } else {
            ret = 0x02UL;
        }
    }
    env->fpscr &= ~(0x0F << FPSCR_FPRF);
    env->fpscr |= ret << FPSCR_FPRF;
    return ret;
}

#if !defined (CONFIG_USER_ONLY)
void cpu_dump_rfi (target_ulong RA, target_ulong msr);

void do_store_msr (void)
{
    T0 = hreg_store_msr(env, T0, 0);
    if (T0 != 0) {
        env->interrupt_request |= CPU_INTERRUPT_EXITTB;
        raise_exception(env, T0);
    }
}

static always_inline void __do_rfi (target_ulong nip, target_ulong msr,
                                    target_ulong msrm, int keep_msrh)
{
#if defined(TARGET_PPC64)
    if (msr & (1ULL << MSR_SF)) {
        nip = (uint64_t)nip;
        msr &= (uint64_t)msrm;
    } else {
        nip = (uint32_t)nip;
        msr = (uint32_t)(msr & msrm);
        if (keep_msrh)
            msr |= env->msr & ~((uint64_t)0xFFFFFFFF);
    }
#else
    nip = (uint32_t)nip;
    msr &= (uint32_t)msrm;
#endif
    /* XXX: beware: this is false if VLE is supported */
    env->nip = nip & ~((target_ulong)0x00000003);
    hreg_store_msr(env, msr, 1);
#if defined (DEBUG_OP)
    cpu_dump_rfi(env->nip, env->msr);
#endif
    /* No need to raise an exception here,
     * as rfi is always the last insn of a TB
     */
    env->interrupt_request |= CPU_INTERRUPT_EXITTB;
}

void do_rfi (void)
{
    __do_rfi(env->spr[SPR_SRR0], env->spr[SPR_SRR1],
             ~((target_ulong)0xFFFF0000), 1);
}

#if defined(TARGET_PPC64)
void do_rfid (void)
{
    __do_rfi(env->spr[SPR_SRR0], env->spr[SPR_SRR1],
             ~((target_ulong)0xFFFF0000), 0);
}

void do_hrfid (void)
{
    __do_rfi(env->spr[SPR_HSRR0], env->spr[SPR_HSRR1],
             ~((target_ulong)0xFFFF0000), 0);
}
#endif
#endif

void helper_tw (target_ulong arg1, target_ulong arg2, uint32_t flags)
{
    if (!likely(!(((int32_t)arg1 < (int32_t)arg2 && (flags & 0x10)) ||
                  ((int32_t)arg1 > (int32_t)arg2 && (flags & 0x08)) ||
                  ((int32_t)arg1 == (int32_t)arg2 && (flags & 0x04)) ||
                  ((uint32_t)arg1 < (uint32_t)arg2 && (flags & 0x02)) ||
                  ((uint32_t)arg1 > (uint32_t)arg2 && (flags & 0x01))))) {
        raise_exception_err(env, POWERPC_EXCP_PROGRAM, POWERPC_EXCP_TRAP);
    }
}

#if defined(TARGET_PPC64)
void helper_td (target_ulong arg1, target_ulong arg2, uint32_t flags)
{
    if (!likely(!(((int64_t)arg1 < (int64_t)arg2 && (flags & 0x10)) ||
                  ((int64_t)arg1 > (int64_t)arg2 && (flags & 0x08)) ||
                  ((int64_t)arg1 == (int64_t)arg2 && (flags & 0x04)) ||
                  ((uint64_t)arg1 < (uint64_t)arg2 && (flags & 0x02)) ||
                  ((uint64_t)arg1 > (uint64_t)arg2 && (flags & 0x01)))))
        raise_exception_err(env, POWERPC_EXCP_PROGRAM, POWERPC_EXCP_TRAP);
}
#endif

/*****************************************************************************/
/* PowerPC 601 specific instructions (POWER bridge) */
void do_POWER_abso (void)
{
    if ((int32_t)T0 == INT32_MIN) {
        T0 = INT32_MAX;
        env->xer |= (1 << XER_OV) | (1 << XER_SO);
    } else if ((int32_t)T0 < 0) {
        T0 = -T0;
        env->xer &= ~(1 << XER_OV);
    } else {
        env->xer &= ~(1 << XER_OV);
    }
}

void do_POWER_clcs (void)
{
    switch (T0) {
    case 0x0CUL:
        /* Instruction cache line size */
        T0 = env->icache_line_size;
        break;
    case 0x0DUL:
        /* Data cache line size */
        T0 = env->dcache_line_size;
        break;
    case 0x0EUL:
        /* Minimum cache line size */
        T0 = env->icache_line_size < env->dcache_line_size ?
            env->icache_line_size : env->dcache_line_size;
        break;
    case 0x0FUL:
        /* Maximum cache line size */
        T0 = env->icache_line_size > env->dcache_line_size ?
            env->icache_line_size : env->dcache_line_size;
        break;
    default:
        /* Undefined */
        break;
    }
}

void do_POWER_div (void)
{
    uint64_t tmp;

    if (((int32_t)T0 == INT32_MIN && (int32_t)T1 == (int32_t)-1) ||
        (int32_t)T1 == 0) {
        T0 = UINT32_MAX * ((uint32_t)T0 >> 31);
        env->spr[SPR_MQ] = 0;
    } else {
        tmp = ((uint64_t)T0 << 32) | env->spr[SPR_MQ];
        env->spr[SPR_MQ] = tmp % T1;
        T0 = tmp / (int32_t)T1;
    }
}

void do_POWER_divo (void)
{
    int64_t tmp;

    if (((int32_t)T0 == INT32_MIN && (int32_t)T1 == (int32_t)-1) ||
        (int32_t)T1 == 0) {
        T0 = UINT32_MAX * ((uint32_t)T0 >> 31);
        env->spr[SPR_MQ] = 0;
        env->xer |= (1 << XER_OV) | (1 << XER_SO);
    } else {
        tmp = ((uint64_t)T0 << 32) | env->spr[SPR_MQ];
        env->spr[SPR_MQ] = tmp % T1;
        tmp /= (int32_t)T1;
        if (tmp > (int64_t)INT32_MAX || tmp < (int64_t)INT32_MIN) {
            env->xer |= (1 << XER_OV) | (1 << XER_SO);
        } else {
            env->xer &= ~(1 << XER_OV);
        }
        T0 = tmp;
    }
}

void do_POWER_divs (void)
{
    if (((int32_t)T0 == INT32_MIN && (int32_t)T1 == (int32_t)-1) ||
        (int32_t)T1 == 0) {
        T0 = UINT32_MAX * ((uint32_t)T0 >> 31);
        env->spr[SPR_MQ] = 0;
    } else {
        env->spr[SPR_MQ] = T0 % T1;
        T0 = (int32_t)T0 / (int32_t)T1;
    }
}

void do_POWER_divso (void)
{
    if (((int32_t)T0 == INT32_MIN && (int32_t)T1 == (int32_t)-1) ||
        (int32_t)T1 == 0) {
        T0 = UINT32_MAX * ((uint32_t)T0 >> 31);
        env->spr[SPR_MQ] = 0;
        env->xer |= (1 << XER_OV) | (1 << XER_SO);
    } else {
        T0 = (int32_t)T0 / (int32_t)T1;
        env->spr[SPR_MQ] = (int32_t)T0 % (int32_t)T1;
        env->xer &= ~(1 << XER_OV);
    }
}

void do_POWER_dozo (void)
{
    if ((int32_t)T1 > (int32_t)T0) {
        T2 = T0;
        T0 = T1 - T0;
        if (((uint32_t)(~T2) ^ (uint32_t)T1 ^ UINT32_MAX) &
            ((uint32_t)(~T2) ^ (uint32_t)T0) & (1UL << 31)) {
            env->xer |= (1 << XER_OV) | (1 << XER_SO);
        } else {
            env->xer &= ~(1 << XER_OV);
        }
    } else {
        T0 = 0;
        env->xer &= ~(1 << XER_OV);
    }
}

void do_POWER_maskg (void)
{
    uint32_t ret;

    if ((uint32_t)T0 == (uint32_t)(T1 + 1)) {
        ret = UINT32_MAX;
    } else {
        ret = (UINT32_MAX >> ((uint32_t)T0)) ^
            ((UINT32_MAX >> ((uint32_t)T1)) >> 1);
        if ((uint32_t)T0 > (uint32_t)T1)
            ret = ~ret;
    }
    T0 = ret;
}

void do_POWER_mulo (void)
{
    uint64_t tmp;

    tmp = (uint64_t)T0 * (uint64_t)T1;
    env->spr[SPR_MQ] = tmp >> 32;
    T0 = tmp;
    if (tmp >> 32 != ((uint64_t)T0 >> 16) * ((uint64_t)T1 >> 16)) {
        env->xer |= (1 << XER_OV) | (1 << XER_SO);
    } else {
        env->xer &= ~(1 << XER_OV);
    }
}

#if !defined (CONFIG_USER_ONLY)
void do_POWER_rac (void)
{
    mmu_ctx_t ctx;
    int nb_BATs;

    /* We don't have to generate many instances of this instruction,
     * as rac is supervisor only.
     */
    /* XXX: FIX THIS: Pretend we have no BAT */
    nb_BATs = env->nb_BATs;
    env->nb_BATs = 0;
    if (get_physical_address(env, &ctx, T0, 0, ACCESS_INT) == 0)
        T0 = ctx.raddr;
    env->nb_BATs = nb_BATs;
}

void do_POWER_rfsvc (void)
{
    __do_rfi(env->lr, env->ctr, 0x0000FFFF, 0);
}

void do_store_hid0_601 (void)
{
    uint32_t hid0;

    hid0 = env->spr[SPR_HID0];
    if ((T0 ^ hid0) & 0x00000008) {
        /* Change current endianness */
        env->hflags &= ~(1 << MSR_LE);
        env->hflags_nmsr &= ~(1 << MSR_LE);
        env->hflags_nmsr |= (1 << MSR_LE) & (((T0 >> 3) & 1) << MSR_LE);
        env->hflags |= env->hflags_nmsr;
        if (loglevel != 0) {
            fprintf(logfile, "%s: set endianness to %c => " ADDRX "\n",
                    __func__, T0 & 0x8 ? 'l' : 'b', env->hflags);
        }
    }
    env->spr[SPR_HID0] = T0;
}
#endif

/*****************************************************************************/
/* 602 specific instructions */
/* mfrom is the most crazy instruction ever seen, imho ! */
/* Real implementation uses a ROM table. Do the same */
#define USE_MFROM_ROM_TABLE
void do_op_602_mfrom (void)
{
    if (likely(T0 < 602)) {
#if defined(USE_MFROM_ROM_TABLE)
#include "mfrom_table.c"
        T0 = mfrom_ROM_table[T0];
#else
        double d;
        /* Extremly decomposed:
         *                    -T0 / 256
         * T0 = 256 * log10(10          + 1.0) + 0.5
         */
        d = T0;
        d = float64_div(d, 256, &env->fp_status);
        d = float64_chs(d);
        d = exp10(d); // XXX: use float emulation function
        d = float64_add(d, 1.0, &env->fp_status);
        d = log10(d); // XXX: use float emulation function
        d = float64_mul(d, 256, &env->fp_status);
        d = float64_add(d, 0.5, &env->fp_status);
        T0 = float64_round_to_int(d, &env->fp_status);
#endif
    } else {
        T0 = 0;
    }
}

/*****************************************************************************/
/* Embedded PowerPC specific helpers */

/* XXX: to be improved to check access rights when in user-mode */
void do_load_dcr (void)
{
    target_ulong val;

    if (unlikely(env->dcr_env == NULL)) {
        if (loglevel != 0) {
            fprintf(logfile, "No DCR environment\n");
        }
        raise_exception_err(env, POWERPC_EXCP_PROGRAM,
                            POWERPC_EXCP_INVAL | POWERPC_EXCP_INVAL_INVAL);
    } else if (unlikely(ppc_dcr_read(env->dcr_env, T0, &val) != 0)) {
        if (loglevel != 0) {
            fprintf(logfile, "DCR read error %d %03x\n", (int)T0, (int)T0);
        }
        raise_exception_err(env, POWERPC_EXCP_PROGRAM,
                            POWERPC_EXCP_INVAL | POWERPC_EXCP_PRIV_REG);
    } else {
        T0 = val;
    }
}

void do_store_dcr (void)
{
    if (unlikely(env->dcr_env == NULL)) {
        if (loglevel != 0) {
            fprintf(logfile, "No DCR environment\n");
        }
        raise_exception_err(env, POWERPC_EXCP_PROGRAM,
                            POWERPC_EXCP_INVAL | POWERPC_EXCP_INVAL_INVAL);
    } else if (unlikely(ppc_dcr_write(env->dcr_env, T0, T1) != 0)) {
        if (loglevel != 0) {
            fprintf(logfile, "DCR write error %d %03x\n", (int)T0, (int)T0);
        }
        raise_exception_err(env, POWERPC_EXCP_PROGRAM,
                            POWERPC_EXCP_INVAL | POWERPC_EXCP_PRIV_REG);
    }
}

#if !defined(CONFIG_USER_ONLY)
void do_40x_rfci (void)
{
    __do_rfi(env->spr[SPR_40x_SRR2], env->spr[SPR_40x_SRR3],
             ~((target_ulong)0xFFFF0000), 0);
}

void do_rfci (void)
{
    __do_rfi(env->spr[SPR_BOOKE_CSRR0], SPR_BOOKE_CSRR1,
             ~((target_ulong)0x3FFF0000), 0);
}

void do_rfdi (void)
{
    __do_rfi(env->spr[SPR_BOOKE_DSRR0], SPR_BOOKE_DSRR1,
             ~((target_ulong)0x3FFF0000), 0);
}

void do_rfmci (void)
{
    __do_rfi(env->spr[SPR_BOOKE_MCSRR0], SPR_BOOKE_MCSRR1,
             ~((target_ulong)0x3FFF0000), 0);
}

void do_load_403_pb (int num)
{
    T0 = env->pb[num];
}

void do_store_403_pb (int num)
{
    if (likely(env->pb[num] != T0)) {
        env->pb[num] = T0;
        /* Should be optimized */
        tlb_flush(env, 1);
    }
}
#endif

/* 440 specific */
void do_440_dlmzb (void)
{
    target_ulong mask;
    int i;

    i = 1;
    for (mask = 0xFF000000; mask != 0; mask = mask >> 8) {
        if ((T0 & mask) == 0)
            goto done;
        i++;
    }
    for (mask = 0xFF000000; mask != 0; mask = mask >> 8) {
        if ((T1 & mask) == 0)
            break;
        i++;
    }
 done:
    T0 = i;
}

/*****************************************************************************/
/* SPE extension helpers */
/* Use a table to make this quicker */
static uint8_t hbrev[16] = {
    0x0, 0x8, 0x4, 0xC, 0x2, 0xA, 0x6, 0xE,
    0x1, 0x9, 0x5, 0xD, 0x3, 0xB, 0x7, 0xF,
};

static always_inline uint8_t byte_reverse (uint8_t val)
{
    return hbrev[val >> 4] | (hbrev[val & 0xF] << 4);
}

static always_inline uint32_t word_reverse (uint32_t val)
{
    return byte_reverse(val >> 24) | (byte_reverse(val >> 16) << 8) |
        (byte_reverse(val >> 8) << 16) | (byte_reverse(val) << 24);
}

#define MASKBITS 16 // Random value - to be fixed (implementation dependant)
target_ulong helper_brinc (target_ulong arg1, target_ulong arg2)
{
    uint32_t a, b, d, mask;

    mask = UINT32_MAX >> (32 - MASKBITS);
    a = arg1 & mask;
    b = arg2 & mask;
    d = word_reverse(1 + word_reverse(a | ~b));
    return (arg1 & ~mask) | (d & b);
}

uint32_t helper_cntlsw32 (uint32_t val)
{
    if (val & 0x80000000)
        return clz32(~val);
    else
        return clz32(val);
}

uint32_t helper_cntlzw32 (uint32_t val)
{
    return clz32(val);
}

/* Single-precision floating-point conversions */
static always_inline uint32_t efscfsi (uint32_t val)
{
    CPU_FloatU u;

    u.f = int32_to_float32(val, &env->spe_status);

    return u.l;
}

static always_inline uint32_t efscfui (uint32_t val)
{
    CPU_FloatU u;

    u.f = uint32_to_float32(val, &env->spe_status);

    return u.l;
}

static always_inline int32_t efsctsi (uint32_t val)
{
    CPU_FloatU u;

    u.l = val;
    /* NaN are not treated the same way IEEE 754 does */
    if (unlikely(isnan(u.f)))
        return 0;

    return float32_to_int32(u.f, &env->spe_status);
}

static always_inline uint32_t efsctui (uint32_t val)
{
    CPU_FloatU u;

    u.l = val;
    /* NaN are not treated the same way IEEE 754 does */
    if (unlikely(isnan(u.f)))
        return 0;

    return float32_to_uint32(u.f, &env->spe_status);
}

static always_inline uint32_t efsctsiz (uint32_t val)
{
    CPU_FloatU u;

    u.l = val;
    /* NaN are not treated the same way IEEE 754 does */
    if (unlikely(isnan(u.f)))
        return 0;

    return float32_to_int32_round_to_zero(u.f, &env->spe_status);
}

static always_inline uint32_t efsctuiz (uint32_t val)
{
    CPU_FloatU u;

    u.l = val;
    /* NaN are not treated the same way IEEE 754 does */
    if (unlikely(isnan(u.f)))
        return 0;

    return float32_to_uint32_round_to_zero(u.f, &env->spe_status);
}

static always_inline uint32_t efscfsf (uint32_t val)
{
    CPU_FloatU u;
    float32 tmp;

    u.f = int32_to_float32(val, &env->spe_status);
    tmp = int64_to_float32(1ULL << 32, &env->spe_status);
    u.f = float32_div(u.f, tmp, &env->spe_status);

    return u.l;
}

static always_inline uint32_t efscfuf (uint32_t val)
{
    CPU_FloatU u;
    float32 tmp;

    u.f = uint32_to_float32(val, &env->spe_status);
    tmp = uint64_to_float32(1ULL << 32, &env->spe_status);
    u.f = float32_div(u.f, tmp, &env->spe_status);

    return u.l;
}

static always_inline uint32_t efsctsf (uint32_t val)
{
    CPU_FloatU u;
    float32 tmp;

    u.l = val;
    /* NaN are not treated the same way IEEE 754 does */
    if (unlikely(isnan(u.f)))
        return 0;
    tmp = uint64_to_float32(1ULL << 32, &env->spe_status);
    u.f = float32_mul(u.f, tmp, &env->spe_status);

    return float32_to_int32(u.f, &env->spe_status);
}

static always_inline uint32_t efsctuf (uint32_t val)
{
    CPU_FloatU u;
    float32 tmp;

    u.l = val;
    /* NaN are not treated the same way IEEE 754 does */
    if (unlikely(isnan(u.f)))
        return 0;
    tmp = uint64_to_float32(1ULL << 32, &env->spe_status);
    u.f = float32_mul(u.f, tmp, &env->spe_status);

    return float32_to_uint32(u.f, &env->spe_status);
}

#define HELPER_SPE_SINGLE_CONV(name)                                          \
uint32_t helper_e##name (uint32_t val)                                        \
{                                                                             \
    return e##name(val);                                                      \
}
/* efscfsi */
HELPER_SPE_SINGLE_CONV(fscfsi);
/* efscfui */
HELPER_SPE_SINGLE_CONV(fscfui);
/* efscfuf */
HELPER_SPE_SINGLE_CONV(fscfuf);
/* efscfsf */
HELPER_SPE_SINGLE_CONV(fscfsf);
/* efsctsi */
HELPER_SPE_SINGLE_CONV(fsctsi);
/* efsctui */
HELPER_SPE_SINGLE_CONV(fsctui);
/* efsctsiz */
HELPER_SPE_SINGLE_CONV(fsctsiz);
/* efsctuiz */
HELPER_SPE_SINGLE_CONV(fsctuiz);
/* efsctsf */
HELPER_SPE_SINGLE_CONV(fsctsf);
/* efsctuf */
HELPER_SPE_SINGLE_CONV(fsctuf);

#define HELPER_SPE_VECTOR_CONV(name)                                          \
uint64_t helper_ev##name (uint64_t val)                                       \
{                                                                             \
    return ((uint64_t)e##name(val >> 32) << 32) |                             \
            (uint64_t)e##name(val);                                           \
}
/* evfscfsi */
HELPER_SPE_VECTOR_CONV(fscfsi);
/* evfscfui */
HELPER_SPE_VECTOR_CONV(fscfui);
/* evfscfuf */
HELPER_SPE_VECTOR_CONV(fscfuf);
/* evfscfsf */
HELPER_SPE_VECTOR_CONV(fscfsf);
/* evfsctsi */
HELPER_SPE_VECTOR_CONV(fsctsi);
/* evfsctui */
HELPER_SPE_VECTOR_CONV(fsctui);
/* evfsctsiz */
HELPER_SPE_VECTOR_CONV(fsctsiz);
/* evfsctuiz */
HELPER_SPE_VECTOR_CONV(fsctuiz);
/* evfsctsf */
HELPER_SPE_VECTOR_CONV(fsctsf);
/* evfsctuf */
HELPER_SPE_VECTOR_CONV(fsctuf);

/* Single-precision floating-point arithmetic */
static always_inline uint32_t efsadd (uint32_t op1, uint32_t op2)
{
    CPU_FloatU u1, u2;
    u1.l = op1;
    u2.l = op2;
    u1.f = float32_add(u1.f, u2.f, &env->spe_status);
    return u1.l;
}

static always_inline uint32_t efssub (uint32_t op1, uint32_t op2)
{
    CPU_FloatU u1, u2;
    u1.l = op1;
    u2.l = op2;
    u1.f = float32_sub(u1.f, u2.f, &env->spe_status);
    return u1.l;
}

static always_inline uint32_t efsmul (uint32_t op1, uint32_t op2)
{
    CPU_FloatU u1, u2;
    u1.l = op1;
    u2.l = op2;
    u1.f = float32_mul(u1.f, u2.f, &env->spe_status);
    return u1.l;
}

static always_inline uint32_t efsdiv (uint32_t op1, uint32_t op2)
{
    CPU_FloatU u1, u2;
    u1.l = op1;
    u2.l = op2;
    u1.f = float32_div(u1.f, u2.f, &env->spe_status);
    return u1.l;
}

#define HELPER_SPE_SINGLE_ARITH(name)                                         \
uint32_t helper_e##name (uint32_t op1, uint32_t op2)                          \
{                                                                             \
    return e##name(op1, op2);                                                 \
}
/* efsadd */
HELPER_SPE_SINGLE_ARITH(fsadd);
/* efssub */
HELPER_SPE_SINGLE_ARITH(fssub);
/* efsmul */
HELPER_SPE_SINGLE_ARITH(fsmul);
/* efsdiv */
HELPER_SPE_SINGLE_ARITH(fsdiv);

#define HELPER_SPE_VECTOR_ARITH(name)                                         \
uint64_t helper_ev##name (uint64_t op1, uint64_t op2)                         \
{                                                                             \
    return ((uint64_t)e##name(op1 >> 32, op2 >> 32) << 32) |                  \
            (uint64_t)e##name(op1, op2);                                      \
}
/* evfsadd */
HELPER_SPE_VECTOR_ARITH(fsadd);
/* evfssub */
HELPER_SPE_VECTOR_ARITH(fssub);
/* evfsmul */
HELPER_SPE_VECTOR_ARITH(fsmul);
/* evfsdiv */
HELPER_SPE_VECTOR_ARITH(fsdiv);

/* Single-precision floating-point comparisons */
static always_inline uint32_t efststlt (uint32_t op1, uint32_t op2)
{
    CPU_FloatU u1, u2;
    u1.l = op1;
    u2.l = op2;
    return float32_lt(u1.f, u2.f, &env->spe_status) ? 4 : 0;
}

static always_inline uint32_t efststgt (uint32_t op1, uint32_t op2)
{
    CPU_FloatU u1, u2;
    u1.l = op1;
    u2.l = op2;
    return float32_le(u1.f, u2.f, &env->spe_status) ? 0 : 4;
}

static always_inline uint32_t efststeq (uint32_t op1, uint32_t op2)
{
    CPU_FloatU u1, u2;
    u1.l = op1;
    u2.l = op2;
    return float32_eq(u1.f, u2.f, &env->spe_status) ? 4 : 0;
}

static always_inline uint32_t efscmplt (uint32_t op1, uint32_t op2)
{
    /* XXX: TODO: test special values (NaN, infinites, ...) */
    return efststlt(op1, op2);
}

static always_inline uint32_t efscmpgt (uint32_t op1, uint32_t op2)
{
    /* XXX: TODO: test special values (NaN, infinites, ...) */
    return efststgt(op1, op2);
}

static always_inline uint32_t efscmpeq (uint32_t op1, uint32_t op2)
{
    /* XXX: TODO: test special values (NaN, infinites, ...) */
    return efststeq(op1, op2);
}

#define HELPER_SINGLE_SPE_CMP(name)                                           \
uint32_t helper_e##name (uint32_t op1, uint32_t op2)                          \
{                                                                             \
    return e##name(op1, op2) << 2;                                            \
}
/* efststlt */
HELPER_SINGLE_SPE_CMP(fststlt);
/* efststgt */
HELPER_SINGLE_SPE_CMP(fststgt);
/* efststeq */
HELPER_SINGLE_SPE_CMP(fststeq);
/* efscmplt */
HELPER_SINGLE_SPE_CMP(fscmplt);
/* efscmpgt */
HELPER_SINGLE_SPE_CMP(fscmpgt);
/* efscmpeq */
HELPER_SINGLE_SPE_CMP(fscmpeq);

static always_inline uint32_t evcmp_merge (int t0, int t1)
{
    return (t0 << 3) | (t1 << 2) | ((t0 | t1) << 1) | (t0 & t1);
}

#define HELPER_VECTOR_SPE_CMP(name)                                           \
uint32_t helper_ev##name (uint64_t op1, uint64_t op2)                         \
{                                                                             \
    return evcmp_merge(e##name(op1 >> 32, op2 >> 32), e##name(op1, op2));     \
}
/* evfststlt */
HELPER_VECTOR_SPE_CMP(fststlt);
/* evfststgt */
HELPER_VECTOR_SPE_CMP(fststgt);
/* evfststeq */
HELPER_VECTOR_SPE_CMP(fststeq);
/* evfscmplt */
HELPER_VECTOR_SPE_CMP(fscmplt);
/* evfscmpgt */
HELPER_VECTOR_SPE_CMP(fscmpgt);
/* evfscmpeq */
HELPER_VECTOR_SPE_CMP(fscmpeq);

/* Double-precision floating-point conversion */
uint64_t helper_efdcfsi (uint32_t val)
{
    CPU_DoubleU u;

    u.d = int32_to_float64(val, &env->spe_status);

    return u.ll;
}

uint64_t helper_efdcfsid (uint64_t val)
{
    CPU_DoubleU u;

    u.d = int64_to_float64(val, &env->spe_status);

    return u.ll;
}

uint64_t helper_efdcfui (uint32_t val)
{
    CPU_DoubleU u;

    u.d = uint32_to_float64(val, &env->spe_status);

    return u.ll;
}

uint64_t helper_efdcfuid (uint64_t val)
{
    CPU_DoubleU u;

    u.d = uint64_to_float64(val, &env->spe_status);

    return u.ll;
}

uint32_t helper_efdctsi (uint64_t val)
{
    CPU_DoubleU u;

    u.ll = val;
    /* NaN are not treated the same way IEEE 754 does */
    if (unlikely(isnan(u.d)))
        return 0;

    return float64_to_int32(u.d, &env->spe_status);
}

uint32_t helper_efdctui (uint64_t val)
{
    CPU_DoubleU u;

    u.ll = val;
    /* NaN are not treated the same way IEEE 754 does */
    if (unlikely(isnan(u.d)))
        return 0;

    return float64_to_uint32(u.d, &env->spe_status);
}

uint32_t helper_efdctsiz (uint64_t val)
{
    CPU_DoubleU u;

    u.ll = val;
    /* NaN are not treated the same way IEEE 754 does */
    if (unlikely(isnan(u.d)))
        return 0;

    return float64_to_int32_round_to_zero(u.d, &env->spe_status);
}

uint64_t helper_efdctsidz (uint64_t val)
{
    CPU_DoubleU u;

    u.ll = val;
    /* NaN are not treated the same way IEEE 754 does */
    if (unlikely(isnan(u.d)))
        return 0;

    return float64_to_int64_round_to_zero(u.d, &env->spe_status);
}

uint32_t helper_efdctuiz (uint64_t val)
{
    CPU_DoubleU u;

    u.ll = val;
    /* NaN are not treated the same way IEEE 754 does */
    if (unlikely(isnan(u.d)))
        return 0;

    return float64_to_uint32_round_to_zero(u.d, &env->spe_status);
}

uint64_t helper_efdctuidz (uint64_t val)
{
    CPU_DoubleU u;

    u.ll = val;
    /* NaN are not treated the same way IEEE 754 does */
    if (unlikely(isnan(u.d)))
        return 0;

    return float64_to_uint64_round_to_zero(u.d, &env->spe_status);
}

uint64_t helper_efdcfsf (uint32_t val)
{
    CPU_DoubleU u;
    float64 tmp;

    u.d = int32_to_float64(val, &env->spe_status);
    tmp = int64_to_float64(1ULL << 32, &env->spe_status);
    u.d = float64_div(u.d, tmp, &env->spe_status);

    return u.ll;
}

uint64_t helper_efdcfuf (uint32_t val)
{
    CPU_DoubleU u;
    float64 tmp;

    u.d = uint32_to_float64(val, &env->spe_status);
    tmp = int64_to_float64(1ULL << 32, &env->spe_status);
    u.d = float64_div(u.d, tmp, &env->spe_status);

    return u.ll;
}

uint32_t helper_efdctsf (uint64_t val)
{
    CPU_DoubleU u;
    float64 tmp;

    u.ll = val;
    /* NaN are not treated the same way IEEE 754 does */
    if (unlikely(isnan(u.d)))
        return 0;
    tmp = uint64_to_float64(1ULL << 32, &env->spe_status);
    u.d = float64_mul(u.d, tmp, &env->spe_status);

    return float64_to_int32(u.d, &env->spe_status);
}

uint32_t helper_efdctuf (uint64_t val)
{
    CPU_DoubleU u;
    float64 tmp;

    u.ll = val;
    /* NaN are not treated the same way IEEE 754 does */
    if (unlikely(isnan(u.d)))
        return 0;
    tmp = uint64_to_float64(1ULL << 32, &env->spe_status);
    u.d = float64_mul(u.d, tmp, &env->spe_status);

    return float64_to_uint32(u.d, &env->spe_status);
}

uint32_t helper_efscfd (uint64_t val)
{
    CPU_DoubleU u1;
    CPU_FloatU u2;

    u1.ll = val;
    u2.f = float64_to_float32(u1.d, &env->spe_status);

    return u2.l;
}

uint64_t helper_efdcfs (uint32_t val)
{
    CPU_DoubleU u2;
    CPU_FloatU u1;

    u1.l = val;
    u2.d = float32_to_float64(u1.f, &env->spe_status);

    return u2.ll;
}

/* Double precision fixed-point arithmetic */
uint64_t helper_efdadd (uint64_t op1, uint64_t op2)
{
    CPU_DoubleU u1, u2;
    u1.ll = op1;
    u2.ll = op2;
    u1.d = float64_add(u1.d, u2.d, &env->spe_status);
    return u1.ll;
}

uint64_t helper_efdsub (uint64_t op1, uint64_t op2)
{
    CPU_DoubleU u1, u2;
    u1.ll = op1;
    u2.ll = op2;
    u1.d = float64_sub(u1.d, u2.d, &env->spe_status);
    return u1.ll;
}

uint64_t helper_efdmul (uint64_t op1, uint64_t op2)
{
    CPU_DoubleU u1, u2;
    u1.ll = op1;
    u2.ll = op2;
    u1.d = float64_mul(u1.d, u2.d, &env->spe_status);
    return u1.ll;
}

uint64_t helper_efddiv (uint64_t op1, uint64_t op2)
{
    CPU_DoubleU u1, u2;
    u1.ll = op1;
    u2.ll = op2;
    u1.d = float64_div(u1.d, u2.d, &env->spe_status);
    return u1.ll;
}

/* Double precision floating point helpers */
uint32_t helper_efdtstlt (uint64_t op1, uint64_t op2)
{
    CPU_DoubleU u1, u2;
    u1.ll = op1;
    u2.ll = op2;
    return float64_lt(u1.d, u2.d, &env->spe_status) ? 4 : 0;
}

uint32_t helper_efdtstgt (uint64_t op1, uint64_t op2)
{
    CPU_DoubleU u1, u2;
    u1.ll = op1;
    u2.ll = op2;
    return float64_le(u1.d, u2.d, &env->spe_status) ? 0 : 4;
}

uint32_t helper_efdtsteq (uint64_t op1, uint64_t op2)
{
    CPU_DoubleU u1, u2;
    u1.ll = op1;
    u2.ll = op2;
    return float64_eq(u1.d, u2.d, &env->spe_status) ? 4 : 0;
}

uint32_t helper_efdcmplt (uint64_t op1, uint64_t op2)
{
    /* XXX: TODO: test special values (NaN, infinites, ...) */
    return helper_efdtstlt(op1, op2);
}

uint32_t helper_efdcmpgt (uint64_t op1, uint64_t op2)
{
    /* XXX: TODO: test special values (NaN, infinites, ...) */
    return helper_efdtstgt(op1, op2);
}

uint32_t helper_efdcmpeq (uint64_t op1, uint64_t op2)
{
    /* XXX: TODO: test special values (NaN, infinites, ...) */
    return helper_efdtsteq(op1, op2);
}

/*****************************************************************************/
/* Softmmu support */
#if !defined (CONFIG_USER_ONLY)

#define MMUSUFFIX _mmu

#define SHIFT 0
#include "softmmu_template.h"

#define SHIFT 1
#include "softmmu_template.h"

#define SHIFT 2
#include "softmmu_template.h"

#define SHIFT 3
#include "softmmu_template.h"

/* try to fill the TLB and return an exception if error. If retaddr is
   NULL, it means that the function was called in C code (i.e. not
   from generated code or from helper.c) */
/* XXX: fix it to restore all registers */
void tlb_fill (target_ulong addr, int is_write, int mmu_idx, void *retaddr)
{
    TranslationBlock *tb;
    CPUState *saved_env;
    unsigned long pc;
    int ret;

    /* XXX: hack to restore env in all cases, even if not called from
       generated code */
    saved_env = env;
    env = cpu_single_env;
    ret = cpu_ppc_handle_mmu_fault(env, addr, is_write, mmu_idx, 1);
    if (unlikely(ret != 0)) {
        if (likely(retaddr)) {
            /* now we have a real cpu fault */
            pc = (unsigned long)retaddr;
            tb = tb_find_pc(pc);
            if (likely(tb)) {
                /* the PC is inside the translated code. It means that we have
                   a virtual CPU fault */
                cpu_restore_state(tb, env, pc, NULL);
            }
        }
        raise_exception_err(env, env->exception_index, env->error_code);
    }
    env = saved_env;
}

/* Software driven TLBs management */
/* PowerPC 602/603 software TLB load instructions helpers */
void do_load_6xx_tlb (int is_code)
{
    target_ulong RPN, CMP, EPN;
    int way;

    RPN = env->spr[SPR_RPA];
    if (is_code) {
        CMP = env->spr[SPR_ICMP];
        EPN = env->spr[SPR_IMISS];
    } else {
        CMP = env->spr[SPR_DCMP];
        EPN = env->spr[SPR_DMISS];
    }
    way = (env->spr[SPR_SRR1] >> 17) & 1;
#if defined (DEBUG_SOFTWARE_TLB)
    if (loglevel != 0) {
        fprintf(logfile, "%s: EPN " TDX " " ADDRX " PTE0 " ADDRX
                " PTE1 " ADDRX " way %d\n",
                __func__, T0, EPN, CMP, RPN, way);
    }
#endif
    /* Store this TLB */
    ppc6xx_tlb_store(env, (uint32_t)(T0 & TARGET_PAGE_MASK),
                     way, is_code, CMP, RPN);
}

void do_load_74xx_tlb (int is_code)
{
    target_ulong RPN, CMP, EPN;
    int way;

    RPN = env->spr[SPR_PTELO];
    CMP = env->spr[SPR_PTEHI];
    EPN = env->spr[SPR_TLBMISS] & ~0x3;
    way = env->spr[SPR_TLBMISS] & 0x3;
#if defined (DEBUG_SOFTWARE_TLB)
    if (loglevel != 0) {
        fprintf(logfile, "%s: EPN " TDX " " ADDRX " PTE0 " ADDRX
                " PTE1 " ADDRX " way %d\n",
                __func__, T0, EPN, CMP, RPN, way);
    }
#endif
    /* Store this TLB */
    ppc6xx_tlb_store(env, (uint32_t)(T0 & TARGET_PAGE_MASK),
                     way, is_code, CMP, RPN);
}

static always_inline target_ulong booke_tlb_to_page_size (int size)
{
    return 1024 << (2 * size);
}

static always_inline int booke_page_size_to_tlb (target_ulong page_size)
{
    int size;

    switch (page_size) {
    case 0x00000400UL:
        size = 0x0;
        break;
    case 0x00001000UL:
        size = 0x1;
        break;
    case 0x00004000UL:
        size = 0x2;
        break;
    case 0x00010000UL:
        size = 0x3;
        break;
    case 0x00040000UL:
        size = 0x4;
        break;
    case 0x00100000UL:
        size = 0x5;
        break;
    case 0x00400000UL:
        size = 0x6;
        break;
    case 0x01000000UL:
        size = 0x7;
        break;
    case 0x04000000UL:
        size = 0x8;
        break;
    case 0x10000000UL:
        size = 0x9;
        break;
    case 0x40000000UL:
        size = 0xA;
        break;
#if defined (TARGET_PPC64)
    case 0x000100000000ULL:
        size = 0xB;
        break;
    case 0x000400000000ULL:
        size = 0xC;
        break;
    case 0x001000000000ULL:
        size = 0xD;
        break;
    case 0x004000000000ULL:
        size = 0xE;
        break;
    case 0x010000000000ULL:
        size = 0xF;
        break;
#endif
    default:
        size = -1;
        break;
    }

    return size;
}

/* Helpers for 4xx TLB management */
void do_4xx_tlbre_lo (void)
{
    ppcemb_tlb_t *tlb;
    int size;

    T0 &= 0x3F;
    tlb = &env->tlb[T0].tlbe;
    T0 = tlb->EPN;
    if (tlb->prot & PAGE_VALID)
        T0 |= 0x400;
    size = booke_page_size_to_tlb(tlb->size);
    if (size < 0 || size > 0x7)
        size = 1;
    T0 |= size << 7;
    env->spr[SPR_40x_PID] = tlb->PID;
}

void do_4xx_tlbre_hi (void)
{
    ppcemb_tlb_t *tlb;

    T0 &= 0x3F;
    tlb = &env->tlb[T0].tlbe;
    T0 = tlb->RPN;
    if (tlb->prot & PAGE_EXEC)
        T0 |= 0x200;
    if (tlb->prot & PAGE_WRITE)
        T0 |= 0x100;
}

void do_4xx_tlbwe_hi (void)
{
    ppcemb_tlb_t *tlb;
    target_ulong page, end;

#if defined (DEBUG_SOFTWARE_TLB)
    if (loglevel != 0) {
        fprintf(logfile, "%s T0 " TDX " T1 " TDX "\n", __func__, T0, T1);
    }
#endif
    T0 &= 0x3F;
    tlb = &env->tlb[T0].tlbe;
    /* Invalidate previous TLB (if it's valid) */
    if (tlb->prot & PAGE_VALID) {
        end = tlb->EPN + tlb->size;
#if defined (DEBUG_SOFTWARE_TLB)
        if (loglevel != 0) {
            fprintf(logfile, "%s: invalidate old TLB %d start " ADDRX
                    " end " ADDRX "\n", __func__, (int)T0, tlb->EPN, end);
        }
#endif
        for (page = tlb->EPN; page < end; page += TARGET_PAGE_SIZE)
            tlb_flush_page(env, page);
    }
    tlb->size = booke_tlb_to_page_size((T1 >> 7) & 0x7);
    /* We cannot handle TLB size < TARGET_PAGE_SIZE.
     * If this ever occurs, one should use the ppcemb target instead
     * of the ppc or ppc64 one
     */
    if ((T1 & 0x40) && tlb->size < TARGET_PAGE_SIZE) {
        cpu_abort(env, "TLB size " TARGET_FMT_lu " < %u "
                  "are not supported (%d)\n",
                  tlb->size, TARGET_PAGE_SIZE, (int)((T1 >> 7) & 0x7));
    }
    tlb->EPN = T1 & ~(tlb->size - 1);
    if (T1 & 0x40)
        tlb->prot |= PAGE_VALID;
    else
        tlb->prot &= ~PAGE_VALID;
    if (T1 & 0x20) {
        /* XXX: TO BE FIXED */
        cpu_abort(env, "Little-endian TLB entries are not supported by now\n");
    }
    tlb->PID = env->spr[SPR_40x_PID]; /* PID */
    tlb->attr = T1 & 0xFF;
#if defined (DEBUG_SOFTWARE_TLB)
    if (loglevel != 0) {
        fprintf(logfile, "%s: set up TLB %d RPN " PADDRX " EPN " ADDRX
                " size " ADDRX " prot %c%c%c%c PID %d\n", __func__,
                (int)T0, tlb->RPN, tlb->EPN, tlb->size,
                tlb->prot & PAGE_READ ? 'r' : '-',
                tlb->prot & PAGE_WRITE ? 'w' : '-',
                tlb->prot & PAGE_EXEC ? 'x' : '-',
                tlb->prot & PAGE_VALID ? 'v' : '-', (int)tlb->PID);
    }
#endif
    /* Invalidate new TLB (if valid) */
    if (tlb->prot & PAGE_VALID) {
        end = tlb->EPN + tlb->size;
#if defined (DEBUG_SOFTWARE_TLB)
        if (loglevel != 0) {
            fprintf(logfile, "%s: invalidate TLB %d start " ADDRX
                    " end " ADDRX "\n", __func__, (int)T0, tlb->EPN, end);
        }
#endif
        for (page = tlb->EPN; page < end; page += TARGET_PAGE_SIZE)
            tlb_flush_page(env, page);
    }
}

void do_4xx_tlbwe_lo (void)
{
    ppcemb_tlb_t *tlb;

#if defined (DEBUG_SOFTWARE_TLB)
    if (loglevel != 0) {
        fprintf(logfile, "%s T0 " TDX " T1 " TDX "\n", __func__, T0, T1);
    }
#endif
    T0 &= 0x3F;
    tlb = &env->tlb[T0].tlbe;
    tlb->RPN = T1 & 0xFFFFFC00;
    tlb->prot = PAGE_READ;
    if (T1 & 0x200)
        tlb->prot |= PAGE_EXEC;
    if (T1 & 0x100)
        tlb->prot |= PAGE_WRITE;
#if defined (DEBUG_SOFTWARE_TLB)
    if (loglevel != 0) {
        fprintf(logfile, "%s: set up TLB %d RPN " PADDRX " EPN " ADDRX
                " size " ADDRX " prot %c%c%c%c PID %d\n", __func__,
                (int)T0, tlb->RPN, tlb->EPN, tlb->size,
                tlb->prot & PAGE_READ ? 'r' : '-',
                tlb->prot & PAGE_WRITE ? 'w' : '-',
                tlb->prot & PAGE_EXEC ? 'x' : '-',
                tlb->prot & PAGE_VALID ? 'v' : '-', (int)tlb->PID);
    }
#endif
}

/* PowerPC 440 TLB management */
void do_440_tlbwe (int word)
{
    ppcemb_tlb_t *tlb;
    target_ulong EPN, RPN, size;
    int do_flush_tlbs;

#if defined (DEBUG_SOFTWARE_TLB)
    if (loglevel != 0) {
        fprintf(logfile, "%s word %d T0 " TDX " T1 " TDX "\n",
                __func__, word, T0, T1);
    }
#endif
    do_flush_tlbs = 0;
    T0 &= 0x3F;
    tlb = &env->tlb[T0].tlbe;
    switch (word) {
    default:
        /* Just here to please gcc */
    case 0:
        EPN = T1 & 0xFFFFFC00;
        if ((tlb->prot & PAGE_VALID) && EPN != tlb->EPN)
            do_flush_tlbs = 1;
        tlb->EPN = EPN;
        size = booke_tlb_to_page_size((T1 >> 4) & 0xF);
        if ((tlb->prot & PAGE_VALID) && tlb->size < size)
            do_flush_tlbs = 1;
        tlb->size = size;
        tlb->attr &= ~0x1;
        tlb->attr |= (T1 >> 8) & 1;
        if (T1 & 0x200) {
            tlb->prot |= PAGE_VALID;
        } else {
            if (tlb->prot & PAGE_VALID) {
                tlb->prot &= ~PAGE_VALID;
                do_flush_tlbs = 1;
            }
        }
        tlb->PID = env->spr[SPR_440_MMUCR] & 0x000000FF;
        if (do_flush_tlbs)
            tlb_flush(env, 1);
        break;
    case 1:
        RPN = T1 & 0xFFFFFC0F;
        if ((tlb->prot & PAGE_VALID) && tlb->RPN != RPN)
            tlb_flush(env, 1);
        tlb->RPN = RPN;
        break;
    case 2:
        tlb->attr = (tlb->attr & 0x1) | (T1 & 0x0000FF00);
        tlb->prot = tlb->prot & PAGE_VALID;
        if (T1 & 0x1)
            tlb->prot |= PAGE_READ << 4;
        if (T1 & 0x2)
            tlb->prot |= PAGE_WRITE << 4;
        if (T1 & 0x4)
            tlb->prot |= PAGE_EXEC << 4;
        if (T1 & 0x8)
            tlb->prot |= PAGE_READ;
        if (T1 & 0x10)
            tlb->prot |= PAGE_WRITE;
        if (T1 & 0x20)
            tlb->prot |= PAGE_EXEC;
        break;
    }
}

void do_440_tlbre (int word)
{
    ppcemb_tlb_t *tlb;
    int size;

    T0 &= 0x3F;
    tlb = &env->tlb[T0].tlbe;
    switch (word) {
    default:
        /* Just here to please gcc */
    case 0:
        T0 = tlb->EPN;
        size = booke_page_size_to_tlb(tlb->size);
        if (size < 0 || size > 0xF)
            size = 1;
        T0 |= size << 4;
        if (tlb->attr & 0x1)
            T0 |= 0x100;
        if (tlb->prot & PAGE_VALID)
            T0 |= 0x200;
        env->spr[SPR_440_MMUCR] &= ~0x000000FF;
        env->spr[SPR_440_MMUCR] |= tlb->PID;
        break;
    case 1:
        T0 = tlb->RPN;
        break;
    case 2:
        T0 = tlb->attr & ~0x1;
        if (tlb->prot & (PAGE_READ << 4))
            T0 |= 0x1;
        if (tlb->prot & (PAGE_WRITE << 4))
            T0 |= 0x2;
        if (tlb->prot & (PAGE_EXEC << 4))
            T0 |= 0x4;
        if (tlb->prot & PAGE_READ)
            T0 |= 0x8;
        if (tlb->prot & PAGE_WRITE)
            T0 |= 0x10;
        if (tlb->prot & PAGE_EXEC)
            T0 |= 0x20;
        break;
    }
}
#endif /* !CONFIG_USER_ONLY */
