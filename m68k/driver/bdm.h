/*
 * Motorola Background Debug Mode Driver
 * Copyright (C) 1995  W. Eric Norum
 * Copyright (C) 1998  Chris Johns
 *
 * Based on:
 *  1. `A Background Debug Mode Driver Package for Motorola's
 *     16- and 32-Bit Microcontrollers', Scott Howard, Motorola
 *     Canada, 1993.
 *  2. `Linux device driver for public domain BDM Interface',
 *     M. Schraut, Technische Universitaet Muenchen, Lehrstuhl
 *     fuer Prozessrechner, 1995.
 *
 * Extended to support the ColdFire BDM interface using the P&E
 * module which comes with the EVB. Currently only tested with the
 * 5206 (5V) device.
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
 * W. Eric Norum
 * Saskatchewan Accelerator Laboratory
 * University of Saskatchewan
 * 107 North Road
 * Saskatoon, Saskatchewan, CANADA
 * S7N 5C6
 * 
 * eric@skatter.usask.ca
 *
 * Coldfire support by:
 * Chris Johns
 * Objective Design Systems
 * 35 Cairo Street
 * Cammeray, Sydney, 2062, Australia
 *
 * ccj@acm.org
 *
 */

#ifndef _BDM_H_
#define _BDM_H_

/*
 * Version of the driver. 
 */

#define BDM_DRV_VERSION   0x020b

/*
 * Hook for Linux kernel
 */

#define BDM_MAJOR_NUMBER  34

/*
 * Allocation of the minor numbers. The number of minors per interface
 * must be a factor of 2.
 */

#define BDM_MINORS_PER_IFACE 4
#define BDM_IFACE_MINOR(m)   (m & (BDM_MINORS_PER_IFACE - 1))
#define BDM_IFACE(m)         (m / BDM_MINORS_PER_IFACE)
#define BDM_NUM_OF_MINORS    (BDM_NUM_OF_IFACES * BDM_MINORS_PER_IFACE)

/*
 * Processors
 */

#define BDM_CPU32           0
#define BDM_COLDFIRE        1

/*
 * Interfaces, used of offset the major number.
 */

#define BDM_CPU32_ERIC      0  
#define BDM_CPU32_PD        BDM_CPU32_ERIC
#define BDM_COLDFIRE_PE     1
#define BDM_CPU32_ICD       2
#define BDM_NUM_OF_IFACES   3

/*
 * Error codes
 */
#define BDM_FAULT_UNKNOWN    210
#define BDM_FAULT_POWER      211 
#define BDM_FAULT_CABLE      212
#define BDM_FAULT_RESPONSE   213
#define BDM_FAULT_RESET      214
#define BDM_FAULT_PORT       215
#define BDM_FAULT_BERR       216
#define BDM_FAULT_NVC        217
#define BDM_FAULT_TIMEOUT    218

/*
 * Structure for I/O requests
 * Address and value are in host-endian order
 */
struct BDMioctl {
    unsigned int    address;
    unsigned int    value;
};

/*
 * The ioctl codes. If these change insure the remote client and server
 * interfaces are kept in sync. Assumes Cygwin does not define Win32.
 */

#if !defined (_IO)
#if defined (__WIN32__)
#include <winsock2.h>
#else
#include <sys/ioctl.h>
#endif
#endif

/*
 * If the OS does not provide any ioctl support as found on
 * some Unix systems then provide something.
 */
#if !defined (_IO)
#undef _IOR
#undef _IOW
#undef _IOWR
#define _IO(x,y)       ((x<<8)|y|0x00000)
#define _IOR(x,y,t)    ((x<<8)|y|0x10000)
#define _IOW(x,y,t)    ((x<<8)|y|0x20000)
#define _IOWR(x,y,t)   ((x<<8)|y|0x30000)
#endif

#if !defined (_IOWR)
#if !defined (IOC_OUTIN)
#define IOC_OUTIN   0x10000000      /* copy in parameters */
#endif
#define _IOWR(x,y,t) (IOC_OUTIN|(((long)sizeof(t)&IOCPARM_MASK)<<16)|(x<<8)|y)
#endif

#define BDM_INIT         _IO('B', 0)
#define BDM_RESET_CHIP   _IO('B', 1)
#define BDM_RESTART_CHIP _IO('B', 2)
#define BDM_STOP_CHIP    _IO('B', 3)
#define BDM_STEP_CHIP    _IO('B', 4)
#define BDM_GET_STATUS   _IOR('B', 5, int)
#define BDM_SPEED        _IOW('B', 6, int)
#define BDM_DEBUG        _IOW('B', 7, int)
#define BDM_RELEASE_CHIP _IO('B', 8)
#define BDM_GO           _IO('B', 9)

/*
 * Input/output requests
 */
/*
 * Addition for general register access.
 *
 * Note, the control and debug registers has been added at the start
 *       so the other allocated number do not change.
 */
#define BDM_READ_CTLREG    _IOWR('B', 16, struct BDMioctl)
#define BDM_WRITE_CTLREG   _IOW('B',  17, struct BDMioctl)
#define BDM_READ_DBREG     _IOWR('B', 18, struct BDMioctl)
#define BDM_WRITE_DBREG    _IOW('B',  19, struct BDMioctl)
#define BDM_READ_REG       _IOWR('B', 20, struct BDMioctl)
#define BDM_READ_SYSREG    _IOWR('B', 21, struct BDMioctl)
#define BDM_READ_LONGWORD  _IOWR('B', 22, struct BDMioctl)
#define BDM_READ_WORD      _IOWR('B', 23, struct BDMioctl)
#define BDM_READ_BYTE      _IOWR('B', 24, struct BDMioctl)
#define BDM_WRITE_REG      _IOW('B',  25, struct BDMioctl)
#define BDM_WRITE_SYSREG   _IOW('B',  26, struct BDMioctl)
#define BDM_WRITE_LONGWORD _IOW('B',  27, struct BDMioctl)
#define BDM_WRITE_WORD     _IOW('B',  28, struct BDMioctl)
#define BDM_WRITE_BYTE     _IOW('B',  29, struct BDMioctl)

/*
 * Detect the driver version, processor or interface type
 */
#define BDM_GET_DRV_VER    _IOR('B', 30, int)
#define BDM_GET_CPU_TYPE   _IOR('B', 31, int)
#define BDM_GET_IF_TYPE    _IOR('B', 32, int)

/*
 * Coldfire specific call to control the use of the
 * PST signals. This is only needed on 5206e targets that
 * use the PST signals for IO.
 */

#define BDM_GET_CF_PST     _IOR('B', 33, int)
#define BDM_SET_CF_PST     _IOR('B', 34, int)

/*
 * bits in status word returned by BDM_GET_STATUS ioctl
 */
#define BDM_TARGETRESET    (1 << 0)    /* Target reset */
#define BDM_TARGETHALT     (1 << 1)    /* Target halt */
#define BDM_TARGETSTOPPED  (1 << 2)    /* Target stopped */
#define BDM_TARGETPOWER    (1 << 3)    /* Power failed */
#define BDM_TARGETNC       (1 << 4)    /* Target not connected */

/*
 * Register codes for BDM_READ_SYSREG/BDM_WRITE_SYSREG ioctls
 *
 * These are the control and debug registers for the CPU32 and
 * Coldfire processor.
 *
 * These are ony logical numbers not the actual registers values used
 * on the BDM port. The driver maps these to the correct command and
 * register pair .
 *
 * Using only the one call keeps the changes to Eric's library and gdb
 * code to a minimum.
 *
 * The WR only registers are shadowed in the driver.
 */
#define BDM_REG_RPC    0x0     /* CPU32, Coldfire */
#define BDM_REG_PCC    0x1     /* CPU32 */
#define BDM_REG_SR     0x2     /* CPU32, Coldfire */
#define BDM_REG_USP    0x3     /* CPU32 */
#define BDM_REG_SSP    0x4     /* CPU32 */
#define BDM_REG_SFC    0x5     /* CPU32 */
#define BDM_REG_DFC    0x6     /* CPU32 */
#define BDM_REG_ATEMP  0x7     /* CPU32 */
#define BDM_REG_FAR    0x8     /* CPU32 */
#define BDM_REG_VBR    0x9     /* CPU32, Coldfire */
#define BDM_REG_CACR   0xa     /* Coldfire */
#define BDM_REG_ACR0   0xb     /* Coldfire */
#define BDM_REG_ACR1   0xc     /* Coldfire */
#define BDM_REG_RAMBAR 0xd     /* Coldfire */
#define BDM_REG_MBAR   0xe     /* Coldfire */
#define BDM_REG_CSR    0xf     /* Coldfire */
#define BDM_REG_AATR   0x10    /* WR only, Coldfire */
#define BDM_REG_TDR    0x11    /* WR only, Coldfire */
#define BDM_REG_PBR    0x12    /* WR only, Coldfire */
#define BDM_REG_PBMR   0x13    /* WR only, Coldfire */
#define BDM_REG_ABHR   0x14    /* WR only, Coldfire */
#define BDM_REG_ABLR   0x15    /* WR only, Coldfire */
#define BDM_REG_DBR    0x16    /* WR only, Coldfire */
#define BDM_REG_DBMR   0x17    /* WR only, Coldfire */
#define BDM_MAX_SYSREG (BDM_REG_DBMR + 1)

/*
 * Register codes for BDM_READ_REG/BDM_WRITE_REG ioctls
 */
#define BDM_REG_D0    0x0
#define BDM_REG_D1    0x1
#define BDM_REG_D2    0x2
#define BDM_REG_D3    0x3
#define BDM_REG_D4    0x4
#define BDM_REG_D5    0x5
#define BDM_REG_D6    0x6
#define BDM_REG_D7    0x7
#define BDM_REG_A0    0x8
#define BDM_REG_A1    0x9
#define BDM_REG_A2    0xa
#define BDM_REG_A3    0xb
#define BDM_REG_A4    0xc
#define BDM_REG_A5    0xd
#define BDM_REG_A6    0xe
#define BDM_REG_A7    0xf  /* use this for the stack pointer */

#endif /* _BDM_H_ */
