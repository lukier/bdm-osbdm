/*
 * Motorola Background Debug Mode Remote Library
 * Copyright (C) 1998  Chris Johns
 *
 * Based on `ser-tcp.c' in the gdb sources.
 *
 * 31-11-1999 Chris Johns (ccj@acm.org)
 * Extended to support remote operation. See bdmRemote.c for details.
 *
 * Chris Johns
 * Objective Design Systems
 * 35 Cairo Street
 * Cammeray, Sydney, 2062, Australia
 *
 * ccj@acm.org
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

/*
  #define BDM_MESSAGE_DEBUG
*/

#include <ctype.h>
#include <stdlib.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>

#define BDM_REMOTE_OPEN_WAIT (15)
#define BDM_REMOTE_TIMEOUT   (5)
#define BDM_REMOTE_BUF_SIZE  (4096)

/*
 * Limit of one BDM per process
 */
static const char *hex = "0123456789abcdef";

/*
 * Ioctl code translation.
 */
static const int ioctl_code_table[] = {
  BDM_INIT,
  BDM_RESET_CHIP,
  BDM_RESTART_CHIP,
  BDM_STOP_CHIP,
  BDM_STEP_CHIP,
  BDM_GET_STATUS,
  BDM_SPEED,
  BDM_DEBUG,
  BDM_RELEASE_CHIP,
  BDM_GO,
  BDM_READ_REG,
  BDM_READ_SYSREG,
  BDM_READ_LONGWORD,
  BDM_READ_WORD,
  BDM_READ_BYTE,
  BDM_WRITE_REG,
  BDM_WRITE_SYSREG,
  BDM_WRITE_LONGWORD,
  BDM_WRITE_WORD,
  BDM_WRITE_BYTE,
  BDM_GET_DRV_VER,
  BDM_GET_CPU_TYPE,
  BDM_GET_IF_TYPE
};

#if !defined (__CYGWIN__)

#include <netinet/tcp.h>

#define remote_close close
#define remote_write write
#define remote_read  read

#endif

/*
 * Reference in the open.
 */
static int bdmRemoteClose (int fd);

/*
 * Like strerror(), but knows about BDM driver errors, too.
 */
static const char *
bdmRemoteStrerror (int error_no)
{
  switch (error_no) {
    case BDM_FAULT_TIMEOUT: return "Remote BDM server timeout";
  }
  return strerror (error_no);  
}

/*
 * Translation of local ioctl numbers to a protocol independent number
 * to send over the link. Different client and server operating systems
 * will generate different numbers.
 */

static int
bdmGenerateIOId (int code)
{
  int id = 0;

  while (id < (sizeof (ioctl_code_table) / sizeof (int)))
    if (ioctl_code_table[id] == code)
      return id;
    else
      id++;
  return -1;
}

static int
bdmRemoteWait (int fd, char *buf, int buf_len)
{
  int            numfds;
  struct timeval tv;
  fd_set         readfds;
  fd_set         exceptfds;
  int            cread;

  FD_ZERO (&readfds);
  FD_ZERO (&exceptfds);

  tv.tv_sec  = BDM_REMOTE_TIMEOUT;
  tv.tv_usec = 0;

  FD_SET (fd, &readfds);
  FD_SET (fd, &exceptfds);

  while (1)
  {
    numfds = select (fd + 1, &readfds, 0, &exceptfds, &tv);
    if (numfds <= 0) {
      if (errno != EINTR) {
        if ((errno == 0) && (tv.tv_sec == 0) && (tv.tv_usec == 0))
          errno = BDM_FAULT_TIMEOUT;
        return -1;
      }
    }
    else {
      memset (buf, 0, buf_len);
      
      cread = remote_read (fd, buf, buf_len);

      if (cread > 0) {
#ifdef BDM_MESSAGE_DEBUG        
        printf ("bdm-remote: [%d] %s\n", cread, buf);
#endif
        return cread;
      }
      
      if (cread < 0)
        if (errno != EINTR)
          return -1;
    }
  }
  return -1;
}

static int
bdmRemoteName (const char *name)
{
  char           lname[MAXHOSTNAMELEN];
  char           *port;
  struct hostent *hostent;

  /*
   * If the user supplies a port, strip it.
   */

  strncpy (lname, name, MAXHOSTNAMELEN);
  
  port = strchr (lname, ':');

  if (port)
    lname[port - lname] = '\0';

  hostent = gethostbyname (lname);

  if (!hostent) {
    errno = ENOENT;
    return 0;
  }
  return 1;
}

/*
 * Open a remote link. The name is of the form :
 *
 *       host:port/device
 */
static int
bdmRemoteOpen (const char *name)
{
  int                fd = -1;
  char               lname[256];
  char               *port_str;
  char               *device;
  int                port = 0;
  struct hostent     *hostent;
  struct sockaddr_in sockaddr;
  struct protoent    *protoent;
  int                reties;
  int                optarg;
  char               buf[BDM_REMOTE_BUF_SIZE];
  int                buf_len;
  char               *s;


  strncpy (lname, name, 256);
  
  port_str = strchr (lname, ':');

  if (port_str) {
    lname[port_str - lname] = '\0';
    port_str++;
    port = atoi (port_str);
  }

  /*
   * We cannot have a port of 0.
   */

  if (port == 0)
    port = 6543;
  
  /*
   * If we have another colon in the string we must assume
   * that another host name is present and we are tring to
   * access via a proxy server. The port string may be
   * present if a number exists or it could be an IP address.
   *
   */

  if (strchr (port_str, ':')) {
    /*
     * We have another host being defined. If a number is the
     * first character of the port field, look for a dot which
     * would make it an IP address. If not the number to the
     * colon will be a port number.
     */

    device = port_str;

    if (isdigit (port_str[0])) {
      while (*port_str != ':') {
        if (!isdigit (*port_str))
          break;
      }
      if (*port_str == ':')
        device = port_str + 1;
    }
  }
  else
    device = strchr (name, '/');

  if (!device) {
    errno = ENOENT;
    return -1;
  }

  /*
   * Get the host name.
   */

  hostent = gethostbyname (lname);

  if (!hostent) {
    errno = ENOENT;
    return -1;
  }

  /*
   * Attempt to make a connection for 15 second. That is
   * 15 attempts a second apart.
   */
  
  for (reties = 0; reties < BDM_REMOTE_OPEN_WAIT; reties++)
  {
    fd = socket (PF_INET, SOCK_STREAM, 0);
    if (fd < 0)
      return -1;

    /*
     * Allow rapid reuse of this port.
     */
    
    optarg = 1;
    if (setsockopt (fd, SOL_SOCKET, SO_REUSEADDR,
                    (char *) &optarg, sizeof (optarg)) < 0) {
      int save_errno = errno;
      remote_close (fd);
      fd = -1;
      errno = save_errno;
      return -1;
    }

    /*
     * Enable TCP keep alive process.
     */
    
    optarg = 1;
    if (setsockopt (fd, SOL_SOCKET, SO_KEEPALIVE,
                    (char *) &optarg, sizeof (optarg)) < 0) {
      int save_errno = errno;
      remote_close (fd);
      fd = -1;
      errno = save_errno;
      return -1;
    }

    sockaddr.sin_family = PF_INET;
    sockaddr.sin_port   = htons (port);
    
    memcpy (&sockaddr.sin_addr.s_addr,
            hostent->h_addr,
            sizeof (struct in_addr));

    if (!connect (fd,
                  (struct sockaddr *) &sockaddr,
                  sizeof (sockaddr)))
      break;

    remote_close (fd);
    fd = -1;
    
    /*
     * We retry for ECONNREFUSED because that is often a temporary
     * condition, which happens when the server is being restarted.
     */

    if (errno != ECONNREFUSED)
      return -1;

    sleep (1);
  }

  if (reties == BDM_REMOTE_OPEN_WAIT) {
    errno = ENXIO;
    return -1;
  }
  
  protoent = getprotobyname ("tcp");
  if (!protoent) {
    remote_close (fd);
    return -1;
  }
  
  optarg = 1;
  if (setsockopt (fd, protoent->p_proto,
                  TCP_NODELAY, (char *) &optarg, sizeof (optarg))) {
    int save_errno = errno;
    remote_close (fd);
    fd = -1;
    errno = save_errno;
    return -1;
  }
  
  /*
   * If we don't do this, then GDB simply exits when the remote
   * side dies.
   */
  signal (SIGPIPE, SIG_IGN);

  /*
   * Say hello. This will cause the server to open the port.
   */

  strcpy (buf, "HELO ");
  buf_len = sizeof "HELO " - 1;
  buf_len += sprintf (buf + buf_len, "%s", device);
  buf_len++;

  if (remote_write (fd, buf, buf_len) != buf_len)
    return -1;

  if (bdmRemoteWait (fd, buf, BDM_REMOTE_BUF_SIZE) < 0)
    return -1;

  /*
   * Unpack the result.
   */

  s = strstr (buf, "HELO");

  if (!s) {
    /* FIXME: need error message */
    return -1;
  }

  s += sizeof "HELO";

  errno = strtoul (s, NULL, 0);

  if (errno) {
    int save_errno = errno;
    bdmRemoteClose (fd);
    fd = -1;
    errno = save_errno;
  }

  return fd;
}

/*
 * Close a remote link.
 */
static int
bdmRemoteClose (int fd)
{
  char buf[BDM_REMOTE_BUF_SIZE];
  
  strcpy (buf, "QUIT Later.");
  remote_write (fd, buf, strlen (buf) + 1);

  return remote_close (fd);
}

/*
 * Do an int-argument  BDM ioctl
 */
static int
bdmRemoteIoctlInt (int fd, int code, int *var)
{
  char buf[BDM_REMOTE_BUF_SIZE];
  int  buf_len;
  char *s;
  int  id;

  /*
   * Get a network id for the IO code.
   */

  id = bdmGenerateIOId (code);

  if (id < 0) {
    errno = EINVAL;
    return -1;
  }

  /*
   * Pack the message and send.
   */
  
  strcpy (buf, "IOINT ");
  buf_len = sizeof "IOINT " - 1;
  buf_len += sprintf (buf + buf_len, "0x%x,0x%x", id, *var);
  buf_len++;
    
  if (remote_write (fd, buf, buf_len) != buf_len)
    return -1;

  if (bdmRemoteWait (fd, buf, BDM_REMOTE_BUF_SIZE) < 0)
    return -1;

  /*
   * Unpack the result.
   */
  
  s = strstr (buf, "IOINT");

  if (!s) {
    /* FIXME: need error message */
    return -1;
  }

  s += sizeof "IOINT";

  errno = strtoul (s, NULL, 0);
  
  s = strchr (s, ',') + 1;

  *var = strtoul (s, NULL, 0);
  
  if (errno)
    return -1;
  
  return 0;
}

/*
 * Do a command (no-argument) BDM ioctl
 */
static int
bdmRemoteIoctlCommand (int fd, int code)
{
  char buf[BDM_REMOTE_BUF_SIZE];
  int  buf_len;
  char *s;
  int  id;

  /*
   * Get a network id for the IO code.
   */

  id = bdmGenerateIOId (code);

  if (id < 0) {
    errno = EINVAL;
    return -1;
  }

  /*
   * Pack the message and send.
   */
  
  strcpy (buf, "IOCMD ");
  buf_len = sizeof "IOCMD " - 1;
  buf_len += sprintf (buf + buf_len, "0x%x", id);
  buf_len++;

  if (remote_write (fd, buf, buf_len) != buf_len)
    return -1;

  if (bdmRemoteWait (fd, buf, BDM_REMOTE_BUF_SIZE) < 0)
    return -1;

  /*
   * Unpack the result.
   */

  s = strstr (buf, "IOCMD");

  if (!s) {
    /* FIXME: need error message */
    return -1;
  }

  s += sizeof "IOCMD";

  errno = strtoul (s, NULL, 0);
  
  if (errno)
    return -1;
  
  return 0;
}

/*
 * Do a BDMioctl-argument BDM ioctl
 */
static int
bdmRemoteIoctlIo (int fd, int code, struct BDMioctl *ioc)
{
  char buf[BDM_REMOTE_BUF_SIZE];
  int  buf_len;
  char *s;
  int  id;

  /*
   * Get a network id for the IO code.
   */

  id = bdmGenerateIOId (code);

  if (id < 0) {
    errno = EINVAL;
    return -1;
  }

  /*
   * Pack the message and send.
   */
  
  strcpy (buf, "IOIO ");
  buf_len = sizeof "IOIO " - 1;
  buf_len += sprintf (buf + buf_len, "0x%x,0x%x,0x%x",
                      id, ioc->address, ioc->value);
  buf_len++;

  if (remote_write (fd, buf, buf_len) != buf_len)
    return -1;

  if (bdmRemoteWait (fd, buf, BDM_REMOTE_BUF_SIZE) < 0)
    return -1;

  /*
   * Unpack the result.
   */

  s = strstr (buf, "IOIO");

  if (!s) {
    /* FIXME: need error message */
    return -1;
  }

  s += sizeof "IOIO";

  errno = strtoul (s, NULL, 0);
  
  s = strchr (s, ',') + 1;
  
  ioc->address = strtoul (s, NULL, 0);

  s = strchr (s, ',') + 1;

  ioc->value = strtoul (s, NULL, 0);

  if (errno)
    return -1;
  
  return 0;
}

/*
 * Do a BDM read
 */
static int
bdmRemoteRead (int fd, unsigned char *cbuf, unsigned long nbytes)
{
  char          buf[BDM_REMOTE_BUF_SIZE];
  int           buf_index;
  int           buf_len;
  unsigned long remote_nbytes;
  unsigned long bytes;
  unsigned char octet;
  char          *s;

  /*
   * Pack the message and send. For a read send the address and length.
   * The server will return a protocol status code, the read protocol
   * label, errno, the length then the data.
   */
  
  strcpy (buf, "READ ");
  buf_len  = sizeof "READ " - 1;
  buf_len += sprintf (buf + buf_len, "%ld", nbytes);
  buf_len++;
  
  if (remote_write (fd, buf, buf_len) != buf_len)
    return -1;

  buf_len = bdmRemoteWait (fd, buf, BDM_REMOTE_BUF_SIZE);
  
  if (buf_len < 0)
    return -1;

  /*
   * Unpack the result.
   */

  s = strstr (buf, "READ");
  
  if (!s) {
    /* FIXME: need error message */
    return -1;
  }

  s += sizeof "READ";
  
  errno = strtoul (s, NULL, 0);
  
  s = strchr (s, ',') + 1;

  remote_nbytes = strtoul (s, NULL, 0);

  /*
   * We could receive an odd number of characters in a buffer
   * and we need an even number of characters to complete a
   * byte. So if a single character is left move it to the
   * start of the buffer and append the next buffer of data.
   */
  
  if (remote_nbytes) {
    bytes     = 0;
    buf_index = strchr (s, ',') - buf + 1;

    while (bytes < nbytes) {
      if (buf_len < 2) {
        int new_read;
        new_read = bdmRemoteWait (fd, buf + buf_len, BDM_REMOTE_BUF_SIZE - buf_len);
        if (new_read < 0)
          return -1;
        buf_len += new_read;
      }
    
      while ((bytes < nbytes) && ((buf_len - buf_index) > 1)) {
        if (buf[buf_index] > '9') {
          buf[buf_index] = tolower (buf[buf_index]);
          octet = buf[buf_index] - 'a' + 10;
        }
        else
          octet = buf[buf_index] - '0';
      
        buf_index++;
    
        octet <<= 4;

        if (buf[buf_index] > '9') {
          buf[buf_index] = tolower (buf[buf_index]);
          octet |= buf[buf_index] - 'a' + 10;
        }
        else
          octet |= buf[buf_index] - '0';
    
        buf_index++;

        *cbuf = octet;

        cbuf++;
        bytes++;
      }

      if (buf_index < buf_len) {
        buf[0]  = buf[buf_index + 1];
        buf_len = 1;
      }
      else
        buf_len = 0;
      
      buf_index = 0;
    }
  }
  return nbytes;
}

/*
 * Do a BDM write
 */
static int
bdmRemoteWrite (int fd, unsigned char *cbuf, unsigned long nbytes)
{
  char          buf[BDM_REMOTE_BUF_SIZE];
  int           buf_len;
  unsigned long bytes;
  char          *s;

  /*
   * Pack the message and send. A write is a matter of
   * formatting buffers of BDM_REMOTE_BUF_SIZE and streaming them to the
   * server. This server uses the number of bytes at the start
   * of the message to detect the number of bytes being sent.
   */

  if (nbytes == 0)
    return 0;
  
  strcpy (buf, "WRITE ");
  buf_len  = sizeof "WRITE " - 1;
  buf_len += sprintf (buf + buf_len, "%ld,", nbytes);
  bytes = 0;

  while (bytes < nbytes) {
    while ((bytes < nbytes) && ((BDM_REMOTE_BUF_SIZE - buf_len) > 1)) {
      buf[buf_len] = hex[(*cbuf >> 4) & 0xf];
      buf_len++;
      buf[buf_len] = hex[*cbuf & 0xf];
      buf_len++;
      cbuf++;
      bytes++;
    }
  
    if (remote_write (fd, buf, buf_len) != buf_len)
      return -1;

    buf_len = 0;
  }
  
  if (bdmRemoteWait (fd, buf, BDM_REMOTE_BUF_SIZE) < 0)
    return -1;

  /*
   * Unpack the result.
   */

  s = strstr (buf, "WRITE");

  if (!s) {
    /* FIXME: need error message */
    return -1;
  }

  s += sizeof "WRITE";

  errno = strtoul (s, NULL, 0);
  
  s = strchr (s, ',') + 1;
  
  nbytes = strtoul (s, NULL, 0);

  return nbytes;
}
