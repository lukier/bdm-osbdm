/* BDM/m68k specific low level interface, for the remote server for GDB.
   Copyright (C) 1989, 1993, 1994, 1995, 1997, 1998, 1999, 2000, 2002, 2003,
   2004, 2005, 2006, 2007 Free Software Foundation, Inc.
   Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   2007 Chris Johns (chrisj@rtems.org).
   Copyright (C) 1995  W. Eric Norum
   Copyright (C) 2000  Bryan Feir (bryan@sgl.crestech.ca)

   This file is part of M68K BDM.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

   The code in the this file is based on the GDB server code in GDB and
   the code in M68K BDM GDB patch file called 'remote-m68k-bdm.c'.
   Based on:
    1. `A Background Debug Mode Driver Package for Motorola's
       16-bit and 32-Bit Microcontrollers', Scott Howard, Motorola
       Canada, 1993.
    2. `Linux device driver for public domain BDM Interface',
       M. Schraut, Technische Universitaet Muenchen, Lehrstuhl
       fuer Prozessrechner, 1995.                                      */

#include <ctype.h>
#include <inttypes.h>
#include <unistd.h>

#include "server.h"
#include "regdef.h"

#include "m68k-bdm-low.h"

/*
 * Compare a string with a constant string.
 */
#define M68K_BDM_STR_IS(_s, _c)                  \
  (strncmp (_s, _c, sizeof (_c) - 1) == 0)

/*
 * Map the internal GDB wait kinds to the response letters for the
 * remote protocol.
 */
#define TARGET_WAITKIND_STOPPED 'S'
#define TARGET_WAITKIND_TRAP    'T'
#define TARGET_WAITKIND_EXITED  'X'

/*
 * The version of BDM hardware.
 */
#define M68K_BDM_VER_A                 (0)
#define M68K_BDM_VER_B                 (1)
#define M68K_BDM_VER_B_PLUS            (9)
#define M68K_BDM_CFV1_VER_B_PLUS      (10) // This is the BDM B+ converted to HCS08 1-pin BDM serial interface.
#define M68K_BDM_VER_C                 (2)
#define M68K_BDM_VER_D                 (3)
#define M68K_BDM_VER_D_PLUS           (12)
#define M68K_BDM_VER_D_PLUS_PST_BUFF  (15)


/*
 * The type of CPU.
 */
#define M68K_BDM_MARCH_68000      (0)
#define M68K_BDM_MARCH_CPU32      (1)
#define M68K_BDM_MARCH_CPU32PLUS  (2)
#define M68K_BDM_MARCH_CF5200     (3)
#define M68K_BDM_MARCH_CF5235     (4)
#define M68K_BDM_MARCH_CF5272     (5)
#define M68K_BDM_MARCH_CF5282     (6)
#define M68K_BDM_MARCH_CF52223    (7)
#define M68K_BDM_MARCH_CF5307     (8)
#define M68K_BDM_MARCH_CFV4E      (9)
#define M68K_BDM_MARCH_CFV1      (10)

/*
 * The CPU labels.
 */
#define M68K_BDM_MARCH_68000_LABEL     "68000"
#define M68K_BDM_MARCH_CPU32_LABEL     "CPU32"
#define M68K_BDM_MARCH_CPU32PLUS_LABEL "CPU32+"
#define M68K_BDM_MARCH_CF5200_LABEL    "CF5200"
#define M68K_BDM_MARCH_CF5235_LABEL    "CF5235"
#define M68K_BDM_MARCH_CF5272_LABEL    "CF5272"
#define M68K_BDM_MARCH_CF5282_LABEL    "CF5282"
#define M68K_BDM_MARCH_CF52223_LABEL   "CF52223"
#define M68K_BDM_MARCH_CF5307_LABEL    "CF5307"
#define M68K_BDM_MARCH_CFV4E_LABEL     "CFV4E"
#define M68K_BDM_MARCH_CFV1_LABEL      "CFV1"

/*
 * The size of the register in bits.
 */
static const int m68k_bdm_reg_sizes[4] = { 32, 32, 32, 65 };

/*
 * The array of registers for each support processor.
 */
struct m68k_bdm_registers {
  const char const*                  xml_name;
  const struct m68k_bdm_reg_mapping* map;
  const int*                         num_regs;
  int                                watchpoints;
  int                                breakpoints;
};

/*
 * The number of registers.
 */
#define M68K_BDM_NUM_REGS_BDM (*m68k_bdm_regs->num_regs)

/*
 * Get a BDM register.
 */
#define M68K_BDM_REG_XML()             (m68k_bdm_regs->xml_name)
#define M68K_BDM_REG_NAME_INDEXED(_r)  (m68k_bdm_regs->map[_r].name)
#define M68K_BDM_REG_TYPE_INDEXED(_r)  (m68k_bdm_regs->map[_r].type)
#define M68K_BDM_REG_NUM_INDEXED(_r)   (m68k_bdm_regs->map[_r].num)
#define M68K_BDM_REG_CODE_INDEXED(_r)  (m68k_bdm_regs->map[_r].code)
#define M68K_BDM_REG_FLAGS_INDEXED(_r) (m68k_bdm_regs->map[_r].flags)
#define M68K_BDM_REG_SIZE_INDEXED(_r)  (m68k_bdm_reg_sizes[M68K_BDM_REG_TYPE_INDEXED (_r)])
#define M68K_BDM_REG_NAME(_r)          M68K_BDM_REG_NAME_INDEXED(m68k_bdm_register_index (_r))
#define M68K_BDM_REG_TYPE(_r)          M68K_BDM_REG_TYPE_INDEXED(m68k_bdm_register_index (_r))
#define M68K_BDM_REG_NUM(_r)           M68K_BDM_REG_NUM_INDEXED(m68k_bdm_register_index (_r))
#define M68K_BDM_REG_CODE(_r)          M68K_BDM_REG_CODE_INDEXED(m68k_bdm_register_index (_r))
#define M68K_BDM_REG_FLAGS(_r)         M68K_BDM_REG_FLAGS_INDEXED(m68k_bdm_register_index (_r))
#define M68K_BDM_REG_SIZE(_r)          M68K_BDM_REG_SIZE_INDEXED(m68k_bdm_register_index (_r))

/*
 * The supported registers maps.
 */

extern const struct m68k_bdm_reg_mapping m68k_bdm_68000_reg_map[];
extern const int m68k_bdm_68000_reg_map_size;
extern const struct m68k_bdm_reg_mapping m68k_bdm_cpu32_reg_map[];
extern const int m68k_bdm_cpu32_reg_map_size;
extern const struct m68k_bdm_reg_mapping m68k_bdm_cpu32plus_reg_map[];
extern const int m68k_bdm_cpu32plus_reg_map_size;
extern const struct m68k_bdm_reg_mapping m68k_bdm_cf5200_reg_map[];
extern const int m68k_bdm_cf5200_reg_map_size;
extern const struct m68k_bdm_reg_mapping m68k_bdm_cf52223_reg_map[];
extern const int m68k_bdm_cf52223_reg_map_size;
extern const struct m68k_bdm_reg_mapping m68k_bdm_cf5235_reg_map[];
extern const int m68k_bdm_cf5235_reg_map_size;
extern const struct m68k_bdm_reg_mapping m68k_bdm_cf5272_reg_map[];
extern const int m68k_bdm_cf5272_reg_map_size;
extern const struct m68k_bdm_reg_mapping m68k_bdm_cf5282_reg_map[];
extern const int m68k_bdm_cf5282_reg_map_size;
extern const struct m68k_bdm_reg_mapping m68k_bdm_cf5307_reg_map[];
extern const int m68k_bdm_cf5307_reg_map_size;
extern const struct m68k_bdm_reg_mapping m68k_bdm_cfv4e_reg_map[];
extern const int m68k_bdm_cfv4e_reg_map_size;
extern const struct m68k_bdm_reg_mapping m68k_bdm_cfv1_reg_map[];
extern const int m68k_bdm_cfv1_reg_map_size;

/*
 * Must match the type of CPU index at the start of this file.
 */
const struct m68k_bdm_registers m68k_bdm_reg_map[] = {
  /* 0 */ { "m68k-core.xml",
    m68k_bdm_68000_reg_map, &m68k_bdm_68000_reg_map_size, 0, 0 },
  /* 1 */ { "m68k-cpu32.xml",
    m68k_bdm_cpu32_reg_map, &m68k_bdm_cpu32_reg_map_size, 0, 0 },
  /* 2 */ { "m68k-cpu32plus.xml",
    m68k_bdm_cpu32plus_reg_map, &m68k_bdm_cpu32plus_reg_map_size, 0, 0 },
  /* 3 */ { "m68k-cf5200.xml",
    m68k_bdm_cf5200_reg_map, &m68k_bdm_cf5200_reg_map_size, 1, 1 },
  /* 4 */ { "m68k-cf5235.xml",
    m68k_bdm_cf5235_reg_map, &m68k_bdm_cf5235_reg_map_size, 1, 1 },
  /* 5 */ { "m68k-cf5272.xml",
    m68k_bdm_cf5272_reg_map, &m68k_bdm_cf5272_reg_map_size, 1, 1 },
  /* 6 */ { "m68k-cf5282.xml",
    m68k_bdm_cf5282_reg_map, &m68k_bdm_cf5282_reg_map_size, 1, 1 },
  /* 7 */ { "m68k-cf52223.xml",
    m68k_bdm_cf52223_reg_map, &m68k_bdm_cf52223_reg_map_size, 1, 4 },
  /* 8 */ { "m68k-cf5307.xml",
    m68k_bdm_cf5307_reg_map, &m68k_bdm_cf5307_reg_map_size, 1, 1 },
  /* 9 */ { "m68k-cfv4e.xml",
    m68k_bdm_cfv4e_reg_map, &m68k_bdm_cfv4e_reg_map_size, 2, 4 },
  /* 10 */ { "m68k-cfv1.xml",
    m68k_bdm_cfv1_reg_map, &m68k_bdm_cfv1_reg_map_size, 1, 3 }
};

const char *m68k_bdm_expedite_regs[] = { "sp", "fp", "pc", 0 };

/*
 * The name of the BDM driver special file
 */
static char*       m68k_bdm_dev_name;
static int         m68k_bdm_cpu_family;
static int         m68k_bdm_cpu_type = M68K_BDM_MARCH_68000;
static const char* m68k_bdm_cpu_label = M68K_BDM_MARCH_68000_LABEL;
static const struct m68k_bdm_registers *m68k_bdm_regs = &m68k_bdm_reg_map[M68K_BDM_MARCH_68000];

static int m68k_bdm_kill_on_exit;

/*
 * Hold BDM ATEMP register (CPU32 only).
 */
static unsigned long m68k_bdm_atemp;
static int m68k_bdm_have_atemp;

/*
 * CF BDM Debug hardware version number.
 */

static unsigned long m68k_bdm_cf_debug_ver;

/*
 * Have we set a debug level.
 */
static int m68k_bdm_debug_level;

/*
 * give target time to come up after reset
 * time in usec
 */
#define M68K_BDM_TIME_TO_COME_UP  60000

/*
 * We are quitting, so do not call error and therefore
 * jump back into the main event handler.
 */
static int m68k_bdm_gdb_is_quitting;

/*
 * Release the processor on a close.
 */
static int m68k_bdm_release_on_exit;

/*
 * Our pid.
 */
#define null_ptid (0)

unsigned long m68k_bdm_ptid;

/*
 * The breakpoint codes for the different processors
 */
#define M68K_BDM_BREAKPOINT_SIZE_MAX (2)
static unsigned char  m68k_bdm_cpu32_breakpoint[] = {0x4a, 0xfa};
static unsigned char  m68k_bdm_cf_breakpoint[] = {0x4a, 0xc8};
static unsigned char* m68k_bdm_breakpoint_code = (unsigned char*) "\x4e\x41";
static unsigned int   m68k_bdm_breakpoint_size = 2;

/*
 * Trial of not stopping on an error.
 */
static int m68k_bdm_use_error;

/*
 * Treat all breakpoints as hardware breakpoints.
 */
static int m68k_bdm_breakpoints_hard;

/*
 * Display error message and jump back to main input loop
 */
static void
m68k_bdm_report_error (void)
{
  if (!m68k_bdm_gdb_is_quitting)
  {
    if (m68k_bdm_use_error)
      error ("m68k-bdm: error: %s", bdmErrorString ());
    else
      printf_filtered ("m68k-bdm: error: %s", bdmErrorString ());
  }
}

static void
m68k_bdm_help (void)
{
  printf_filtered ("%s\n" \
              "%s -vVhBDd -t <time> <device>\n" \
              "\t-v\tVerbose. More than one the more verbose.\n" \
              "\t-V\tVersion.\n" \
              "\t-h\tThis help.\n" \
              "\t-B\tTreat all breakpoints as hardware breakpoints. Useful for flash.\n" \
              "\t-D\tDriver debug level. More than one for more debug.\n" \
              "\t-d\tBDM Library debug level. More than one for more debug.\n" \
              "\t-t time\tDelay timing for the parallel ports.\n" \
              "\tdevice\tThe device to connect to such as /dev/bdmcf0.\n",
              PACKAGE_STRING, PACKAGE_NAME);
}

static void
m68k_bdm_version (void)
{
  printf_filtered ("%s\n" \
              "%s is free software, covered by the GNU General Public License.\n" \
              "Reports bugs to %s.\n",
              PACKAGE_STRING, PACKAGE_NAME, PACKAGE_BUGREPORT);
}

static int
m68k_bdm_register_index (int regno)
{
  int index;
  for (index = 0; index < M68K_BDM_NUM_REGS_BDM; index++)
    if (M68K_BDM_REG_NUM_INDEXED (index) == regno)
      return index;
  fatal ("m68k-bdm: index: no register found: %d", regno);
}

static int
m68k_bdm_register_index_by_name (const char* name)
{
  int index;
  for (index = 0; index < M68K_BDM_NUM_REGS_BDM; index++)
    if (M68K_BDM_STR_IS (name, M68K_BDM_REG_NAME_INDEXED (index)))
      return index;
  fatal ("m68k-bdm: idxbyname: no register found: %s", name);
}

static int
m68k_bdm_register_by_name (const char* name)
{
  return M68K_BDM_REG_CODE_INDEXED (m68k_bdm_register_index_by_name (name));
}

static int
m68k_bdm_register_offset (int regno)
{
  int index;
  int offset = 0;
  for (index = 0; index < M68K_BDM_NUM_REGS_BDM; index++) {
    if (M68K_BDM_REG_NUM_INDEXED (index) == regno)
      return offset;
    if (M68K_BDM_REG_NUM_INDEXED (index) < regno)
      offset += M68K_BDM_REG_SIZE_INDEXED (index);
  }
  fatal ("m68k-bdm: offset: no register found: %d", regno);
}

static void
m68k_bdm_init_registers (void)
{
  int reg;
  struct reg *regs;

  m68k_bdm_regs = &m68k_bdm_reg_map[m68k_bdm_cpu_type];

  regs = calloc (sizeof (struct reg), M68K_BDM_NUM_REGS_BDM);

  if (!regs)
    fatal ("m68k-bdm: no memory for regs");

  for (reg = 0; reg < M68K_BDM_NUM_REGS_BDM; reg++) {
    regs[reg].name   = M68K_BDM_REG_NAME (reg);
    regs[reg].offset = m68k_bdm_register_offset (reg);
    regs[reg].size   = M68K_BDM_REG_SIZE (reg);
    regs[reg].flags  = M68K_BDM_REG_FLAGS (reg);
  }

  if (m68k_bdm_debug_level)
    printf_filtered ("m68k-bdm: registers xml:%s number:%d size:%d\n",
                M68K_BDM_REG_XML(), M68K_BDM_NUM_REGS_BDM,
                m68k_bdm_register_offset (M68K_BDM_NUM_REGS_BDM - 1));
  
  set_register_cache (regs, M68K_BDM_NUM_REGS_BDM);
  gdbserver_expedite_regs = m68k_bdm_expedite_regs;
}

/*
 * Read a system or control registers.
 */
static int
m68k_bdm_read_sys_ctl_reg (const char* name, int cregno, unsigned long* l)
{
  int ret;
  if (m68k_bdm_debug_level)
    printf_filtered ("m68K_bdm_read_sys_ctl_reg: %s (0x%03x)\n",
                name, cregno & BDM_REG_MASK);
  if (cregno & BDM_REG_VIRTUAL_REG) {
    *l = ((cregno & BDM_REG_MASK) << 16) | (cregno & BDM_REG_MASK);
    ret = 0;
  }
  else if (cregno & BDM_REG_CONTROL_REG)
    ret = bdmReadControlRegister (cregno & BDM_REG_CONTROL_REG_MASK, l);
  else if (cregno & BDM_REG_DEBUG_REG)
    ret = bdmReadDebugRegister (cregno & BDM_REG_DEBUG_REG_MASK, l);
  else
    ret = bdmReadSystemRegister (cregno, l);
  return ret;
}

/*
 * Write a system or control registers.
 */
static int
m68k_bdm_write_sys_ctl_reg (const char* name, int cregno, unsigned long l)
{
  int ret;
  if (m68k_bdm_debug_level)
    printf_filtered ("m68k_bdm_write_sys_ctl_reg: %s (0x%03x)\n",
                name, cregno & BDM_REG_CONTROL_REG_MASK);
  if (cregno & BDM_REG_CONTROL_REG)
    ret = bdmWriteControlRegister (cregno & BDM_REG_CONTROL_REG_MASK, l);
  else if (cregno & BDM_REG_DEBUG_REG)
    ret = bdmWriteDebugRegister (cregno & BDM_REG_DEBUG_REG_MASK, l);
  else
    ret = bdmWriteSystemRegister (cregno, l);
  return ret;
}

/*
 * The following routines handle the Coldfire hardware breakpoints.  Only one
 * breakpoint supported so far, and it can be either a PC breakpoint or an
 * address watchpoint.  Unfortunately, the TARGET_CAN_USE_HARDWARE_WATCHPOINT
 * macro only allows for bounds checking within one of the two types and not
 * limits that cover the sum of both, so extra work is needed.
 *
 * (Actually, very few processors seem to have much support for that routine
 * at all.)
 *
 * As a result, the way this is handled is that the can_use routine will allow
 * the first breakpoint of either type to be added.  Any further checking so
 * that only one of the two can be used is done later when GDB actually tries
 * to create the breakpoints, usually when the processor is restarted.
 *
 * We keep a local copy of the breakpoints list becasue the chain that GDB
 * keeps isn't exported globally.
 *
 * While things could be simplified with only one breakpoint present, setting
 * up more general routines allows for later expansion if newer versions of
 * the ColdFire chip support more breakpoints at once.
 *
 * Also, the ColdFire supports things like multi-level triggers and triggers
 * based on the data bus instead of just the address bus.  The GDB commands
 * don't allow for access to this, but much of it isn't necessary anyway,
 * as GDB has its own method of handling 'breakpoint conditions' that is
 * sufficient for most tasks.
 */

#define TDR_TRC_DDATA 0x00000000
#define TDR_TRC_HALT  0x40000000
#define TDR_TRC_DINT  0x80000000
#define TDR_L2_EBL    0x20000000
#define TDR_L2_ALL    0x1FFF0000
#define TDR_L2_EDLW   0x10000000
#define TDR_L2_EDWL   0x08000000
#define TDR_L2_EDWU   0x04000000
#define TDR_L2_EDLL   0x02000000
#define TDR_L2_EDLM   0x01000000
#define TDR_L2_EDUM   0x00800000
#define TDR_L2_EDUU   0x00400000
#define TDR_L2_DI     0x00200000
#define TDR_L2_EAI    0x00100000
#define TDR_L2_EAR    0x00080000
#define TDR_L2_EAL    0x00040000
#define TDR_L2_EPC    0x00020000
#define TDR_L2_PCI    0x00010000
#define TDR_L1_EBL    0x00002000
#define TDR_L1_ALL    0x00001FFF
#define TDR_L1_EDLW   0x00001000
#define TDR_L1_EDWL   0x00000800
#define TDR_L1_EDWU   0x00000400
#define TDR_L1_EDLL   0x00000200
#define TDR_L1_EDLM   0x00000100
#define TDR_L1_EDUM   0x00000080
#define TDR_L1_EDUU   0x00000040
#define TDR_L1_DI     0x00000020
#define TDR_L1_EAI    0x00000010
#define TDR_L1_EAR    0x00000008
#define TDR_L1_EAL    0x00000004
#define TDR_L1_EPC    0x00000002
#define TDR_L1_PCI    0x00000001

/*
 * The type is coded as follows:
 *    0 = software breakpoint
 *    1 = hardware breakpoint
 *    2 = write watchpoint
 *    3 = read watchpoint
 *    4 = access watchpoint
 */
#define M68K_BDM_WP_TYPE_BREAK  ('0')
#define M68K_BDM_WP_TYPE_HBREAK ('1')
#define M68K_BDM_WP_TYPE_WRITE  ('2')
#define M68K_BDM_WP_TYPE_READ   ('3')
#define M68K_BDM_WP_TYPE_ACCESS ('4')
#define M68K_BDM_WP_TYPE_EXEC   ('5')

struct m68k_bdm_watch  {
  char      type;
  CORE_ADDR addr;
  int       len;
};

struct m68k_bdm_break  {
  CORE_ADDR     addr;
  int           len;
  unsigned char code[M68K_BDM_BREAKPOINT_SIZE_MAX];
};

/*
 * We can have a variable number of hardware watch and break
 * points.
 */
static struct m68k_bdm_watch* m68k_bdm_hwatchpoints;
static int m68k_bdm_hbreakpoint_max;
static struct m68k_bdm_watch* m68k_bdm_hbreakpoints;
static int m68k_bdm_hwatchpoint_max;
static int m68k_bdm_hit_watchpoint;

/*
 * An array that increases in size as more break points are added. It
 * does not decrease in size.
 */
#define M68K_BDM_BREAKPOINT_BLOCK_SIZE (50)
static struct m68k_bdm_break *m68k_bdm_breakpoints;
static int m68k_bdm_num_breakpoints;

static int
m68k_bdm_init_watchpoints(void)
{
  /*
   * Return 0 if not supported as an error will abort this program.
   */
  if ( (m68k_bdm_cpu_family != BDM_COLDFIRE) && (m68k_bdm_cpu_family != BDM_COLDFIRE_V1) )
    return 0;

  m68k_bdm_hwatchpoint_max = m68k_bdm_reg_map[m68k_bdm_cpu_type].watchpoints;
  m68k_bdm_hbreakpoint_max = m68k_bdm_reg_map[m68k_bdm_cpu_type].breakpoints;
  
  /*
   * Nothing defined is an error for a Coldfire.
   */
  if ((m68k_bdm_hwatchpoint_max == 0) && (m68k_bdm_hbreakpoint_max == 0))
    return -1;

  if (bdmWriteSystemRegister (BDM_REG_TDR, TDR_TRC_HALT) < 0) {
    bdmClose();
    m68k_bdm_report_error ();
    return -1;
  }

  m68k_bdm_hwatchpoints = calloc (m68k_bdm_hwatchpoint_max,
                                 sizeof (struct m68k_bdm_watch));

  if (!m68k_bdm_hwatchpoints)
    return -1;
  
  m68k_bdm_hbreakpoints = calloc (m68k_bdm_hbreakpoint_max,
                                  sizeof (struct m68k_bdm_watch));

  if (!m68k_bdm_hbreakpoints) {
    free (m68k_bdm_hwatchpoints);
    return -1;
  }
  
  return 0;
}

static int
m68k_bdm_get_pbr_register (int index)
{
  char pbr[6];

  if (index == 0)
    strcpy (pbr, "pbr");
  else
    sprintf (pbr, "pbr%d", index);

  return m68k_bdm_register_by_name (pbr);
}
  
static int
m68k_bdm_get_ablr_register (int index)
{
  char ablr[6];

  if (index == 0)
    strcpy (ablr, "ablr");
  else
    sprintf (ablr, "ablr%d", index);

  return m68k_bdm_register_by_name (ablr);
}
  
static int
m68k_bdm_get_abhr_register (int index)
{
  char abhr[6];

  if (index == 0)
    strcpy (abhr, "abhr");
  else
    sprintf (abhr, "abhr%d", index);

  return m68k_bdm_register_by_name (abhr);
}
  
static int
m68k_bdm_find_hbreakpoint (char type, CORE_ADDR addr, int len)
{
  int i;
  for (i = 0; i < m68k_bdm_hbreakpoint_max; i++) {
    if (m68k_bdm_hbreakpoints[i].type == type &&
        m68k_bdm_hbreakpoints[i].addr == addr &&
        m68k_bdm_hbreakpoints[i].len == len)  {
      return i;
    }
  }
  return -1;
}

static int
m68k_bdm_find_hwatchpoint (char type, CORE_ADDR addr, int len)
{
  int i;
  for (i = 0; i < m68k_bdm_hwatchpoint_max; i++) {
    if (m68k_bdm_hwatchpoints[i].type == type &&
        m68k_bdm_hwatchpoints[i].addr == addr &&
        m68k_bdm_hwatchpoints[i].len == len)  {
      return i;
    }
  }
  return -1;
}

static int
m68k_bdm_hbreakpoint_active ()
{
  int i;
  int c;
  for (c = 0, i = 0; i < m68k_bdm_hbreakpoint_max; i++)
    if (m68k_bdm_hbreakpoints[i].len)
      c++;
  return c;
}

#if NOTUSED
static int
m68k_bdm_hwatchpoint_active ()
{
  int i;
  int c;
  for (c = 0, i = 0; i < m68k_bdm_hwatchpoint_max; i++)
    if (m68k_bdm_hwatchpoints[i].len)
      c++;
  return c;
}
#endif

static void
m68k_bdm_grow_breakpoints (int count)
{
  struct m68k_bdm_break *new_breakpoints;
  new_breakpoints = calloc(m68k_bdm_num_breakpoints + count,
                           sizeof (struct m68k_bdm_break));
  if (!new_breakpoints)
    fatal("no memory for breakpoints");
  if (m68k_bdm_breakpoints) {
    memcpy (new_breakpoints, m68k_bdm_breakpoints,
            m68k_bdm_num_breakpoints * sizeof (struct m68k_bdm_break));
    free (m68k_bdm_breakpoints);
  }
  m68k_bdm_breakpoints = new_breakpoints;
  m68k_bdm_num_breakpoints += count;
}

static int
m68k_bdm_insert_breakpoint (char type, CORE_ADDR addr, int len)
{
  unsigned long tdr;
  int           pbr;
  int           hb;

  if (!m68k_bdm_breakpoints_hard && (type == M68K_BDM_WP_TYPE_BREAK)) {
    int next_free = -1;

    if (len != m68k_bdm_breakpoint_size) {
      warning ("m68k-bdm: invalid breakpoint size");
    }

    if (m68k_bdm_debug_level)
      printf_filtered ("m68k-bdm: insert breakpoint @0x%08lx\n",
                       (unsigned long) addr);

    if (m68k_bdm_breakpoints) {
      /*
       * Do we already have a breakpoint set at this address ?
       * Which is the next free bp slot ?
       */

      int bp;
      for (bp = 0; bp < m68k_bdm_num_breakpoints; bp++) {
        if (m68k_bdm_breakpoints[bp].len) {
          if (m68k_bdm_breakpoints[bp].addr == addr)
            return 0;
        }
        else if (next_free < 0)
          next_free = bp;
      }
    }
    
    /*
     * Are all the slots free ?
     */
    if (next_free < 0) {
      next_free = m68k_bdm_num_breakpoints;
      m68k_bdm_grow_breakpoints (M68K_BDM_BREAKPOINT_BLOCK_SIZE);
    }

    m68k_bdm_breakpoints[next_free].addr = addr;
    m68k_bdm_breakpoints[next_free].len = len;

    if (bdmReadMemory (addr,
                       m68k_bdm_breakpoints[next_free].code,
                       m68k_bdm_breakpoint_size) < 0) {
      m68k_bdm_report_error ();
      return 1;
    }

    if (bdmWriteMemory (addr,
                        m68k_bdm_breakpoint_code,
                        m68k_bdm_breakpoint_size) < 0) {
      m68k_bdm_report_error ();
      return 1;
    }
    return 0;
  }

  /*
   * Hardware breakpoints.
   */
  
  if ( (m68k_bdm_cpu_family != BDM_COLDFIRE) && (m68k_bdm_cpu_family != BDM_COLDFIRE_V1) )
    return 1;

  hb = m68k_bdm_find_hbreakpoint (M68K_BDM_WP_TYPE_HBREAK, addr, 2);
  
  if (hb >= 0)
    return 0;

  for (hb = 0; hb < m68k_bdm_hbreakpoint_max; hb++)
    if (!m68k_bdm_hbreakpoints[hb].len)
      break;

  if (hb == m68k_bdm_hbreakpoint_max)
    return -1;
    
  m68k_bdm_hbreakpoints[hb].type = M68K_BDM_WP_TYPE_HBREAK;
  m68k_bdm_hbreakpoints[hb].addr = addr;
  m68k_bdm_hbreakpoints[hb].len = 2;

  if (bdmReadSystemRegister (BDM_REG_TDR, &tdr) < 0)
    m68k_bdm_report_error ();
    
  tdr |= TDR_L1_EBL | TDR_L1_EPC;

  /*
   * If the BDM version is C then each PBR register has a
   * valid bit (bit 0). If not the first PBR is controlled
   * by the TDR bits and any more if they exist have valid
   * bits.
   */
  
  if ((m68k_bdm_cf_debug_ver == M68K_BDM_VER_C) || (hb > 0))
    addr |= 1;

  pbr = m68k_bdm_get_pbr_register (hb);

  if (bdmWriteDebugRegister (pbr, addr) < 0)
    m68k_bdm_report_error ();
    
  if (bdmWriteSystemRegister (BDM_REG_PBMR, 0) < 0)
    m68k_bdm_report_error ();
  if (bdmWriteSystemRegister (BDM_REG_TDR, tdr) < 0)
    m68k_bdm_report_error ();
    
  if (m68k_bdm_debug_level)
    printf_filtered ("m68k-bdm: insert hbreakpoint:%d: @0x%08lx\n",
                     hb, (unsigned long) addr);
  
  return 0;
}

static int
m68k_bdm_remove_breakpoint (char type, CORE_ADDR addr, int len)
{
  unsigned long tdr;
  int           pbr;
  int           hb;
  
  if (!m68k_bdm_breakpoints_hard && (type == M68K_BDM_WP_TYPE_BREAK)) {
    int bp;
    for (bp = 0; bp < m68k_bdm_num_breakpoints; bp++) {
      if (m68k_bdm_breakpoints[bp].len) {
        if (m68k_bdm_breakpoints[bp].addr == addr) {
          if (m68k_bdm_debug_level)
            printf_filtered ("m68k-bdm: remove breakpoint @0x%08lx\n",
                             (unsigned long) addr);
          m68k_bdm_breakpoints[bp].addr = 0;
          m68k_bdm_breakpoints[bp].len = 0;
          if (bdmWriteMemory (addr,
                              m68k_bdm_breakpoints[bp].code,
                              m68k_bdm_breakpoint_size) < 0) {
            m68k_bdm_report_error ();
            return 1;
          }
          break;
        }
      }
    }
    return 0;
  }
  
  /*
   * Hardware breakpoints.
   */
  
  if ( (m68k_bdm_cpu_family != BDM_COLDFIRE) && (m68k_bdm_cpu_family != BDM_COLDFIRE_V1) )
    return 1;

  hb = m68k_bdm_find_hbreakpoint (M68K_BDM_WP_TYPE_HBREAK, addr, 2);
  
  if (hb < 0)
    return -1;

  if (bdmReadSystemRegister (BDM_REG_TDR, &tdr) < 0)
    m68k_bdm_report_error ();

  /*
   * If the BDM version is not C and there is more than one PBR
   * register then we need to move the higher PBR registers down
   * from PBR registers with the valid bit to PBR(0) or turn
   * off the trigger in the TDR register.
   */

  pbr = m68k_bdm_get_pbr_register (hb);

  if ((hb == 0) &&
      (m68k_bdm_cf_debug_ver != M68K_BDM_VER_C) &&
      (m68k_bdm_hbreakpoint_max > 1)) {
    /*
     * See if any other breakpoints are set. If one is move to
     * slot 0.
     */
    int bp;
    for (bp = 1; bp < m68k_bdm_hbreakpoint_max; bp++) {
      if (m68k_bdm_hbreakpoints[bp].len) {
        if (bdmWriteDebugRegister (pbr, m68k_bdm_hbreakpoints[bp].addr) < 0)
          m68k_bdm_report_error ();
        pbr = m68k_bdm_get_pbr_register (bp);
        m68k_bdm_hbreakpoints[0] = m68k_bdm_hbreakpoints[bp];
        m68k_bdm_hbreakpoints[bp].len = 0;
        break;
      }
    }

    if (bp == m68k_bdm_hbreakpoint_max)
       m68k_bdm_hbreakpoints[hb].len = 0;
  }
  else
    m68k_bdm_hbreakpoints[hb].len = 0;
  
  /*
   * Assume all PBR registers have a valid bit.
   */
  if (bdmWriteDebugRegister (pbr, 0) < 0)
    m68k_bdm_report_error ();

  if (!m68k_bdm_hbreakpoint_active ())
    tdr &= ~TDR_L1_EPC;

  if ((tdr & TDR_L1_ALL) == 0)
    tdr &= ~TDR_L1_EBL;
  
  if (bdmWriteSystemRegister (BDM_REG_TDR, tdr) < 0)
    m68k_bdm_report_error ();

  if (m68k_bdm_debug_level)
    printf_filtered ("m68k-bdm: remove hbreakpoint:%d: @0x%08lx\n",
                     hb, (unsigned long) addr);
  
  return 0;
}

#define AATR_READONLY  0x7F85
#define AATR_WRITEONLY 0x7F05
#define AATR_READWRITE 0xFF05

static int
m68k_bdm_insert_watchpoint (char type, CORE_ADDR addr, int len)
{
  int           tdr;
  unsigned long tdr_value;
  int           ablr;
  int           abhr;
  int           hw;

  if ( (m68k_bdm_cpu_family != BDM_COLDFIRE) && (m68k_bdm_cpu_family != BDM_COLDFIRE_V1) )
    return 1;

  hw = m68k_bdm_find_hwatchpoint (type, addr, len);
  
  if (hw >= 0)
    return 0;

  for (hw = 0; hw < m68k_bdm_hwatchpoint_max; hw++)
    if (!m68k_bdm_hwatchpoints[hw].len)
      break;

  if (hw == m68k_bdm_hwatchpoint_max)
    return -1;
    
  m68k_bdm_hwatchpoints[hw].type = type;
  m68k_bdm_hwatchpoints[hw].addr = addr;
  m68k_bdm_hwatchpoints[hw].len = len;

  /*
   * If the BDM version is C then we have 2 TDR registers
   */
  if ((m68k_bdm_cf_debug_ver == M68K_BDM_VER_C) && (hw > 0))
    tdr = m68k_bdm_register_by_name ("xtdr");
  else
    tdr = m68k_bdm_register_by_name ("tdr");
  
  if (bdmReadDebugRegister (tdr, &tdr_value) < 0)
    m68k_bdm_report_error ();

  tdr_value |= TDR_L1_EBL | TDR_L1_EAR;

  ablr = m68k_bdm_get_ablr_register (hw);
  abhr = m68k_bdm_get_abhr_register (hw);
  
  if (bdmWriteDebugRegister (ablr, addr) < 0)
    m68k_bdm_report_error ();
  if (bdmWriteDebugRegister (abhr, addr + len - 1) < 0)
    m68k_bdm_report_error ();
    
  if (type == M68K_BDM_WP_TYPE_READ) {
    if (bdmWriteSystemRegister (BDM_REG_AATR, AATR_READONLY) < 0)
      m68k_bdm_report_error ();
    if (m68k_bdm_debug_level)
      printf_filtered ("m68k-bdm: insert read hwatchpoint:%d: @0x%08lx-0x%08lx\n",
                       hw,
                       (unsigned long) addr, (unsigned long) addr + len - 1);
  }
  else if (type == M68K_BDM_WP_TYPE_WRITE) {
    if (bdmWriteSystemRegister (BDM_REG_AATR, AATR_WRITEONLY) < 0)
      m68k_bdm_report_error ();
    if (m68k_bdm_debug_level)
      printf_filtered ("m68k-bdm: insert write hwatchpoint:%d @0x%08lx-0x%08lx\n",
                       hw,
                       (long unsigned int) addr, (unsigned long) addr + len - 1);
  }
  else {
    if (bdmWriteSystemRegister (BDM_REG_AATR, AATR_READWRITE) < 0)
      m68k_bdm_report_error ();
    if (m68k_bdm_debug_level)
      printf_filtered ("m68k-bdm: insert access hwatchpoint:%d @0x%08lx-0x%08lx\n",
                       hw,
                       (unsigned long) addr, (unsigned long) addr + len - 1);
  }

  if (bdmWriteDebugRegister (tdr, tdr_value) < 0)
    m68k_bdm_report_error ();

  return 0;
}

static int
m68k_bdm_remove_watchpoint (char type, CORE_ADDR addr, int len)
{
  int           tdr;
  unsigned long tdr_value;
  int           hw;
  
  if ( (m68k_bdm_cpu_family != BDM_COLDFIRE) && (m68k_bdm_cpu_family != BDM_COLDFIRE_V1) )
    return -1;

  hw = m68k_bdm_find_hwatchpoint (type, addr, len);

  if (hw < 0)
    return -1;

  m68k_bdm_hwatchpoints[hw].len = 0;
  
  /*
   * If the BDM version is C then we have 2 TDR registers
   */
  if ((m68k_bdm_cf_debug_ver == M68K_BDM_VER_C) && (hw > 0))
    tdr = m68k_bdm_register_by_name ("xtdr");
  else
    tdr = m68k_bdm_register_by_name ("tdr");

  if (bdmReadDebugRegister (tdr, &tdr_value) < 0)
    m68k_bdm_report_error ();

  tdr &= ~TDR_L1_EAR;

  if ((tdr & TDR_L1_ALL) == 0)
    tdr &= ~TDR_L1_EBL;

  if (bdmWriteDebugRegister (tdr, tdr_value) < 0)
      m68k_bdm_report_error ();

  if (m68k_bdm_debug_level)
    printf_filtered ("m68k-bdm: remove %s watchpoint:%d: @0x%08lx-0x%08lx\n",
                     (type == M68K_BDM_WP_TYPE_READ) ? "read" :
                     ((type == M68K_BDM_WP_TYPE_WRITE) ? "write" : "access"),
                     hw,
                     (unsigned long) addr, (unsigned long) addr + len - 1);

  return 0;
}

static int
m68k_bdm_insert_point (char type, CORE_ADDR addr, int len)
{
  if (m68k_bdm_debug_level)
    printf_filtered ("m68k-bdm: inserting type:%c @0x%08lx %i\n",
                     type, (unsigned long) addr, len);
  if ((type == M68K_BDM_WP_TYPE_BREAK) || (type == M68K_BDM_WP_TYPE_HBREAK))
    return m68k_bdm_insert_breakpoint (type, addr, len);
  return m68k_bdm_insert_watchpoint (type, addr, len);
}

static int
m68k_bdm_remove_point (char type, CORE_ADDR addr, int len)
{
  if (m68k_bdm_debug_level)
    printf_filtered ("m68k-bdm: removing type:%c @0x%08lx %i\n",
                     type, (unsigned long) addr, len);
  if ((type == M68K_BDM_WP_TYPE_BREAK) || (type == M68K_BDM_WP_TYPE_HBREAK))
    return m68k_bdm_remove_breakpoint (type, addr, len);
  return m68k_bdm_remove_watchpoint (type, addr, len);
}

static int
m68k_bdm_stopped_by_watchpoint (void)
{
  return m68k_bdm_hit_watchpoint;
}

static CORE_ADDR
m68k_bdm_stopped_data_address (void)
{
  unsigned long tdr;
  unsigned long ablr;

  if ( (m68k_bdm_cpu_family != BDM_COLDFIRE) && (m68k_bdm_cpu_family != BDM_COLDFIRE_V1) )
    return 0;

  if (bdmReadSystemRegister (BDM_REG_TDR, &tdr) < 0)
    m68k_bdm_report_error ();
  if (bdmReadSystemRegister (BDM_REG_ABLR, &ablr) < 0)
    m68k_bdm_report_error ();

  if (tdr & TDR_L1_EAR) {
    return (CORE_ADDR) ablr;
  }
  return 0;
}

#ifdef SYSCALL_TRAP
/* Immediately after a function call, return the saved pc before the frame
   is setup.  For uCLinux which uses a TRAP #0 for a system call we need to
   read the first long word of the stack to see if the stack frame format is
   type 4 and the vector is 32 (0x4080), and the opcode at the PC is
   "move.w #0x2700,sr". If it is then get the second long from the stack. */

CORE_ADDR
m68k_bdm_saved_pc_after_call (struct frame_info *frame)
{
  unsigned int op;
  unsigned int eframe;
  int          sp;

  if(!IS_BDM)
    return gdbarch_saved_pc_after_call (current_gdbarch, frame);

  sp = read_register (SP_REGNUM);
  eframe = read_memory_integer (sp, 2);
  op = read_memory_integer (frame->pc, 4);

  /*
   * This test could break if some changes the syste call.
   */

  if (eframe == 0x4080 && op == 0x46fc2700)
    return read_memory_integer (sp + 4, 4);
  else
    return read_memory_integer (sp, 4);
}
#endif /* SYSCALL_TRAP */

/*
 * Short pause
 */
static void
m68k_bdm_nap (int microseconds)
{
#if defined (__MINGW32__)
  Sleep (microseconds / 1000);
#else
  struct timeval tv;
  tv.tv_sec = microseconds / 1000000;
  tv.tv_usec = microseconds % 1000000;
  select (0, NULL, NULL, NULL, &tv);
#endif
}

/*
 * Return interface status
 */
static int
m68k_bdm_get_status (void)
{
  int status;

  if ((status = bdmStatus ()) < 0)
    m68k_bdm_report_error ();
  return status;
}

/*
 * release chip: reset and disable bdm mode
 */
#if 0
static void
m68k_bdm_release_chip (void)
{
  m68k_bdm_have_atemp = 0;
  m68k_bdm_ptid = null_ptid;
  if (bdmRelease () < 0)
    m68k_bdm_report_error ();
}
#endif

/*
 * stop chip
 */
static void
m68k_bdm_stop_chip (void)
{
  if (m68k_bdm_debug_level)
    printf_filtered ("m68k-bdm: stop chip\n");
  if (bdmStop () < 0)
    m68k_bdm_report_error ();
}

/*
 * Allow chip to resume execution
 */
static void
m68k_bdm_go (void)
{
  m68k_bdm_have_atemp = 0;
  if (bdmGo () < 0)
    m68k_bdm_report_error ();
}

/*
 * Reset chip, enter BDM mode
 */
static void
m68k_bdm_reset (void)
{
  m68k_bdm_have_atemp = 0;
  m68k_bdm_ptid = null_ptid;
  if (bdmReset () < 0)
    m68k_bdm_report_error ();
  m68k_bdm_nap (M68K_BDM_TIME_TO_COME_UP);
}

/*
 * step cpu32 chip: execute a single instruction
 * This is complicated by the presence of interrupts.
 * Consider the following sequence of events:
 *    - User attempts to `continue' from a breakpoint.
 *    - Gdb calls bdm_step_chip to single-step the instruction that
 *      had been replaced by the BGND instruction.
 *    - The target processor executes the instruction and stops.
 *    - GDB replaces the instruction with a BGND instruction to
 *      force a breakpoint the next time the instruction is hit.
 *    - GDB calls bdm_go and the target resumes execution.
 * This all seems fine, but now consider what happens when a interrupt
 * is pending:
 *    - User attempts to `continue' from a breakpoint.
 *    - Gdb calls bdm_step_chip to single-step the instruction that
 *      had been replaced by the BGND instruction.
 *    - The target processor does not execute the replaced instruction,
 *      but rather executes the first instruction of the interrupt
 *      service routine, then stops.
 *    - GDB replaces the instruction with a BGND instruction to
 *      force a breakpoint the next time the instruction is hit.
 *    - GDB calls bdm_go and the target resumes execution.
 *    - The target finishes off the interrupt, and upon returing from
 *      the interrupt generates another breakpoint!
 * The solution is simple -- disable interrupts when single stepping.
 * The problem then becomes the handling of instructions which involve
 * the program status word!
 */
static void
m68k_bdm_step_cpu32_chip (void)
{
  unsigned long pc;
  unsigned short instruction;
  unsigned short immediate;
  unsigned long d7;
  unsigned long sr;
  unsigned long nsr;
  enum {
    op_other,
    op_ANDIsr,
    op_EORIsr,
    op_ORIsr,
    op_TOsr,
    op_FROMsr,
    op_FROMsrTOd7
  } op;

  /*
   * Get the existing status register
   */
  if (bdmReadSystemRegister (BDM_REG_SR, &sr) < 0)
    m68k_bdm_report_error ();

  /*
   * Read the instuction about to be executed
   */
  if ((bdmReadSystemRegister (BDM_REG_RPC, &pc) < 0)
      || (bdmReadWord (pc, &instruction) < 0))
    m68k_bdm_report_error ();

  /*
   * See what operation is to be performed
   */
  if (instruction == 0x027C)
    op = op_ANDIsr;
  else if (instruction == 0x0A7C)
    op = op_EORIsr;
  else if (instruction == 0x007C)
    op = op_ORIsr;
  else if (instruction == 0x40C7)
    op = op_FROMsrTOd7;
  else if ((instruction & 0xFFC0) == 0x40C0)
    op = op_FROMsr;
  else if ((instruction & 0xFFC0) == 0x46C0)
    op = op_TOsr;
  else
    op = op_other;

  /*
   * Set things up for the single-step operation
   */
  switch (op) {
    case op_FROMsr:
    case op_FROMsrTOd7:
      /*
       * It's storing the SR somewhere.
       * Store the SR in D7 and change the instruction
       * to save D7.  This fails if the addressing mode
       * is one of the esoteric modes that uses D7 as
       * and index register, but we'll just have to hope
       * that doesn't happen too often.
       */
      if ((bdmReadRegister (7, &d7) < 0)
          || (bdmWriteRegister (7, sr) < 0)
          || (bdmWriteWord (pc, 0x3007 |
                            ((instruction & 0x38) << 3) |
                            ((instruction & 0x07) << 9)) < 0))
        m68k_bdm_report_error ();
      break;

    case op_ANDIsr:
    case op_EORIsr:
    case op_ORIsr:
      /*
       * It's an immediate operation to the SR -- pick up the value
       */
      if (bdmReadWord (pc+2, &immediate) < 0)
        m68k_bdm_report_error ();
      break;

    case op_TOsr:
    case op_other:
      break;
  }

  /*
   * Ensure the step is done with interrupts disabled
   */
  if (bdmWriteSystemRegister (BDM_REG_SR, sr | 0x0700) < 0)
    m68k_bdm_report_error ();

  /*
   * Do the single-step
   */
  if (bdmStep () < 0)
    m68k_bdm_report_error ();

  /*
   * Get the ATEMP register since the following operations may
   * modify it.
   */
  if (bdmReadSystemRegister (BDM_REG_ATEMP, &m68k_bdm_atemp) < 0)
    m68k_bdm_report_error ();
  m68k_bdm_have_atemp = 1;

  /*
   * Clean things up
   */
  switch (op) {
    case op_FROMsr:
      if ((bdmWriteRegister (7, d7) < 0)
          || (bdmWriteWord (pc, instruction) < 0)
          || (bdmWriteSystemRegister (BDM_REG_SR, sr) < 0))
        m68k_bdm_report_error ();
      break;

    case op_FROMsrTOd7:
      if ((bdmReadRegister (7, &d7) < 0)
          || (bdmWriteRegister (7, (d7 & ~0xFFFF) | (sr & 0xFFFF)) < 0)
          || (bdmWriteSystemRegister (BDM_REG_SR, sr) < 0))
        m68k_bdm_report_error ();
      break;

    case op_ANDIsr:
      if (bdmWriteSystemRegister (BDM_REG_SR, sr & immediate) < 0)
        m68k_bdm_report_error ();
      break;

    case op_EORIsr:
      if (bdmWriteSystemRegister (BDM_REG_SR, sr ^ immediate) < 0)
        m68k_bdm_report_error ();
      break;

    case op_ORIsr:
      if (bdmWriteSystemRegister (BDM_REG_SR, sr | immediate) < 0)
        m68k_bdm_report_error ();
      break;

    case op_TOsr:
      break;

    case op_other:
      if ((bdmReadSystemRegister (BDM_REG_SR, &nsr) < 0) ||
          (bdmWriteSystemRegister (BDM_REG_SR,
                                   (nsr & ~0x0700) | (sr & 0x0700)) < 0))
        m68k_bdm_report_error ();
      break;
  }
}

static void
m68k_bdm_step_chip (void)
{
  /*
   * The cpu32 is harder to step than the Coldfire.
   */
  if (m68k_bdm_cpu_family == BDM_CPU32) {
    m68k_bdm_step_cpu32_chip();
    return;
  }

  /*
   * Do the single-step
   */
  if (bdmStep () < 0)
    m68k_bdm_report_error ();
}

/* Make a copy of the string at PTR with SIZE characters
   (and add a null character at the end in the copy).
   Uses malloc to get the space.  Returns the address of the copy.  */

static char *
savestring (const char *ptr, size_t size)
{
  char *p = (char *) malloc (size + 1);
  if (!p)
    error ("m68k-bdm: no memory for string");
  memcpy (p, ptr, size);
  p[size] = 0;
  return p;
}
static void m68k_bdm_close (void);

/* Start an inferior process and returns its pid.
   ALLARGS is a vector of program-name and args. */
static int
m68k_bdm_create_inferior (char *program, char *argv[])
{
  char*         device = NULL;
  char*         p;
  int           arg = 0;
  unsigned int  version = 0;
  unsigned long csr = 0;
  unsigned long csr2 = 0;
  int           delay = -1;
  int           driver_debug_level = 0;
  int           debug_level = 0;
 
  warning_prefix = "m68k-bdm-gdbserver";

  while (argv[arg]) {
    if (argv[arg][0] == '-') {
      switch (argv[arg][1])
      {
        case 'B':
          m68k_bdm_breakpoints_hard = 1;
          break;
        case 'D':
          driver_debug_level++;
          break;
        case 'd':
          debug_level++;
          break;
        case 't':
          arg++;
          if (!argv[arg])
            fatal ("m68k-bdm: no delay timeout argument found");
          delay = strtoul (argv[arg], 0, 0);
          break;
        case 'v':
          m68k_bdm_debug_level++;
          break;
        case 'V':
          m68k_bdm_version ();
          break;
        case 'h':
          m68k_bdm_help ();
          break;
        default:
          fatal ("m68k-bdm: invalid option: %s, try -h", argv[arg]);
          break;
      }
    }
    else {
      if (device)
        warning ("m68k-bdm: device alread set to %s, ignoring %s\n",
                 device, argv[arg]);
      else
        device = argv[arg];
    }
    arg++;
  }

  if (!device)
    error ("no device path found");
  
  /*
   * Open a connection the target via bdm
   * name is the devicename of bdm and the filename to be used
   * used for communication.
   */
  
  if (m68k_bdm_debug_level)
    printf_filtered ("m68k-bdm: create inferior called\n");

  if (bdmIsOpen ()) {
    if (m68k_bdm_debug_level)
      printf_filtered ("m68k-bdm: bdm driver is already open; closing.\n");
    bdmClose ();
  }

  m68k_bdm_ptid = null_ptid;

  /*
   * Find the first whitespace character after device and chop it off
   */
  for (p = device; (*p != '\0') && (!isspace (*p)); p++);
  if ((*p == '\0') && (p == device))
    error ("m68k-bdm: the name of the bdm port device is missing.");
  m68k_bdm_dev_name = savestring (device, p - device);

  if (m68k_bdm_debug_level)
    printf_filtered ("m68k-bdm: opening: %s\n", m68k_bdm_dev_name);
  
  if (debug_level > 0) {
    if (m68k_bdm_debug_level)
      printf_filtered ("m68k-bdm: set BDM lib debug level: %d\n",
                       debug_level);
    bdmSetDebugFlag (debug_level);
  }
  
  if (bdmOpen (m68k_bdm_dev_name) < 0)
    m68k_bdm_report_error ();

  if (driver_debug_level > 0) {
    if (m68k_bdm_debug_level)
      printf_filtered ("m68k-bdm: set driver debug level: %d\n",
                       driver_debug_level);
    bdmSetDriverDebugFlag (driver_debug_level);
  }
  

  if (delay > 0) {
    if (bdmSetDelay (delay) < 0)
      m68k_bdm_report_error ();
  }

  if (bdmStatus () & (BDM_TARGETPOWER | BDM_TARGETNC)) {
    bdmClose ();
    error ("m68k-bdm: target or cable problem");
  }

  /*
   * Ask the driver for it's version.
   * We are only interested in the major number when checking the
   * driver version number.
   */
  if (bdmGetDrvVersion (&version) < 0)
    m68k_bdm_report_error ();
  if ((version & 0xff00) != (BDM_DRV_VERSION & 0xff00)) {
    printf_filtered ("m68k-bdm: incorrect driver version, looking for %i.%i"\
                     " and found %i.%i\n",
                     BDM_DRV_VERSION >> 8, BDM_DRV_VERSION & 0xff,
                     version >> 8, version  & 0xff);
    bdmClose ();
    error ("m68k-bdm: can't run with incorrect BDM driver version");
  }

  /*
   * Get the processor type.
   */
  if (bdmGetProcessor (&m68k_bdm_cpu_family) < 0)
    m68k_bdm_report_error ();

  switch (m68k_bdm_cpu_family) {
    case BDM_CPU32:
      m68k_bdm_breakpoint_code = m68k_bdm_cpu32_breakpoint;
      m68k_bdm_breakpoint_size = sizeof m68k_bdm_cpu32_breakpoint;
      m68k_bdm_cpu_type = M68K_BDM_MARCH_CPU32;
      m68k_bdm_cpu_label = M68K_BDM_MARCH_CPU32_LABEL;
      break;

    case BDM_COLDFIRE:
    case BDM_COLDFIRE_V1:
      m68k_bdm_breakpoint_code = m68k_bdm_cf_breakpoint;
      m68k_bdm_breakpoint_size = sizeof m68k_bdm_cf_breakpoint;

      /*
       * Read the CSR register to determine the debug module
       * version.
       */
      if (bdmReadSystemRegister (BDM_REG_CSR, &csr) < 0)
        m68k_bdm_report_error ();
      m68k_bdm_cf_debug_ver = (csr >> 20) & 0x0f;
      if (bdmReadSystemRegister (BDM_REG_CSR2, &csr2) == 0)
	m68k_bdm_cf_debug_ver += (csr2 >> 16) & 0x0f;
      printf_filtered ("m68k-bdm: debug module version %" PRIdMAX "\n", m68k_bdm_cf_debug_ver);

      /*
       * If the processor is a version 0 read the PC and VBR
       * an if they can be read read the mbar. If that fails
       * we have a 5282.
       */
      if (m68k_bdm_cf_debug_ver == M68K_BDM_VER_A) {
        unsigned long junk;
        m68k_bdm_cpu_type = M68K_BDM_MARCH_CF5200;
        m68k_bdm_cpu_label = M68K_BDM_MARCH_CF5200_LABEL;
        if ((bdmReadSystemRegister (BDM_REG_RPC, &junk) == 0) &&
            (bdmReadSystemRegister (BDM_REG_VBR, &junk) == 0)) {
          if (bdmReadControlRegister (0xc0f, &junk) < 0) {
            /* XCSR on the 5235 */
            if (bdmReadDebugRegister (0x1, &junk) < 0) {
              m68k_bdm_cpu_type = M68K_BDM_MARCH_CF5282;
              m68k_bdm_cpu_label = M68K_BDM_MARCH_CF5282_LABEL;
              printf_filtered ("m68k-bdm: detected MCF5282\n");
            }
            else {
              m68k_bdm_cpu_type = M68K_BDM_MARCH_CF5235;
              m68k_bdm_cpu_label = M68K_BDM_MARCH_CF5235_LABEL;
              printf_filtered ("m68k-bdm: detected MCF5235\n");
            }
          }
          else {
            m68k_bdm_cpu_type = M68K_BDM_MARCH_CF5272;
            m68k_bdm_cpu_label = M68K_BDM_MARCH_CF5272_LABEL;
            printf_filtered ("m68k-bdm: detected V2 core\n");
          }
        }
      } else if (m68k_bdm_cf_debug_ver == M68K_BDM_VER_B_PLUS) {
        m68k_bdm_cpu_type = M68K_BDM_MARCH_CF52223;
        m68k_bdm_cpu_label = M68K_BDM_MARCH_CF52223_LABEL;
        printf_filtered ("m68k-bdm: detected MCF52223 (MCF52235)\n");
      } else if (m68k_bdm_cf_debug_ver == M68K_BDM_CFV1_VER_B_PLUS) {
	m68k_bdm_cpu_type = M68K_BDM_MARCH_CFV1;
        m68k_bdm_cpu_label = M68K_BDM_MARCH_CFV1_LABEL;
        printf_filtered ("m68k-bdm: detected CFv1 (CFV1)\n");
      } else if (m68k_bdm_cf_debug_ver == M68K_BDM_VER_B) {
        m68k_bdm_cpu_type = M68K_BDM_MARCH_CF5307;
        m68k_bdm_cpu_label = M68K_BDM_MARCH_CF5307_LABEL;
        printf_filtered ("m68k-bdm: detected MCF5307\n");
      } else if (m68k_bdm_cf_debug_ver == M68K_BDM_VER_C
             ||  m68k_bdm_cf_debug_ver == M68K_BDM_VER_D) {
        m68k_bdm_cpu_type = M68K_BDM_MARCH_CFV4E;
        m68k_bdm_cpu_label = M68K_BDM_MARCH_CFV4E_LABEL;
        printf_filtered ("m68k-bdm: detected V4e core\n");
      } else {
        bdmClose ();
        error ("m68k-bdm: unknown processor debug version.");
      }
      break;

    default:
      bdmClose ();
      error ("m68k-bdm: unknown processor type returned from the driver.");
  }

  printf_filtered ("m68k-bdm: architecture %s connected to %s\n",
                   m68k_bdm_cpu_label, m68k_bdm_dev_name);
  if ( (m68k_bdm_cpu_family == BDM_COLDFIRE) || (m68k_bdm_cpu_family == BDM_COLDFIRE_V1) )
  {
    char* cf_type = "5206(e)/5235/5272/5282";
    m68k_bdm_ptid = 0x5200;
    if (m68k_bdm_cf_debug_ver == M68K_BDM_VER_B) {
      cf_type = "5307/5407";
      m68k_bdm_ptid = 0x5300;
    }
    else if (m68k_bdm_cf_debug_ver == M68K_BDM_VER_B_PLUS) {
      cf_type = "52223/52235";
      m68k_bdm_ptid = 0x52223;
    } else if (m68k_bdm_cf_debug_ver == M68K_BDM_CFV1_VER_B_PLUS) {
      cf_type = "cfv1";
      m68k_bdm_ptid = 0x5100;
    }
    else if (m68k_bdm_cf_debug_ver == M68K_BDM_VER_C
          || m68k_bdm_cf_debug_ver == M68K_BDM_VER_D ) {
      cf_type = "V4e (547x/548x)";
      m68k_bdm_ptid = 0x5400;
    }
    printf_filtered ("m68k-bdm: Coldfire debug module version is %ld (%s)\n",
                     m68k_bdm_cf_debug_ver, cf_type);
  }
  else {
    m68k_bdm_ptid = 0x68300;
  }

  m68k_bdm_init_registers ();

  m68k_bdm_init_watchpoints ();

  set_breakpoint_data (m68k_bdm_breakpoint_code, m68k_bdm_breakpoint_size);

  add_thread (m68k_bdm_ptid, NULL, m68k_bdm_ptid);

  return m68k_bdm_ptid;
}

static int
m68k_bdm_attach (unsigned long pid)
{
  if (m68k_bdm_debug_level)
    printf_filtered ("m68k-bdm: attach\n");
  m68k_bdm_ptid = pid;
  return 0;
}

/*
 * Terminate current application and return to system prompt.
 * On a target, just let the program keep on running
 */
static void
m68k_bdm_kill (void)
{
  if (m68k_bdm_debug_level)
    printf_filtered ("m68k-bdm: kill: release on exit: %d\n",
                m68k_bdm_release_on_exit);
  if (m68k_bdm_release_on_exit) {
    if (m68k_bdm_get_status () & BDM_TARGETSTOPPED)
      m68k_bdm_go ();
  } else {
    m68k_bdm_stop_chip ();
    m68k_bdm_reset ();
  }
}

static void
m68k_bdm_close (void)
{
  if (m68k_bdm_debug_level)
    printf_filtered ("m68k-bdm: close\n");
  m68k_bdm_gdb_is_quitting = 1;
  m68k_bdm_have_atemp = 0;
  if (m68k_bdm_kill_on_exit)
    m68k_bdm_kill ();
  bdmClose ();
  m68k_bdm_ptid = null_ptid;
}

/*
 * m68k_bdm_detach -- Terminate the open connection to the remote debugger.
 * takes a program previously attached to and detaches it.
 * We better not have left any breakpoints
 * in the program or it'll die when it hits one.
 * Close the open connection to the remote debugger.
 * Use this when you want to detach and do something else
 * with your gdb.
 */
static int
m68k_bdm_detach (void)
{
  if (m68k_bdm_debug_level)
    printf_filtered ("m68k-bdm: detach\n");
  m68k_bdm_ptid = null_ptid;
  
  // When we detach we have to close the device, force the close
  m68k_bdm_close();
  return 0;
}

static void
m68k_bdm_join (void)
{
  if (m68k_bdm_debug_level)
    printf_filtered ("m68k-bdm: join\n");
}

static int
m68k_bdm_thread_alive (unsigned long tid)
{
  return tid == m68k_bdm_ptid;
}

/*
 * m68k_bdm_resume -- Tell the remote machine to resume.
 */
static void
m68k_bdm_resume (struct thread_resume *resume_info)
{
  if (m68k_bdm_debug_level)
    printf_filtered ("m68k-bdm: resume: thread:%ld leave stopped:%d step:%d\n",
                resume_info->thread, resume_info->leave_stopped,
                resume_info->step);

  /*
   * Invalidate the reigsters in the register cache.
   */
  regcache_invalidate ();
  
  if (resume_info->step)
    m68k_bdm_step_chip ();
  else
    m68k_bdm_go ();
}

/*
 * We have fallen into an exception supported by the runtime system.
 * by executing a `breakpoint' instruction.
 */
static unsigned char
m68k_bdm_analyze_exception (char* status)
{
  unsigned long  pc;
  unsigned char  opcode[20]; /* `big enough' */

  if (m68k_bdm_cpu_family == BDM_CPU32) {
    if (bdmReadSystemRegister (BDM_REG_PCC, &pc) < 0)
      m68k_bdm_report_error ();
  }
  else {
    if (bdmReadSystemRegister (BDM_REG_RPC, &pc) < 0)
      m68k_bdm_report_error ();
    if (pc)
      pc -= 2;
  }
  
  *status = TARGET_WAITKIND_STOPPED;

  /*
   * See if it was a `breakpoint' instruction
   */
  if (bdmReadMemory (pc, opcode, m68k_bdm_breakpoint_size) < 0)
    m68k_bdm_report_error ();
  if (memcmp (m68k_bdm_breakpoint_code, opcode, m68k_bdm_breakpoint_size) == 0) {
    if (bdmWriteSystemRegister (BDM_REG_RPC, pc) < 0)
      m68k_bdm_report_error ();
    return TARGET_SIGNAL_TRAP;
  }
  
  /*
   * FIXME: Why an illegal instruction signal ?
   */
  return TARGET_SIGNAL_ILL;
}

/*
 * Wait until the remote machine stops, then return the
 * signals that stopped us and the status flag. The flags
 * are documented on the GDB manual for the remote protocol.
 *
 * 'S' : AA is the returned signal number.
 *
 * 'T' : AA is the returned signal number. The server
 *       will attempt to see if the stop was due to a
 *       watch point.
 *
 * 'W' : AA is the returned exit code for the process that has
 *       exitted.
 *
 * 'X' : AA is the returned signal number for the terminated
 *       process.
 */

static unsigned char
m68k_bdm_wait (char* status)
{
  unsigned char signal;
  int           bdm_stat;
  
  m68k_bdm_hit_watchpoint = 0;
  *status = TARGET_WAITKIND_EXITED;
  signal = TARGET_SIGNAL_0;

  /*
   * A work around a problem with the 5206e processor. Not sure
   * what the issue is. It could be the processor.
   */
  if (m68k_bdm_cpu_family == BDM_COLDFIRE && m68k_bdm_cf_debug_ver == 0)
    m68k_bdm_nap (10000);

  /*
   * Wait here till the target requires service
   */
  while ((bdm_stat = m68k_bdm_get_status ()) == 0) {
    check_remote_input_interrupt_request ();
    m68k_bdm_nap (20000);
  }

  /*
   * Determine why the target stopped
   */
  switch (bdm_stat) {
    case BDM_TARGETNC:
      *status = TARGET_WAITKIND_EXITED;
      signal = TARGET_SIGNAL_LOST;
      break;
    case BDM_TARGETPOWER:
      *status = TARGET_WAITKIND_EXITED;
      signal = TARGET_SIGNAL_PWR;
      break;
    case BDM_TARGETRESET:
      *status = TARGET_WAITKIND_STOPPED;
      signal = TARGET_SIGNAL_KILL;
      break;
    case BDM_TARGETSTOPPED:
    case BDM_TARGETHALT:
    case BDM_TARGETSTOPPED | BDM_TARGETHALT:
      if (m68k_bdm_cpu_family == BDM_CPU32) {
        if (!m68k_bdm_have_atemp
            && (bdmReadSystemRegister (BDM_REG_ATEMP, &m68k_bdm_atemp) < 0))
          m68k_bdm_report_error ();
        switch (m68k_bdm_atemp & 0xffff) {
          case 0xffff:  /* double bus fault */
            *status = TARGET_WAITKIND_STOPPED;
            signal = TARGET_SIGNAL_BUS;
            break;
          case 0x0:  /* HW bkpt */
            *status = TARGET_WAITKIND_TRAP;
            signal = TARGET_SIGNAL_TRAP;
            break;
          case 0x1:  /* background mode */
            signal = m68k_bdm_analyze_exception (status);
            break;
          default:
            printf_filtered ("m68 bdm: wait: unknown atemp: %#lx\n", m68k_bdm_atemp);
            *status = TARGET_WAITKIND_STOPPED;
            signal = TARGET_SIGNAL_GRANT;
        }
      }
      else {
        unsigned long csr;
        if (bdmReadSystemRegister (BDM_REG_CSR, &csr) < 0)
          m68k_bdm_report_error ();
        if (csr & 0x08000000)  {                /* double bus fault */
          *status = TARGET_WAITKIND_STOPPED;
          signal = TARGET_SIGNAL_BUS;
        }
        else  {
          if (csr & 0x04000000)  {          /* hardware trigger */
            m68k_bdm_hit_watchpoint = 1;
            *status = TARGET_WAITKIND_STOPPED;
            signal = TARGET_SIGNAL_TRAP;
          }
          else  {
            if (csr & 0x01000000)  {      /* -BKPT signal */
              *status = TARGET_WAITKIND_STOPPED;
              signal = TARGET_SIGNAL_TRAP;
            }
            else  {
              if (csr & 0x02000000)  {    /* HALT/software bkpt */
                signal = m68k_bdm_analyze_exception (status);
              }
              else  {
                printf_filtered ("m68k-bdm: wait: unknown csr: %#lx", csr);
                *status = TARGET_WAITKIND_STOPPED;
                signal = TARGET_SIGNAL_GRANT;
              }
            }
          }
        }
      }
      break;
    default:
      printf_filtered ("m68k-bdm: wait: unknown bdm status: %#x", bdm_stat);
      break;
  }
  m68k_bdm_have_atemp = 0;
  return signal;
}

/*
 * Fetch register REGNO, or all user registers if REGNO is -1.
 */
static void
m68k_bdm_fetch_registers (int regno)
{
  unsigned long lu = 0;
  unsigned long ll = 0;
  unsigned char cbuf[8];
  int ret;

  /* ??? Some callers use 0 to mean all registers.  */
  if (regno == 0)
    regno = -1;

  if (regno < 0) {
    for (regno = 1; regno <= M68K_BDM_NUM_REGS_BDM; regno++)
      m68k_bdm_fetch_registers (regno);
  }
  else {
    /*
     * Drop down to be from 0..M68K_BDM_NUM_REGS_BDM.
     */
    regno--;
    
    if (M68K_BDM_REG_FLAGS (regno) & REG_NOT_ACCESSABLE) {
      if (m68k_bdm_debug_level) {
        printf_filtered ("m68k-bdm: fetch reg:%s(%i) is not accessable\n",
                         M68K_BDM_REG_NAME (regno), regno);
      }
      return;
    }
    
    if (M68K_BDM_REG_FLAGS (regno) & REG_WRITE_ONLY) {
      if (m68k_bdm_debug_level) {
        printf_filtered ("m68k-bdm: fetch reg:%s(%i) is write only\n",
                         M68K_BDM_REG_NAME (regno), regno);
      }
      return;
    }
    
    if (regno < 16) {
      ret = bdmReadRegister (regno, &lu);
    }
    else {
      if (regno < M68K_BDM_NUM_REGS_BDM) {
        ret = m68k_bdm_read_sys_ctl_reg (M68K_BDM_REG_NAME (regno),
                                         M68K_BDM_REG_CODE (regno),
                                         &lu);
        if ((ret == 0) &&
            (M68K_BDM_REG_TYPE (regno) == M68K_BDM_REG_TYPE_M68881_EXT))
          ret = m68k_bdm_read_sys_ctl_reg (M68K_BDM_REG_NAME (regno),
                                           M68K_BDM_REG_CODE (regno) + 1,
                                           &ll);
      }
      else {
        error ("Bad register number (%d)", regno);
        return;
      }
    }

    if (ret < 0)
      m68k_bdm_report_error ();
    
    cbuf[0] = lu >> 24;
    cbuf[1] = lu >> 16;
    cbuf[2] = lu >> 8;
    cbuf[3] = lu;
    cbuf[4] = ll >> 24;
    cbuf[5] = ll >> 16;
    cbuf[6] = ll >> 8;
    cbuf[7] = ll;
    
    if (m68k_bdm_debug_level) {
      if (M68K_BDM_REG_TYPE (regno) == M68K_BDM_REG_TYPE_M68881_EXT)
        printf_filtered ("m68k-bdm: fetch reg:%s(%i) = 0x%08lx%08lx\n",
                         M68K_BDM_REG_NAME (regno), regno, lu, ll);
      else
        printf_filtered ("m68k-bdm: fetch reg:%s(%i) = 0x%08lx\n",
                         M68K_BDM_REG_NAME (regno), regno, lu);
    }
    supply_register (regno, (char*) cbuf);
  }
}

/*
 * Store register REGNO, or all user registers if REGNO == -1.
 */
void
m68k_bdm_store_registers (int regno)
{
  /* ??? Some callers use 0 to mean all registers.  */
  if (regno == 0)
    regno = -1;

  if (regno == -1) {
    for (regno = 1; regno <= M68K_BDM_NUM_REGS_BDM; regno++)
      m68k_bdm_store_registers (regno);
  }
  else {
    unsigned char cbuf[8];

    /*
     * Drop down to be from 0..M68K_BDM_NUM_REGS_BDM.
     */
    regno--;
    
    if (M68K_BDM_REG_FLAGS (regno) & REG_NOT_ACCESSABLE) {
      if (m68k_bdm_debug_level) {
        printf_filtered ("m68k-bdm: store reg:%s(%i) is not accessable\n",
                         M68K_BDM_REG_NAME (regno), regno);
      }
      return;
    }
    
    if (M68K_BDM_REG_FLAGS (regno) & REG_READ_ONLY) {
      if (m68k_bdm_debug_level) {
        printf_filtered ("m68k-bdm: store reg:%s(%i) is read only\n",
                         M68K_BDM_REG_NAME (regno), regno);
      }
      return;
    }
    
    if (collect_register (regno, (char*) cbuf)) {
      int ret;

      unsigned long lu = 0;
      unsigned long ll = 0;
      
      lu = (cbuf[0] << 24) | (cbuf[1] << 16) | (cbuf[2] << 8) | cbuf[3];
      ll = (cbuf[4] << 24) | (cbuf[5] << 16) | (cbuf[6] << 8) | cbuf[7];
    
      if (m68k_bdm_debug_level) {
      if (M68K_BDM_REG_TYPE (regno) == M68K_BDM_REG_TYPE_M68881_EXT)
        printf_filtered ("m68k-bdm: store reg:%s(%i) = 0x%08lx%08lx\n",
                         M68K_BDM_REG_NAME (regno), regno, lu, ll);
      else
        printf_filtered ("m68k-bdm: store reg:%s(%i) = 0x%08lx\n",
                         M68K_BDM_REG_NAME (regno), regno, lu);
      }

      if (regno < 16) {
        ret = bdmWriteRegister (regno, lu);
      }
      else {
        if (regno < M68K_BDM_NUM_REGS_BDM) {
          ret = m68k_bdm_write_sys_ctl_reg (M68K_BDM_REG_NAME (regno),
                                            M68K_BDM_REG_CODE (regno),
                                            lu);
          if ((ret == 0) ||
              (M68K_BDM_REG_TYPE (regno) == M68K_BDM_REG_TYPE_M68881_EXT))
            ret = m68k_bdm_write_sys_ctl_reg (M68K_BDM_REG_NAME (regno),
                                              M68K_BDM_REG_CODE (regno) + 1,
                                              ll);
        }
        else {
          error ("m68k-bdm: bad register number (%d)", regno);
          return;
        }
      }
      if (ret < 0)
        m68k_bdm_report_error ();
    }
    else {
      if (m68k_bdm_debug_level) {
        printf_filtered ("m68k-bdm: store reg:%s(%i) is clean\n",
                         M68K_BDM_REG_NAME (regno), regno);
      }
    }
  }
}

static int
m68k_bdm_read_memory (CORE_ADDR memaddr, unsigned char *myaddr, int len)
{
  int ret = 0;
  if (bdmReadMemory (memaddr, myaddr, len) < 0) {
    m68k_bdm_report_error ();
    ret = EIO;
  }
  return ret;
}

static int
m68k_bdm_write_memory (CORE_ADDR memaddr, const unsigned char *myaddr, int len)
{
  int ret = 0;
  if (bdmWriteMemory (memaddr, (unsigned char*) myaddr, len) < 0) {
    m68k_bdm_report_error ();
    ret = EIO;
  }
  return ret;
}

static void
m68k_bdm_look_up_symbols (void)
{
}

static void
m68k_bdm_request_interrupt (void)
{
  int save_m68k_bdm_use_error = m68k_bdm_use_error;
  m68k_bdm_use_error = 0;
  m68k_bdm_stop_chip ();
  m68k_bdm_use_error = save_m68k_bdm_use_error;
}

static const char*
m68k_bdm_arch_string (void)
{
  return m68k_bdm_cpu_label;
}

static const char *
m68k_bdm_xml (const char *annex)
{
  extern const char *const xml_builtin[][2];
  int xml = 0;

  if (strcmp (annex, "target.xml") == 0) {
    while (xml_builtin[xml][0]) {
      if (strcmp (xml_builtin[xml][0], M68K_BDM_REG_XML ()) == 0)
        return xml_builtin[xml][1];
      xml++;
    }
  }

  xml = 0;
    
  while (xml_builtin[xml][0]) {
    if (strcmp (annex, xml_builtin[xml][0]) == 0)
      return xml_builtin[xml][1];
    xml++;
  }

  return NULL;
}

static int
m68k_bdm_parse_reg_value (const char* command,
                          unsigned int* reg,
                          unsigned long* value)
{
  char* s = strchr (command, ' ');
  if (!s) {
    monitor_output ("m68k-bdm: invalid command format: no reg found\n");
    return 0;
  }
  *reg = strtoul (s, &s, 0);
  if (value) {
    if (!s) {
      monitor_output ("m68k-bdm: invalid command format: no value found\n");
      return 0;
    }
    *value = strtoul (s, 0, 0);
  }
  return 1;
}

static void
m68k_bdm_cmd_help (void)
{
  monitor_output ("m68k-bdm: monitor commands:\n");
  monitor_output ("  bdm-help\n");
  monitor_output ("    This help message.\n");
  monitor_output ("  bdm-debug <level>\n");
  monitor_output ("    Set the M68K BDM debug level.\n");
  monitor_output ("  bdm-lib-debug <level>\n");
  monitor_output ("    Set the BDM library debug level.\n");
  monitor_output ("  bdm-driver-debug <level>\n");
  monitor_output ("    Set the BDM driver debug level. This may result in a\n");
  monitor_output ("    remote BDm server logging to syslog if this is " \
                  "enabled.\n");
  monitor_output ("  bdm-ctl-get <reg>\n");
  monitor_output ("    Get the control register where <reg> is a register " \
                  "value\n");
  monitor_output ("    supported by the target. For example: bdm-ctl-get " \
                  "0x801\n");
  monitor_output ("    will return the VBR register for most Coldfire " \
                  "processors.\n");
  monitor_output ("  bdm-ctl-set <reg <value>>\n");
  monitor_output ("    Set the control reigster where <reg> is a register " \
                  "value\n");
  monitor_output ("    supported by the target. For example: bdm-ctl-set " \
                  "0x801 0\n");
  monitor_output ("  bdm-dbg-get <reg>\n");
  monitor_output ("    Get the debug register where <reg> is a register value\n");
  monitor_output ("    supported by the target. For example: bdm-dbg-get 0x1\n");
  monitor_output ("    will return the XCSR register on Coldfire with Debug B+.\n");
  monitor_output ("  bdm-dbg-set <reg> <value>\n");
  monitor_output ("    Set the debug reigster where <reg> is a register value\n");
  monitor_output ("    supported by the target. For example: bdm-dbg-set 0x1 0\n");
  monitor_output ("  bdm-reset\n");
  monitor_output ("    Reset the BDM pod\n");
  monitor_output ("  bdm-sleep\n");
  monitor_output ("    Sleep the require number of milliseconds.\n");
}

static int
m68k_bdm_commands (const char* command, int len)
{
  if (M68K_BDM_STR_IS (command, "bdm-help"))
    m68k_bdm_cmd_help ();
  else if (M68K_BDM_STR_IS (command, "bdm-debug")) {
    m68k_bdm_debug_level = strtoul (command + sizeof ("bdm-debug"), 0, 0);
    if (m68k_bdm_debug_level)
      printf_filtered ("m68k-bdm: set debug level: %d\n",
                       m68k_bdm_debug_level);
  }
  else if (M68K_BDM_STR_IS (command, "bdm-lib-debug")) {
    int level = strtoul (command + sizeof ("bdm-lib-debug"), 0, 0);
    if (m68k_bdm_debug_level)
      printf_filtered ("m68k-bdm: set BDM lib debug level: %d\n",
                       level);
    bdmSetDebugFlag (level);
  }
  else if (M68K_BDM_STR_IS (command, "bdm-driver-debug")) {
    int level = strtoul (command + sizeof ("bdm-driver-debug"), 0, 0);
    if (m68k_bdm_debug_level)
      printf_filtered ("m68k-bdm: set driver debug level: %d\n",
                       level);
    bdmSetDriverDebugFlag (level);
  }
  else if (M68K_BDM_STR_IS (command, "bdm-ctl-get")) {
    unsigned int reg = 0;
    unsigned long value = 0;
    if (!m68k_bdm_parse_reg_value (command, &reg, 0))
      return 0;
    if (m68k_bdm_debug_level)
      printf_filtered ("m68k-bdm: get control reg: 0x%03x\n", reg);
    if (bdmReadControlRegister (reg, &value) < 0)
      monitor_output ("m68k-bdm: error: %s\n", bdmErrorString ());
    else
      monitor_output ("m68k-bdm: control reg: 0x%03x = %ld (0x%0lx)\n",
                      reg, value, value);
  }
  else if (M68K_BDM_STR_IS (command, "bdm-ctl-set")) {
    unsigned int reg = 0;
    unsigned long value = 0;
    if (!m68k_bdm_parse_reg_value (command, &reg, &value))
      return 0;
    if (m68k_bdm_debug_level)
      printf_filtered ("m68k-bdm: set control reg: 0x%03x = %ld (0x%0lx)\n",
                       reg, value, value);
    if (bdmWriteControlRegister (reg, value) < 0)
      monitor_output ("m68k-bdm: error: %s\n", bdmErrorString ());
  }
  else if (M68K_BDM_STR_IS (command, "bdm-dbg-get")) {
    unsigned int reg = 0;
    unsigned long value = 0;
    if (!m68k_bdm_parse_reg_value (command, &reg, 0))
      return 0;
    if (m68k_bdm_debug_level)
      printf_filtered ("m68k-bdm: get debug reg: 0x%03x\n", reg);
    if (bdmReadDebugRegister (reg, &value) < 0)
      monitor_output ("m68k-bdm: error: %s\n", bdmErrorString ());
    else
      monitor_output ("m68k-bdm: debug reg: 0x%03x = %ld (0x%0lx)\n",
                      reg, value, value);
  }
  else if (M68K_BDM_STR_IS (command, "bdm-dbg-set")) {
    unsigned int reg = 0;
    unsigned long value = 0;
    if (!m68k_bdm_parse_reg_value (command, &reg, &value))
      return 0;
    if (m68k_bdm_debug_level)
      printf_filtered ("m68k-bdm: set debug reg: 0x%03x = %ld (0x%0lx)\n",
                       reg, value, value);
    if (bdmWriteDebugRegister (reg, value) < 0)
      monitor_output ("m68k-bdm: error: %s\n", bdmErrorString ());
  }
  else if (M68K_BDM_STR_IS (command, "bdm-reset")) {
     if (m68k_bdm_debug_level)
        printf_filtered ("m68k-bdm: reset the bdm\n");
     m68k_bdm_reset();
  }
  else if (M68K_BDM_STR_IS (command, "bdm-sleep")) {
    unsigned int msecs = 0;
    msecs = strtoul (command + sizeof ("bdm-sleep"), 0, 0);
#if defined (__MINGW32__)
    Sleep (msecs);
#else
    {
      struct timeval tv;
      tv.tv_sec = msecs / 1000;
      tv.tv_usec = (msecs % 1000) * 1000;
      select (0, NULL, NULL, NULL, &tv);
    }
#endif    
  }  else {
    monitor_output ("Unknown monitor command.\n\n");
    m68k_bdm_cmd_help ();
    return 0;
  }
  return 1;
}

static struct target_ops m68k_bdm_target_ops = {
  m68k_bdm_create_inferior,
  m68k_bdm_attach,
  m68k_bdm_close,
  m68k_bdm_detach,
  m68k_bdm_join,
  m68k_bdm_thread_alive,
  m68k_bdm_resume,
  m68k_bdm_wait,
  m68k_bdm_fetch_registers,
  m68k_bdm_store_registers,
  m68k_bdm_read_memory,
  m68k_bdm_write_memory,
  m68k_bdm_look_up_symbols,
  m68k_bdm_request_interrupt,
  NULL,
  m68k_bdm_insert_point,
  m68k_bdm_remove_point,
  m68k_bdm_stopped_by_watchpoint,
  m68k_bdm_stopped_data_address,
  NULL,
  NULL,
  m68k_bdm_arch_string,
  NULL,
  m68k_bdm_xml,
  m68k_bdm_commands
};

int using_threads;

void
initialize_low (void)
{
  set_target_ops (&m68k_bdm_target_ops);
  set_breakpoint_data (m68k_bdm_breakpoint_code, m68k_bdm_breakpoint_size);
  m68k_bdm_use_error = 1;
}
