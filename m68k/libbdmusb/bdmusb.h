/*
    BDM USB common code 
    Copyright (C) 2010 Rafael Campos 
    Rafael Campos Las Heras <rafael@freedom.ind.br>

    Turbo BDM Light ColdFire
    Copyright (C) 2006  Daniel Malik

    Changed to support the BDM project.
    Chris Johns (cjohns@user.sourgeforge.net)
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef _BDMUSB_H_
#define _BDMUSB_H_

#include "libusb-1.0/libusb.h"
#include "bdm_types.h"


static unsigned int tblcf_dev_count;
static unsigned int pe_dev_count;
static unsigned int osbdm_dev_count;
static unsigned int usb_dev_count;
static bdmusb_dev    *usb_devs;

/**
  * Macros to get the number of devices 
  */
#define tblcf_usb_cnt()		(tblcf_dev_count)
#define pe_usb_cnt()		(pe_dev_count)
#define osbdm_usb_cnt()		(osbdm_dev_count)
#define bdmusb_usb_cnt()	(usb_dev_count)

//void tblcf_usb_find_devices(unsigned short int product_id);
void bdmusb_find_supported_devices(void);
void bdmusb_dev_name(int dev, char *name, int namelen);

/* returns version of the DLL in BCD format */
unsigned char bdmusb_version(void);

/* initialises USB and returns number of devices found */
unsigned char bdmusb_init(void);

#endif /* _BDMUSB_H_ */
