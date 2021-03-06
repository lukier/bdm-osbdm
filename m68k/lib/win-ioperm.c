/*
 * Motorola Background Debug Mode Driver
 * Copyright (C) 1998-2010  Chris Johns
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Windows support by:
 * Chris Johns
 * Contemporary Software
 * chris@contemporary.net.au
 *
 * Michael Cooper
 * Integrated Chipware
 * 1861 Wiehle Ave.
 * Reston, Virginia  20190, USA
 * mikec@chipware.com
 *
 * Rick Haubenstricker
 * Perceptron, Inc
 * 47827 Halyard Dr
 * Plymouth, Michigan 48170, USA
 * rickh@perceptron.com
 *
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <windows.h>
#include <win/win-io.h>

#define BDM_DEFAULT_DEBUG 0

static int debugLevel = BDM_DEFAULT_DEBUG;

/*
 ************************************************************************
 *     Override the C library function.                                 *
 ************************************************************************
 */

int
stat (const char *file_name, struct stat *buf)
{
  if (strncmp (file_name, "/dev/bdm", sizeof ("/dev/bdm") - 1) == 0 )
  {
     return 0 ;
  }
  else /* Check for real file */
  {
     int fd = open(file_name,O_RDONLY);

     if ( fd > 0 )
     {
        int status = fstat(fd, buf);

        close(fd);

        return status;
     }
     else
     {
        return fd;
     }
  }
}

/*
 ************************************************************************
 *   Windows driver support routines                                    *
 ************************************************************************
 */

#if !defined (__CYGWIN__)

#ifndef HZ
#define HZ 100
#endif

void
usleep(int usecs)
{
  LARGE_INTEGER lpc;
  LONGLONG      lpc2;
  LONGLONG      current_usecs;
  LONGLONG      final_usecs;

  static int           first_time = 1;
  static LARGE_INTEGER lpf;
  static LONGLONG      lpf2;

  /*
   * First time through, get frequency of high-resolution
   *  performance counter  (value is counts per second)
   */
  if (first_time) {
    QueryPerformanceFrequency (&lpf);
    lpf2 = *(LONGLONG *) &lpf;
    first_time = 0;
  }

  /*
   * Get current value of high-resolution performance counter
   *  (value in counts)
   */
  QueryPerformanceCounter (&lpc);
  lpc2 = *(LONGLONG *)&lpc;

  /*
   * Calculate desired final time in usecs
   */
  final_usecs = ((lpc2 * (LONGLONG) 1000000) / lpf2) + (LONGLONG) usecs;

  /*
   * Loop until current count when converted to usecs is greater
   *  than final time
   */
  do {
    /*
     * Get current value of high-resolution performance
     *  counter (value in counts)
     */
    QueryPerformanceCounter (&lpc);
    lpc2 = *(LONGLONG *) &lpc;

    /*
     * Calculate current time in usecs
     */
    current_usecs = (lpc2 * (LONGLONG) 1000000 / lpf2);
  }
  while (current_usecs < final_usecs);
}

#endif

/*
 ************************************************************************
 *       Good old-fashioned UNIX driver entry points                    *
 ************************************************************************
 */

static int bdm_dev_registered = 0;

/*
 * Unhook module from kernel
 */
static void bdm_cleanup_module (void)
{
  if (bdm_dev_registered) {
    bdm_dev_registered = 0;
    fprintf (stderr, "BDM driver unregistered.\n");
  }
}

/*
 * Basically just detect the port.
 */
static int
bdm_ioperm_init ()
{
  int           minor;
  HANDLE        h;
  OSVERSIONINFO osvi;
  BOOL          bOsVersionInfoEx;

  if (bdm_dev_registered)
    return 0;
  
  /*
   * Determine which operating system in use, if NT or 2000,
   * we need to enable the port.
   */
  ZeroMemory (&osvi, sizeof(OSVERSIONINFO));
  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

  if (!(bOsVersionInfoEx = GetVersionEx ((LPOSVERSIONINFO) &osvi))) {
    /*
     * If OSVERSIONINFOEX doesn't work, try OSVERSIONINFO.
     */
     osvi.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
     if (! GetVersionEx ((OSVERSIONINFO *) &osvi)) {
       fprintf (stderr, "error: unsupported Windows version.\n");
       errno = EIO;
       return -1;
     }
  }

  if (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT) {
    /*
     * Open access to parallel port using giveio
     */
    h = CreateFile ("\\\\.\\giveio", GENERIC_READ, 0, NULL,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
      fprintf (stderr, "error: could not access the GiveIO device.\n");
      errno = EIO;
      return -1;
    }
    CloseHandle (h);
  }

#ifdef BDM_VER_MESSAGE
  fprintf (stderr, "bdm_init %d.%d, " __DATE__ ", " __TIME__ "\n",
           BDM_DRV_VERSION >> 8, BDM_DRV_VERSION & 0xff);
#endif

  /*
   * Set up port numbers
   */
  for (minor = 0 ;
       minor < (sizeof(bdm_device_info) / sizeof(bdm_device_info[0])) ;
       minor++) {
    unsigned short port;
    struct BDM *self = &bdm_device_info[minor];

    /*
     * First set the default debug level.
     */

    self->debugFlag = BDM_DEFAULT_DEBUG;

    /*
     * Choose a port number
     */
    switch (BDM_IFACE_MINOR (minor)) {
      case 0:  port = 0x378;  break;  /* LPT1 */
      case 1:  port = 0x278;  break;  /* LPT2 */
      case 2:  port = 0x3bc;  break;  /* LPT3 */
      case 3:  port = 0x2bc;  break;  /* LPT4, ccj - made this up :-) */
      default:
        fprintf (stderr, "BDM driver has no address for LPT%d.\n",
                 BDM_IFACE_MINOR (minor) + 1);
        bdm_cleanup_module();
        errno = EIO;
        return -1;
    }

    /*
     * See if the port exists
     */

    self->exists = 1;

    outb (0x00, port);
    udelay (50);
    if (inb (port) != 0x00) {
      self->exists = 0;
      if (self->debugFlag)
        fprintf (stderr, "BDM driver cannot detect LPT%d.\n",
                 BDM_IFACE_MINOR (minor) + 1);
      continue;
    }

    sprintf (self->name, "bdm%d", minor);
    self->portBase    = self->dataPort = port;
    self->statusPort  = port + 1;
    self->controlPort = port + 2;
    self->delayTimer  = 0;

    switch (BDM_IFACE (minor)) {
      case BDM_CPU32_PD:       bdm_cpu32_pd_init_self (self); break;
      case BDM_CPU32_ICD:      bdm_cpu32_icd_init_self (self); break;
      case BDM_COLDFIRE_PE:    bdm_cf_pe_init_self (self); break;
      default:
        fprintf (stderr, "BDM driver has no interface for minor number\n");
        bdm_cleanup_module();
        errno = EIO;
        return -1;
    }
  }

  bdm_dev_registered = 1;

  return 0;
}

int
bdm_ioperm_close (int fd)
{
  bdm_close (fd);
  return 0;
}

unsigned long
bdm_ioperm_read (int fd, char *buf, unsigned long count)
{
  errno = bdm_read (fd, buf, count);
  if (errno)
    return -1;
  return count;
}

unsigned long
bdm_ioperm_write (int fd, char *buf, unsigned long count)
{
  errno = bdm_write (fd, buf, count);
  if (errno)
    return -1;
  return count;
}

int
bdm_ioperm_ioctl (int fd, unsigned int cmd, ...)
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
  if (!bdm_dev_registered) {

    switch (cmd) {
      case BDM_DEBUG:
        err = os_copy_in ((void*) &iarg, (void*) arg, sizeof iarg);
        break;
    }
    if (err)
      return err;

    if (debugLevel > 3)
      fprintf (stderr, "bdm_ioperm_ioctl cmd:0x%08x\n", cmd);

    switch (cmd) {
      case BDM_DEBUG:
        debugLevel = iarg;
        break;
    }
  }

  errno = bdm_ioctl (fd, cmd, (unsigned long) arg);
  if (errno)
    return -1;
  return 0;
}

static int
bdm_ioperm_ioctl_int (int fd, int code, int *var)
{
  return bdm_ioperm_ioctl (fd, code, var);
}

static int
bdm_ioperm_ioctl_command (int fd, int code)
{
  return bdm_ioperm_ioctl (fd, code, NULL);
}

static int
bdm_ioperm_ioctl_io (int fd, int code, struct BDMioctl *ioc)
{
  return bdm_ioperm_ioctl (fd, code, ioc);
}

static bdm_iface iopermIface = {
  .open = bdm_ioperm_open,
  .close = bdm_ioperm_close,
  .read = bdm_ioperm_read,
  .write = bdm_ioperm_write,
  .ioctl_int = bdm_ioperm_ioctl_int,
  .ioctl_io = bdm_ioperm_ioctl_io,
  .ioctl_cmd = bdm_ioperm_ioctl_command,
  .error_str = NULL
};

/*
 * The device is a device name of the form /dev/bdmcpu320 or
 * /dev/bdmcf0 where the /dev/bdm must be present
 * the next field can be cpu32 or cf followed by
 * a number which is the port.
 */

int
bdm_ioperm_open (const char *device, bdm_iface** iface)
{
  int port = 0;

  *iface = NULL;
  
  if (strncmp (device, "/dev/bdm", sizeof ("/dev/bdm") - 1) == 0)
  {
    if (bdm_ioperm_init () < 0)
      return -1;

    device += sizeof ("/dev/bdm") - 1;

    if (strncmp (device, "cpu32", 5) == 0)
    {
      port = 0;
      device += 5;  /* s.b. 5 */
    }
    /* Allow ICD access under cygwin */
    else if (strncmp (device, "icd", 3) == 0)
    {
      port = 8;
      device += 3;
    }
    else if (strncmp (device, "cf", 2) == 0)
    {
      port = 4;
      device += 2;
    }
    else
    {
      errno = ENOENT;
      return -1;
    }

    port += strtoul (device, 0, 0);
  }
  else
  {
    if (usb_bdm_init (device) < 0)
      return -1;
  }
  
  errno = bdm_open (port);
  if (errno)
    return -1;
  return port;
}
