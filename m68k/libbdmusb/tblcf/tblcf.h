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

#ifndef TBLCF_H
#define TBLCF_H

#include "bdm_types.h"

/* returns version of the DLL in BCD format */
unsigned char tblcf_version(void);

/* closes currently open device */
//void tblcf_usb_close(int dev);

/* returns status of the last command: 0 on sucess and non-zero on failure */
unsigned char tblcf_get_last_sts(int dev);

/* resynchronizes communication with the target (in case of noise, etc.);
 * returns 0 on success and non-zero on failure */
unsigned char tblcf_resynchronize(int dev);

/* asserts the TA signal for the specified time (in 10us ticks); returns 0 on
 * success and non-zero on failure */
unsigned char tblcf_assert_ta(int dev, unsigned char duration_10us);

/* JTAG - go from RUN-TEST/IDLE to SHIFT-DR (mode==0) or SHIFT-IR (mode!=0)
 * state */
unsigned char tblcf_jtag_sel_shift(int dev, unsigned char mode);

/* JTAG - go from RUN-TEST/IDLE to TEST-LOGIC-RESET state */
unsigned char tblcf_jtag_sel_reset(int dev);

/* JTAG - write data; parameter exit: ==0 : stay in SHIFT-xx, !=0 : go to
 * RUN-TEST/IDLE when done; data: shifted in LSB (last byte) first, unused bits
 * (if any) are in the MSB (first) byte */
void tblcf_jtag_write(int dev, unsigned char bit_count, 
                      unsigned char exit, unsigned char *buffer);

/* JTAG - read data; parameter exit: ==0 : stay in SHIFT-xx, !=0 : go to
 * RUN-TEST/IDLE when done; data: shifted in LSB (last byte) first, unused bits
 * (if any) are in the MSB (first) byte */
unsigned char tblcf_jtag_read(int dev, unsigned char bit_count, 
                              unsigned char exit, unsigned char *buffer);


#endif /* TBLCF_H */
