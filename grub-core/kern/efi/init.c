/* init.c - generic EFI initialization and finalization */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2006,2007  Free Software Foundation, Inc.
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

#include <grub/efi/efi.h>
#include <grub/efi/console.h>
#include <grub/efi/disk.h>
#include <grub/term.h>
#include <grub/misc.h>
#include <grub/env.h>
#include <grub/mm.h>
#include <grub/kernel.h>
#include <grub/file.h>

grub_addr_t grub_modbase;

void
grub_efi_init (void)
{
  grub_modbase = grub_efi_modules_addr ();
  /* First of all, initialize the console so that GRUB can display
     messages.  */
  grub_console_init ();

  /* Initialize the memory management system.  */
  grub_efi_mm_init ();

  efi_call_4 (grub_efi_system_table->boot_services->set_watchdog_timer,
	      0, 0, 0, NULL);

  grub_efidisk_init ();
}

void (*grub_efi_net_config) (grub_efi_handle_t hnd, 
			     char **device,
			     char **path);
static char *
workaround_efi_firmware_path (const char *device, const char *path)
{
  char *config = NULL;;
  char *config_upper = NULL;
  char *path_upper = NULL;
  char *ret_path = NULL;
  grub_file_t config_fd = NULL;
  char *s;

  if (!device || !path)
    return NULL;

  /* only workaround if booting off from cd device */
  if (grub_strncmp (device, "cd", 2) != 0)
    goto quit;

  config = grub_xasprintf ("(%s)%s/grub.cfg", device, path);
  config_fd = grub_file_open (config, GRUB_FILE_TYPE_CONFIG);

  /* everything's fine, so quit the workaround */
  if (config_fd)
    goto quit;

  /* reset grub error state because noone else does... */
  grub_errno = GRUB_ERR_NONE;

  /* try again, this time upper case path */
  path_upper = grub_strdup (path);
  if (! path_upper)
    goto quit;

  s = path_upper;
  for (; *s; s++) *s = grub_toupper(*s);

  config_upper = grub_xasprintf ("(%s)%s/grub.cfg", device, path_upper);
  if (! config_upper)
    goto quit;

  config_fd = grub_file_open (config_upper, GRUB_FILE_TYPE_CONFIG);

  /* if config can be found by the upper case path, return it */
  if (config_fd)
    ret_path = grub_strdup (path_upper);

quit:

  if (config_fd)
    grub_file_close (config_fd);

  if (grub_errno)
    grub_errno = GRUB_ERR_NONE;

  if (config)
    grub_free (config);

  if (config_upper)
    grub_free (config_upper);

  return ret_path;
}

void
grub_machine_get_bootlocation (char **device, char **path)
{
  grub_efi_loaded_image_t *image = NULL;
  char *p;

  image = grub_efi_get_loaded_image (grub_efi_image_handle);
  if (!image)
    return;
  *device = grub_efidisk_get_device_name (image->device_handle);
  if (!*device && grub_efi_net_config)
    {
      grub_efi_net_config (image->device_handle, device, path);
      return;
    }

  *path = grub_efi_get_filename (image->file_path);
  if (*path)
    {
      /* Get the directory.  */
      p = grub_strrchr (*path, '/');
      if (p)
        *p = '\0';

      if ((p = workaround_efi_firmware_path (*device, *path)))
	{
	  grub_free (*path);
	  *path = p;
	}
    }
}

void
grub_efi_fini (void)
{
  grub_efidisk_fini ();
  grub_console_fini ();
  grub_efi_memory_fini ();
}
