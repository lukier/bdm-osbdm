/*
 * Chris Johns <cjohns@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "bdm.h"
#include "BDMlib.h"

#include "tblcf/tblcf.h"
#include "tblcf/tblcf_usb.h"

#include "bdmusb.h"

#include "bdm-usb.h"

extern int debugLevel;

int os_copy_in (void *dst, void *src, int size);
int os_copy_out (void *dst, void *src, int size);

static int
bdm_usb_close (int fd)
{
  bdm_close (fd);
  tblcf_usb_close(fd);
  return 0;
}

static int
bdm_usb_read (int fd, unsigned char *buf, int count)
{
  errno = bdm_read (fd, buf, count);
  if (errno)
    return -1;
  return count;
}

static int
bdm_usb_write (int fd, unsigned char *buf, int count)
{
  errno = bdm_write (fd, buf, count);
  if (errno)
    return -1;
  return count;
}

static int
bdm_usb_ioctl (int fd, unsigned int cmd, ...)
{
  va_list       args;
  unsigned long *arg;

  int iarg;
  int err = 0;

  va_start (args, cmd);

  arg = va_arg (args, unsigned long *);

  /*
   * Pick up the argument
   */
  switch (cmd) {
    case BDM_DEBUG:
      err = os_copy_in ((void*) &iarg, (void*) arg, sizeof iarg);
      break;
  }
  if (err)
    return err;

  if (debugLevel > 3)
    bdmInfo ("bdm_usb_ioctl cmd:0x%08x\n", cmd);

  switch (cmd) {
    case BDM_DEBUG:
      debugLevel = iarg;
      break;
  }

  errno = bdm_ioctl (fd, cmd, (unsigned long) arg);
  if (errno)
    return -1;
  return 0;
}

static int
bdm_usb_ioctl_int (int fd, int code, int *var)
{
  return bdm_usb_ioctl (fd, code, var);
}

static int
bdm_usb_ioctl_command (int fd, int code)
{
  return bdm_usb_ioctl (fd, code, NULL);
}

static int
bdm_usb_ioctl_io (int fd, int code, struct BDMioctl *ioc)
{
  return bdm_usb_ioctl (fd, code, ioc);
}

static bdm_iface usbIface = {
  .open      = bdm_usb_open,
  .close     = bdm_usb_close,
  .read      = bdm_usb_read,
  .write     = bdm_usb_write,
  .ioctl_int = bdm_usb_ioctl_int,
  .ioctl_io  = bdm_usb_ioctl_io,
  .ioctl_cmd = bdm_usb_ioctl_command,
  .error_str = NULL
};

int
bdm_usb_open (const char *device, bdm_iface** iface)
{
  struct BDM *self;
  int         devs;
  int         dev;
  char        usb_device[256];
#if linux
  ssize_t     dev_size;
  char        udev_usb_device[256];
#endif

  *iface = NULL;
  
  /*
   * Initialise the USB layers.
   */
  devs = bdmusb_init ();

#ifdef BDM_VER_MESSAGE
  fprintf (stderr, "usb-bdm-init %d.%d, " __DATE__ ", " __TIME__ " devices:%d\n",
           BDM_DRV_VERSION >> 8, BDM_DRV_VERSION & 0xff, devs);
#endif

#if linux
  {
    struct stat sb;
    char        *p;

    if (lstat (device, &sb) == 0)
    {
      if (S_ISLNK (sb.st_mode))
      {
        dev_size = readlink (device, udev_usb_device, sizeof (udev_usb_device));

        if ((dev_size >= 0) || (dev_size < sizeof (udev_usb_device) - 1))
        {
          udev_usb_device[dev_size] = '\0';

          /*
           * On Linux this is bus/usb/....
           */

          if (strncmp (udev_usb_device, "bus/usb", sizeof ("bus/usb") - 1) == 0)
          {
            memmove (udev_usb_device, udev_usb_device + sizeof ("bus/usb"),
                     sizeof (udev_usb_device) - sizeof ("bus/usb") - 1);

            p = strchr (udev_usb_device, '/');

            *p = '-';

            device = udev_usb_device;
          }
        }
      }
    }
  }
#endif
  
  for (dev = 0; dev < devs; dev++)
  {
    char* name;
    int   length;

    bdmusb_dev_name (dev, usb_device, sizeof (usb_device));

    name = usb_device;
    length = strlen (usb_device);

    while (name && length)
    {
      name = strchr (name, device[0]);

      if (name)
      {
        length = strlen (name);

        if (strlen (device) <= length)
        {
          if (strncmp (device, name, length) == 0)
          {
            /*
             * Set up the self srructure. Only ever one with ioperm.
             */
            self = bdm_get_device_info (0);

            /*
             * Open the USB device.
             */
            self->usbDev = tblcf_open (usb_device);

            if (self->usbDev < 0)
            {
              errno = EIO;
              return -1;
            }

            /*
             * First set the default debug level.
             */

            self->debugFlag = BDM_DEFAULT_DEBUG;

            /*
             * Force the port to exist.
             */
            self->exists = 1;

            self->delayTimer = 0;
      
            bdm_tblcf_init_self (self);

            *iface = &usbIface;
            
            return 0;
          }
        }

        name++;
        length = strlen (name);
      }
    }
  }

  errno = ENOENT;
  return -1;
}