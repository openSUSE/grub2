/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2002,2003,2004,2005,2006,2007,2008,2009,2013  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/kernel.h>
#include <grub/mm.h>
#include <grub/memory.h>
#include <grub/machine/console.h>
#include <grub/offsets.h>
#include <grub/types.h>
#include <grub/err.h>
#include <grub/dl.h>
#include <grub/misc.h>
#include <grub/loader.h>
#include <grub/env.h>
#include <grub/cache.h>
#include <grub/time.h>
#include <grub/symbol.h>
#include <grub/video.h>

extern grub_uint8_t _start[];
extern grub_uint8_t _end[];
extern grub_uint8_t _edata[];

grub_uint64_t
grub_armv7_get_timer_value(void);

grub_uint32_t
grub_armv7_get_timer_frequency(void);

grub_uint32_t
grub_arm_pfr1(void);

void  __attribute__ ((noreturn))
grub_exit (void)
{
  /* We can't use grub_fatal() in this function.  This would create an infinite
     loop, since grub_fatal() calls grub_abort() which in turn calls grub_exit().  */
  while (1)
    grub_cpu_idle ();
}

static grub_uint64_t modend;
static int have_memory = 0;

/* Helper for grub_machine_init.  */
static int
heap_init (grub_uint64_t addr, grub_uint64_t size, grub_memory_type_t type,
	   void *data __attribute__ ((unused)))
{
  grub_uint64_t begin = addr, end = addr + size;

#if GRUB_CPU_SIZEOF_VOID_P == 4
  /* Restrict ourselves to 32-bit memory space.  */
  if (begin > GRUB_ULONG_MAX)
    return 0;
  if (end > GRUB_ULONG_MAX)
    end = GRUB_ULONG_MAX;
#endif

  if (type != GRUB_MEMORY_AVAILABLE)
    return 0;

  if (modend && begin < modend)
    begin = modend;
  
  if (end <= begin)
    return 0;

  grub_mm_init_region ((void *) (grub_addr_t) begin, (grub_size_t) (end - begin));

  have_memory = 1;

  return 0;
}

static grub_uint32_t timer_frequency_in_khz;

static grub_uint64_t
get_time_ms (void)
{
  return grub_divmod64 (grub_armv7_get_timer_value(), timer_frequency_in_khz, 0);
}

static int
try_generic_timer (void)
{
  if (((grub_arm_pfr1 () >> 16) & 0xf) != 1)
    return 0;
  timer_frequency_in_khz = grub_armv7_get_timer_frequency() / 1000;
  if (timer_frequency_in_khz == 0)
    return 0;
  grub_install_get_time_ms (get_time_ms);
  return 1;
}

void
grub_machine_init (void)
{
  modend = grub_modules_get_end ();

  grub_video_coreboot_fb_early_init ();

  grub_machine_mmap_iterate (heap_init, NULL);
  if (!have_memory)
    grub_fatal ("No memory found");

  grub_video_coreboot_fb_late_init ();

  grub_font_init ();
  grub_gfxterm_init ();

  if (!try_generic_timer ())
    grub_fatal ("No timer found");
}

void
grub_machine_get_bootlocation (char **device __attribute__ ((unused)),
			       char **path __attribute__ ((unused)))
{
}

void
grub_machine_fini (int flags __attribute__ ((unused)))
{
}
