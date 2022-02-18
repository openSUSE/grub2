#include <grub/dl.h>
#include <grub/mm.h>
#include <grub/file.h>
#include <grub/disk.h>
#include <grub/misc.h>
#include <grub/err.h>
#include <grub/env.h>
#include <grub/partition.h>
#include <grub/lib/envblk.h>
#include <grub/extcmd.h>
#include <grub/i18n.h>
#include <grub/gpt_partition.h>
#include <regex.h>

GRUB_MOD_LICENSE ("GPLv3+");

static char *
match_substr (regmatch_t *match, const char *str)
{
  if (match->rm_so != -1)
    {
      char *substr;
      regoff_t sz = match->rm_eo - match->rm_so;

      if (!sz)
	return NULL;
      substr = grub_malloc (1 + sz);
      if (!substr)
	{
	  grub_print_error ();
	  return NULL;
	}
      grub_memcpy (substr, str + match->rm_so, sz);
      substr[sz] = '\0';
      return substr;
    }

  return NULL;
}

static int
is_prep_partition (grub_device_t dev)
{
  if (!dev->disk)
    return 0;
  if (!dev->disk->partition)
    return 0;
  if (grub_strcmp (dev->disk->partition->partmap->name, "msdos") == 0)
    return (dev->disk->partition->msdostype == 0x41);

  if (grub_strcmp (dev->disk->partition->partmap->name, "gpt") == 0)
    {
      struct grub_gpt_partentry gptdata;
      grub_partition_t p = dev->disk->partition;
      int ret = 0;
      dev->disk->partition = dev->disk->partition->parent;

      if (grub_disk_read (dev->disk, p->offset, p->index,
			  sizeof (gptdata), &gptdata) == 0)
	{
	  const grub_gpt_part_guid_t template = {
	    grub_cpu_to_le32_compile_time (0x9e1a2d38),
	    grub_cpu_to_le16_compile_time (0xc612),
	    grub_cpu_to_le16_compile_time (0x4316),
	    { 0xaa, 0x26, 0x8b, 0x49, 0x52, 0x1e, 0x5a, 0x8b }
	  };

	  ret = grub_memcmp (&template, &gptdata.type,
			     sizeof (template)) == 0;
	}
      dev->disk->partition = p;
      return ret;
    }

  return 0;
}

static int
part_hook (grub_disk_t disk, const grub_partition_t partition, void *data)
{
  char **ret = data;
  char *partition_name, *devname;
  grub_device_t dev;

  partition_name = grub_partition_get_name (partition);
  if (! partition_name)
    return 2;

  devname = grub_xasprintf ("%s,%s", disk->name, partition_name);
  grub_free (partition_name);
  if (!devname)
    return 2;

  dev = grub_device_open (devname);
  if (!dev)
    {
      grub_free (devname);
      return 2;
    }
  if (is_prep_partition (dev))
    {
      *ret = devname;
      return 1;
    }
  grub_free (devname);
  grub_device_close (dev);
  return 0;
}

static int
set_var (const char *name, const char *value,
	 void *hook_data __attribute__ ((unused)))
{
  grub_env_set (name, value);
  grub_env_export (name);
  return 0;
}

static grub_err_t
prep_read_envblk (const char *devname)
{
  char *buf = NULL;
  grub_device_t dev = NULL;
  grub_envblk_t envblk = NULL;

  dev = grub_device_open (devname);
  if (!dev)
    return grub_errno;

  if (!dev->disk || !dev->disk->partition)
    {
      grub_error (GRUB_ERR_BAD_DEVICE, "disk device required");
      goto fail;
    }

  buf = grub_malloc (GRUB_ENVBLK_PREP_SIZE);
  if (!buf)
    goto fail;

  if (grub_disk_read (dev->disk, dev->disk->partition->len - (GRUB_ENVBLK_PREP_SIZE >> GRUB_DISK_SECTOR_BITS), 0, GRUB_ENVBLK_PREP_SIZE, buf))
    goto fail;

  envblk = grub_envblk_open (buf, GRUB_ENVBLK_PREP_SIZE);
  if (!envblk)
    {
      grub_error (GRUB_ERR_BAD_FILE_TYPE, "invalid environment block");
      goto fail;
    }
  grub_envblk_iterate (envblk, NULL, set_var);

 fail:
  if (envblk)
    grub_envblk_close (envblk);
  else
    grub_free (buf);
  if (dev)
    grub_device_close (dev);
  return grub_errno;
}

static grub_err_t
prep_partname (const char *devname, char **prep)
{
  grub_device_t dev = NULL;
  grub_err_t err;
  int ret;

  dev = grub_device_open (devname);
  if (!dev)
    return grub_errno;

  ret = grub_partition_iterate (dev->disk, part_hook, prep);
  if (ret == 1 && *prep)
    {
      err = GRUB_ERR_NONE;
      goto out;
    }
  else if (ret == 0 && grub_errno == GRUB_ERR_NONE)
    err = grub_error (GRUB_ERR_FILE_NOT_FOUND, "no prep partition");
  else
    err = grub_errno;

 out:
  grub_device_close (dev);
  return err;
}

static grub_err_t
boot_disk_prep_partname (char **name)
{
  regex_t regex;
  int ret;
  grub_size_t s;
  char *comperr;
  const char *cmdpath;
  regmatch_t *matches = NULL;
  grub_err_t err = GRUB_ERR_NONE;

  *name = NULL;

  cmdpath = grub_env_get ("cmdpath");
  if (!cmdpath)
    return GRUB_ERR_NONE;

  ret = regcomp (&regex, "\\(([^,]+)(,?.*)?\\)(.*)", REG_EXTENDED);
  if (ret)
    goto fail;

  matches = grub_calloc (regex.re_nsub + 1, sizeof (*matches));
  if (! matches)
    goto fail;

  ret = regexec (&regex, cmdpath, regex.re_nsub + 1, matches, 0);
  if (!ret)
    {
      char *devname = devname = match_substr (matches + 1, cmdpath);
      if (!devname)
	{
	  err = grub_error (GRUB_ERR_FILE_NOT_FOUND, "%s contains no disk name", cmdpath);
	  goto out;
	}

      err = prep_partname (devname, name);
 out:
      grub_free (devname);
      regfree (&regex);
      grub_free (matches);
      return err;
    }

 fail:
  grub_free (matches);
  s = regerror (ret, &regex, 0, 0);
  comperr = grub_malloc (s);
  if (!comperr)
    {
      regfree (&regex);
      return grub_errno;
    }
  regerror (ret, &regex, comperr, s);
  err = grub_error (GRUB_ERR_TEST_FAILURE, "%s", comperr);
  regfree (&regex);
  grub_free (comperr);
  return err;
}

static grub_err_t
grub_cmd_prep_loadenv (grub_command_t cmd __attribute__ ((unused)),
		       int argc,
		       char **argv)
{
  char *devname, *prep = NULL;
  grub_err_t err;

  if (argc < 1)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "device name required");

  devname = grub_file_get_device_name(argv[0]);
  if (!devname)
    return grub_errno;

  err = prep_partname (devname, &prep);
  if (prep == NULL || err != GRUB_ERR_NONE)
    goto out;

  err = prep_read_envblk (prep);

 out:
  grub_free (devname);
  grub_free (prep);
  return err;
}

static void
early_prep_loadenv (void)
{
  grub_err_t err;
  char *prep;

  err = boot_disk_prep_partname (&prep);
  if (err == GRUB_ERR_NONE && prep)
    err = prep_read_envblk (prep);
  if (err == GRUB_ERR_BAD_FILE_TYPE || err == GRUB_ERR_FILE_NOT_FOUND)
    grub_error_pop ();
  if (err != GRUB_ERR_NONE)
    grub_print_error ();
  grub_free (prep);
}

static grub_command_t cmd_prep_load;

GRUB_MOD_INIT(prep_loadenv)
{
  early_env_hook = early_prep_loadenv;
  cmd_prep_load =
    grub_register_command("prep_load_env", grub_cmd_prep_loadenv,
			  "DEVICE",
			  N_("Load variables from environment block file."));
}

GRUB_MOD_FINI(prep_loadenv)
{
  grub_unregister_command (cmd_prep_load);
}
