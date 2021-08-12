/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2008  Free Software Foundation, Inc.
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

#include <grub/types.h>
#include <grub/dl.h>
/* For NULL.  */
#include <grub/mm.h>
#include <grub/btrfs.h>
#include <grub/lib/zstd.h>

GRUB_MOD_LICENSE ("GPLv3+");

GRUB_MOD_INIT (btrfs_zstd)
{
  grub_btrfs_zstd_decompress_func = grub_zstd_decompress;
}

GRUB_MOD_FINI (btrfs_zstd)
{
  grub_btrfs_zstd_decompress_func = NULL;
}
