/*
 * Copyright 2019-present Facebook. All Rights Reserved.
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
 */

#ifndef __LATTICE_CPLD_H__
#define __LATTICE_CPLD_H__

#define BITS_OF_ONE_BYTE                8
#define BITS_OF_UNSIGNED_INT            4*BITS_OF_ONE_BYTE
#define LATTICE_INSTRUCTION_LENGTH      (BITS_OF_ONE_BYTE)
#define LATTICE_OPCODE_LENGTH           (BITS_OF_ONE_BYTE*3)

#define CPD_IDCODE_PUB		  0xe0 /* Read Device ID */
#define ISC_ENABLE_X          0x74 /* Enable Configuration Interface (Transparent Mode) */
#define ISC_ENABLE		      0xc6 /* Enable Configuration Interface (Offline Mode) */
#define LSC_CHECK_BUSY        0xF0 /* Read Busy Flag */

#define LSC_READ_STATUS       0x3c /* Read Status Register */
#define CPLD_STATUS_ALL_BIT       0xFFFFFFFF
#define CPLD_STATUS_BUSY_BIT      0x00010000
#define CPLD_STATUS_FAIL_BIT      0x00020000

#define ISC_ERASE             0x0e 
#define CPLD_OPCODE_ERASE_SRAM_BIT       0x1
#define CPLD_OPCODE_ERASE_FEATURE_BIT    0x2
#define CPLD_OPCODE_ERASE_CONF_FLASH_BIT 0x4
#define CPLD_OPCODE_ERASE_UFM            0x8

#define LSC_ERASE_TAG         0xcb /* Erase the UFM sector only */
#define LSC_INIT_ADDRESS      0x46 /* Set Page Address pointer to the beginning of the Configuration Flash sector */
#define LSC_WRITE_ADDRESS     0xb4 /* Set Page Address pointer to the flash page */
#define LSC_PROG_INCR_NV      0x70 /* Program one Flash page. */
#define LSC_INIT_ADDR_UFM     0x47 /* Set the Page Address Pointer to the beginning of UFM sector */
#define LSC_PROG_TAG          0xc9 /* Program one UFM page */
#define ISC_PROGRAM_USERCODE  0xc2 /* Program the USERCODE */
#define CPD_USERCODE          0xc0 /* Retrieves the 32-bit USERCODE value */
#define LSC_PROG_FEATURE      0xe4 /* Program the Feature Row bits */
#define LSC_READ_FEATURE      0xe7 /* Retrieves the Feature Row bits */
#define LSC_PROG_FEABITS      0xf8 /* Program the FEABITS */
#define LSC_READ_FEABITS      0xfb /* Retrieves the FEABITS */
#define LSC_READ_INCR_NV      0x73 /* M0PPPP - Retrieves  PPPP count pages,     M (0-I2C, 1-JTAG/SSPI/WB) */
#define LSC_READ_UFM          0xca /* M0PPPP - Retrieves  PPPP count UFM pages, M (0-I2C, 1-JTAG/SSPI/WB) */
#define LSC_PROGRAM_DONE      0x5e /* Program the DONE status bit enabling SDM */
#define LSC_PROG_OTP          0xf9 /* Makes the selected memory space One Time Programmable.*/
#define LSC_READ_OTP          0xfa /* Read the state of the One Time Programmable fuses. */
#define ISC_DISABLE           0x26 /* Exit offline or transparent programming */
#define ISC_NOOP              0xff /* No operation and device wakeup */
#define LSC_REFRESH           0x79 /* Force the MachXO2 to reconfigure, the same fashion as asserting PROGRAMN.*/
#define ISC_PROGRAM_SECURITY  0xce /* Program the Security bit (Secures CFG Flash sector).*/
#define ISC_PROGRAM_SECPLUS   0xcf /* Program the Security Plus bit (Secures CFG and UFM Sectors).*/
#define CPD_UIDCODE_PUB       0x19 /* Read 64-bit TraceID. */
#define CPD_PRELOAD           0x1c /* JTAG common instruction */

/* Shift in bitstream (.bit) generated by Diamond. Recommend using compressed bitstream
to reduce configuration time. Number of bits varies depending on compression ratio. */
#define LSC_BITSTREAM_BURST   0x7a
#define LSC_PROG_PASSWORD     0xf1 /* Program the 64-bit Password into the device. */
#define LSC_READ_PASSWORD     0xf2 /* Read the 64-bit Password from the device. */


/* When enabled (PWD_enable = 1), the write data is compared to the Password contained into the Feature
Row. If the values match, the device is unlocked for programming and configuration operations.
The device remains unlocked until a Disable Configuration command is received, a Refresh
command is issued, or a power cycle event occurs */
#define LSC_SHIFT_PASSWORD    0xbc /* Present the 64-bit Password. */


#define CPLD_TRANSPARENT_MODE 0
#define CPLD_OFFLINE_MODE     1

typedef struct {
	const char		*name;
	unsigned int    dev_id;
	unsigned short  page_bits; /* page size, unit bit */
	unsigned int    num_of_cfg_pages;
	unsigned int    num_of_ufm_pages;
}cpld_device_t;

typedef void (*progress_func_t)(int percent);

#ifdef __cplusplus
extern "C" {
#endif

jtag_object_t *jtag_hardware_mode_init(const char *dev_name);
cpld_device_t *scan_cpld_device();
int run_test_idle(unsigned char reset, unsigned char endstate, unsigned char tck);
int check_cpld_status(unsigned int options, int seconds);
int write_data_register(unsigned char endstate, unsigned int *buf, int length);
int write_onebyte_instruction(unsigned char endstate, unsigned char instruction);
int write_onebyte_opcode(unsigned char endstate, unsigned char opcode);
int read_data_register(unsigned char endstate, unsigned int *buf, int length);
int enter_configuration_mode(int mode);
int exit_configuration_mode();
int erase_cpld(unsigned char option, int num_of_loops);
int program_configuration(int bytes_per_page, int num_of_pages, progress_func_t progress);
int program_feature_row(unsigned int a, unsigned int b);
int program_feabits(unsigned short feabits);
int program_user_code(unsigned int code);
int programe_done();

#ifdef __cplusplus
}
#endif
#endif /* __LATTICE_CPLD_H__ */

