#include <grub/err.h>
#include <grub/i18n.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/tpm.h>
#include <grub/term.h>

grub_tpm_t grub_tpm = NULL;

grub_err_t
grub_tpm_measure (unsigned char *buf, grub_size_t size, grub_uint8_t pcr,
		  const char *kind, const char *description)
{
  grub_err_t ret;
  char *desc;

  if (!grub_tpm)
    return GRUB_ERR_NONE;

  desc = grub_xasprintf("%s %s", kind, description);
  if (!desc)
    return GRUB_ERR_OUT_OF_MEMORY;
  ret = grub_tpm->log_event(buf, size, pcr, desc);
  grub_free(desc);
  return ret;
}
