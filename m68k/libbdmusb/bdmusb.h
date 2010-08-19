/*
    BDM USB common code 
    Copyright (C) 2010 Rafael Campos Las Heras <rafael@freedom.ind.br>

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

#define WRITE_BLOCK_CHECK /* Keep on. CCJ */

/** Type of BDM target */
typedef enum {
	T_HC12     = 0,     /** - HC12 or HCS12 target */
	T_HCS08	   = 1,     /** - HCS08 target */
	T_RS08	   = 2,     /** - RS08 target */
	T_CFV1     = 3,     /** - Coldfire Version 1 target */
	T_CFVx     = 4,     /** - Coldfire Version 2,3,4 target */
	T_JTAG     = 5,     /** - JTAG target - TAP is set to \b RUN-TEST/IDLE */
	T_EZFLASH  = 6,     /** - EzPort Flash interface (SPI?) */
	T_OFF      = 0xFF,  /** - Turn off interface (no target) */
} target_type_e;


/** Type of reset action required */
typedef enum {
	RESET_MODE_MASK   = (3<<0), /**  Mask for reset mode (SPECIAL/NORMAL) */
	RESET_SPECIAL     = (0<<0), /**  - Special mode [BDM active, Target halted] */
	RESET_NORMAL      = (1<<0), /**  - Normal mode [usual reset, Target executes] */

	RESET_TYPE_MASK   = (3<<2), /**  Mask for reset type (Hardware/Software/Power) */
	RESET_ALL         = (0<<2), /**  Use all reset stategies as appropriate */
	RESET_HARDWARE    = (1<<2), /**  Use hardware RESET pin reset */
	RESET_SOFTWARE    = (2<<2), /**  Use software (BDM commands) reset */
	RESET_POWER       = (3<<2), /**  Cycle power */

	// Legacy methods
	SPECIAL_MODE = RESET_SPECIAL|RESET_ALL,  /**  - Special mode [BDM active, Target halted] */
	NORMAL_MODE  = RESET_NORMAL|RESET_ALL,   /**  - Normal mode [usual reset, Target executes] */
} target_mode_e;

typedef enum {	/* target reset detection state */
	RESET_NOT_DETECTED=0,
	RESET_DETECTED=1
} reset_detection_e;

typedef enum {	/* target reset state */
	RSTO_ACTIVE=0,
	RSTO_INACTIVE=1
} reset_state_e;

typedef struct { /* state of BDM communication */
	reset_state_e reset_state;
	reset_detection_e reset_detection;
} bdmcf_status_t;

/* returns version of the DLL in BCD format */
unsigned char tblcf_version(void);

/* initialises USB and returns number of devices found */
unsigned char tblcf_init(void);

/* opens a device with given number (0...), returns 0 on success and 1 on error */
int tblcf_open(const char *device);

/* closes currently open device */
void tblcf_close(int dev);

/* returns hardware & software version of the cable in BCD format - SW version
 * in lower byte and HW version in upper byte */
unsigned int tblcf_get_version(int dev);

/* returns status of the last command: 0 on sucess and non-zero on failure */
unsigned char tblcf_get_last_sts(int dev);

/* returns status of the last command value */
unsigned char tblcf_get_last_sts_value(int dev);

/* requests bootloader execution on new power-up, returns 0 on success and
 * non-zero on failure */
unsigned char tblcf_request_boot(int dev);

/* sets target MCU type; returns 0 on success and non-zero on failure */
unsigned char tblcf_set_target_type(int dev, target_type_e target_type);

/* resets the target to normal or BDM mode; returns 0 on success and non-zero
 * on failure */
unsigned char tblcf_target_reset(int dev, target_mode_e target_mode);

/* fills user supplied structure with current state of the BDM communication
 * channel; returns 0 on success and non-zero on failure */
unsigned char tblcf_bdm_sts(int dev, bdmcf_status_t *bdmcf_status);

/* brings the target into BDM mode; returns 0 on success and non-zero on
 * failure */
unsigned char tblcf_target_halt(int dev);

/* starts target execution from current PC address; returns 0 on success and
 * non-zero on failure */
unsigned char tblcf_target_go(int dev);

/* steps over a single target instruction; returns 0 on success and non-zero on
 * failure */
unsigned char tblcf_target_step(int dev);

/* resynchronizes communication with the target (in case of noise, etc.);
 * returns 0 on success and non-zero on failure */
unsigned char tblcf_resynchronize(int dev);

/* asserts the TA signal for the specified time (in 10us ticks); returns 0 on
 * success and non-zero on failure */
unsigned char tblcf_assert_ta(int dev, unsigned char duration_10us);

/* reads control register at the specified address and writes its contents into
 * the supplied buffer; returns 0 on success and non-zero on failure */
unsigned char tblcf_read_creg(int dev, unsigned int address, unsigned long int * result);

/* writes control register at the specified address; returns 0 on success and
 * non-zero on failure */
void tblcf_write_creg(int dev, unsigned int address, unsigned long int value);

/* reads the specified debug register and writes its contents into the
 * supplied buffer; returns 0 on success and non-zero on failure */
unsigned char tblcf_read_dreg(int dev, unsigned char dreg_index, unsigned long int * result);

/* writes specified debug register */
void tblcf_write_dreg(int dev, unsigned char dreg_index, unsigned long int value);

/* reads the specified register and writes its contents into the supplied
 * buffer; returns 0 on success and non-zero on failure */
unsigned char tblcf_read_reg(int dev, unsigned char reg_index, unsigned long int * result);

/* writes specified register */
void tblcf_write_reg(int dev, unsigned char reg_index, unsigned long int value);

/* reads byte from the specified address; returns 0 on success and non-zero on
 * failure */
unsigned char tblcf_read_mem8(int dev, unsigned long int address, unsigned char * result);

/* reads word from the specified address; returns 0 on success and non-zero on
 * failure */
unsigned char tblcf_read_mem16(int dev, unsigned long int address, unsigned int * result);

/* reads long word from the specified address; returns 0 on success and
 * non-zero on failure */
unsigned char tblcf_read_mem32(int dev, unsigned long int address, unsigned long int * result);

/* writes byte at the specified address */
void tblcf_write_mem8(int dev, unsigned long int address, unsigned char value);

/* writes word at the specified address */
void tblcf_write_mem16(int dev, unsigned long int address, unsigned int value);

/* writes long word at the specified address */
void tblcf_write_mem32(int dev, unsigned long int address, unsigned long int value);

/* reads the requested number of bytes from target memory from the supplied
 * address and stores results into the user supplied buffer; uses byte accesses
 * only; returns 0 on success and non-zero on failure */
unsigned char tblcf_read_block8(int dev, unsigned long int address, 
                                unsigned long int bytecount, unsigned char *buffer);

/* reads the requested number of bytes from target memory from the supplied
 * address and stores results into the user supplied buffer; uses word
 * accesses; returns 0 on success and non-zero on failure */
unsigned char tblcf_read_block16(int dev, unsigned long int address, 
                                 unsigned long int bytecount, unsigned char *buffer);

/* reads the requested number of bytes from target memory from the supplied
 * address and stores results into the user supplied buffer; uses long word
 * accesses; returns 0 on success and non-zero on failure */
unsigned char tblcf_read_block32(int dev, unsigned long int address,
                                 unsigned long int bytecount, unsigned char *buffer);

/* writes the requested number of bytes to target memory from the supplied
 * address; uses byte accesses only; returns 0 on success and non-zero on
 * failure (must be compiled with WRITE_BLOCK_CHECK, otherwise always returns
 * 0) */
unsigned char tblcf_write_block8(int dev, unsigned long int address,
                                 unsigned long int bytecount, unsigned char *buffer);

/* writes the requested number of bytes to target memory at the supplied
 * address; uses word accesses; returns 0 on success and non-zero on failure
 * (must be compiled with WRITE_BLOCK_CHECK, otherwise always returns 0) */
unsigned char tblcf_write_block16(int dev, unsigned long int address,
                                  unsigned long int bytecount, unsigned char *buffer);

/* writes the requested number of bytes to target memory at the supplied
 * address; uses long word accesses; returns 0 on success and non-zero on
 * failure (must be compiled with WRITE_BLOCK_CHECK, otherwise always returns
 * 0) */
unsigned char tblcf_write_block32(int dev, unsigned long int address,
                                  unsigned long int bytecount, unsigned char *buffer);

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

#endif
