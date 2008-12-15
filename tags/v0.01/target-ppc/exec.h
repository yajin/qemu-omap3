/*
 *  PowerPC emulation definitions for qemu.
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
#if !defined (__PPC_H__)
#define __PPC_H__

#include "config.h"

#include "dyngen-exec.h"

#include "cpu.h"
#include "exec-all.h"

/* For normal operations, precise emulation should not be needed */
//#define USE_PRECISE_EMULATION 1
#define USE_PRECISE_EMULATION 0

register struct CPUPPCState *env asm(AREG0);
#if TARGET_LONG_BITS > HOST_LONG_BITS
/* no registers can be used */
#define T0 (env->t0)
#define T1 (env->t1)
#define T2 (env->t2)
#define TDX "%016" PRIx64
#else
register target_ulong T0 asm(AREG1);
register target_ulong T1 asm(AREG2);
register target_ulong T2 asm(AREG3);
#define TDX "%016lx"
#endif

#if defined (DEBUG_OP)
# define RETURN() __asm__ __volatile__("nop" : : : "memory");
#else
# define RETURN() __asm__ __volatile__("" : : : "memory");
#endif

static always_inline target_ulong rotl8 (target_ulong i, int n)
{
    return (((uint8_t)i << n) | ((uint8_t)i >> (8 - n)));
}

static always_inline target_ulong rotl16 (target_ulong i, int n)
{
    return (((uint16_t)i << n) | ((uint16_t)i >> (16 - n)));
}

static always_inline target_ulong rotl32 (target_ulong i, int n)
{
    return (((uint32_t)i << n) | ((uint32_t)i >> (32 - n)));
}

#if defined(TARGET_PPC64)
static always_inline target_ulong rotl64 (target_ulong i, int n)
{
    return (((uint64_t)i << n) | ((uint64_t)i >> (64 - n)));
}
#endif

#if !defined(CONFIG_USER_ONLY)
#include "softmmu_exec.h"
#endif /* !defined(CONFIG_USER_ONLY) */

void raise_exception_err (CPUState *env, int exception, int error_code);
void raise_exception (CPUState *env, int exception);

int get_physical_address (CPUState *env, mmu_ctx_t *ctx, target_ulong vaddr,
                          int rw, int access_type);

void ppc6xx_tlb_store (CPUState *env, target_ulong EPN, int way, int is_code,
                       target_ulong pte0, target_ulong pte1);

static always_inline void env_to_regs (void)
{
}

static always_inline void regs_to_env (void)
{
}

int cpu_ppc_handle_mmu_fault (CPUState *env, target_ulong address, int rw,
                              int mmu_idx, int is_softmmu);

static always_inline int cpu_halted (CPUState *env)
{
    if (!env->halted)
        return 0;
    if (msr_ee && (env->interrupt_request & CPU_INTERRUPT_HARD)) {
        env->halted = 0;
        return 0;
    }
    return EXCP_HALTED;
}

#endif /* !defined (__PPC_H__) */
