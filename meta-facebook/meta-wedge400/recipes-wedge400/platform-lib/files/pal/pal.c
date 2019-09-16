/*
 *
 * Copyright 2019-present Facebook. All Rights Reserved.
 *
 * This file contains code to support IPMI2.0 Specificaton available @
 * http://www.intel.com/content/www/us/en/servers/ipmi/ipmi-specifications.html
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

 /*
  * This file contains functions and logics that depends on Wedge100 specific
  * hardware and kernel drivers. Here, some of the empty "default" functions
  * are overridden with simple functions that returns non-zero error code.
  * This is for preventing any potential escape of failures through the
  * default functions that will return 0 no matter what.
  */

// #define DEBUG
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>
#include <openbmc/log.h>
#include <openbmc/libgpio.h>
#include <openbmc/kv.h>
#include <openbmc/obmc-i2c.h>
#include <openbmc/obmc-sensor.h>
#include <openbmc/sensor-correction.h>
#include <openbmc/misc-utils.h>
#include <facebook/bic.h>
#include <facebook/wedge_eeprom.h>
#include "pal.h"

#define GUID_SIZE 16
#define OFFSET_DEV_GUID 0x1800

#define GPIO_VAL "/sys/class/gpio/gpio%d/value"
#define SCM_BRD_ID "6-0021"
#define SENSOR_NAME_ERR "---- It should not be show ----"
uint8_t g_dev_guid[GUID_SIZE] = {0};

typedef struct {
  char name[32];
} sensor_desc_t;

struct threadinfo {
  uint8_t is_running;
  uint8_t fru;
  pthread_t pt;
};

static sensor_desc_t m_snr_desc[MAX_NUM_FRUS][MAX_SENSOR_NUM] = {0};
static struct threadinfo t_dump[MAX_NUM_FRUS] = {0, };

/* List of BIC Discrete sensors to be monitored */
const uint8_t bic_discrete_list[] = {
  BIC_SENSOR_SYSTEM_STATUS,
  BIC_SENSOR_PROC_FAIL,
  BIC_SENSOR_SYS_BOOT_STAT,
  BIC_SENSOR_CPU_DIMM_HOT,
  BIC_SENSOR_VR_HOT,
  BIC_SENSOR_POWER_THRESH_EVENT,
  BIC_SENSOR_POST_ERR,
  BIC_SENSOR_POWER_ERR,
  BIC_SENSOR_PROC_HOT_EXT,
  BIC_SENSOR_MACHINE_CHK_ERR,
  BIC_SENSOR_PCIE_ERR,
  BIC_SENSOR_OTHER_IIO_ERR,
  BIC_SENSOR_MEM_ECC_ERR,
  BIC_SENSOR_SPS_FW_HLTH,
  BIC_SENSOR_CAT_ERR,
};

// List of BIC sensors which need to do negative reading handle
const uint8_t bic_neg_reading_sensor_support_list[] = {
  /* Temperature sensors*/
  BIC_SENSOR_MB_OUTLET_TEMP,
  BIC_SENSOR_MB_INLET_TEMP,
  BIC_SENSOR_PCH_TEMP,
  BIC_SENSOR_SOC_TEMP,
  BIC_SENSOR_SOC_DIMMA_TEMP,
  BIC_SENSOR_SOC_DIMMB_TEMP,
  BIC_SENSOR_VCCIN_VR_CURR,
};

/* List of SCM sensors to be monitored */
const uint8_t scm_sensor_list[] = {
  SCM_SENSOR_OUTLET_TEMP,
  SCM_SENSOR_INLET_TEMP,
  SCM_SENSOR_HSC_VOLT,
  SCM_SENSOR_HSC_CURR,
  SCM_SENSOR_HSC_POWER,
};

/* List of SCM and BIC sensors to be monitored */
const uint8_t scm_all_sensor_list[] = {
  SCM_SENSOR_OUTLET_TEMP,
  SCM_SENSOR_INLET_TEMP,
  SCM_SENSOR_HSC_VOLT,
  SCM_SENSOR_HSC_CURR,
  SCM_SENSOR_HSC_POWER,
  BIC_SENSOR_MB_OUTLET_TEMP,
  BIC_SENSOR_MB_INLET_TEMP,
  BIC_SENSOR_PCH_TEMP,
  BIC_SENSOR_VCCIN_VR_TEMP,
  BIC_SENSOR_1V05COMB_VR_TEMP,
  BIC_SENSOR_SOC_TEMP,
  BIC_SENSOR_SOC_THERM_MARGIN,
  BIC_SENSOR_VDDR_VR_TEMP,
  BIC_SENSOR_SOC_DIMMA_TEMP,
  BIC_SENSOR_SOC_DIMMB_TEMP,
  BIC_SENSOR_SOC_PACKAGE_PWR,
  BIC_SENSOR_VCCIN_VR_POUT,
  BIC_SENSOR_VDDR_VR_POUT,
  BIC_SENSOR_SOC_TJMAX,
  BIC_SENSOR_P3V3_MB,
  BIC_SENSOR_P12V_MB,
  BIC_SENSOR_P1V05_PCH,
  BIC_SENSOR_P3V3_STBY_MB,
  BIC_SENSOR_P5V_STBY_MB,
  BIC_SENSOR_PV_BAT,
  BIC_SENSOR_PVDDR,
  BIC_SENSOR_P1V05_COMB,
  BIC_SENSOR_1V05COMB_VR_CURR,
  BIC_SENSOR_VDDR_VR_CURR,
  BIC_SENSOR_VCCIN_VR_CURR,
  BIC_SENSOR_VCCIN_VR_VOL,
  BIC_SENSOR_VDDR_VR_VOL,
  BIC_SENSOR_P1V05COMB_VR_VOL,
  BIC_SENSOR_P1V05COMB_VR_POUT,
  BIC_SENSOR_INA230_POWER,
  BIC_SENSOR_INA230_VOL,
};

/* List of SMB sensors to be monitored */
const uint8_t w400_smb_sensor_list[] = {
  SMB_SENSOR_1220_VMON1,
  SMB_SENSOR_1220_VMON2,
  SMB_SENSOR_1220_VMON3,
  SMB_SENSOR_1220_VMON4,
  SMB_SENSOR_1220_VMON5,
  SMB_SENSOR_1220_VMON6,
  SMB_SENSOR_1220_VMON7,
  SMB_SENSOR_1220_VMON8,
  SMB_SENSOR_1220_VMON9,
  SMB_SENSOR_1220_VMON10,
  SMB_SENSOR_1220_VMON11,
  SMB_SENSOR_1220_VMON12,
  SMB_SENSOR_1220_VCCA,
  SMB_SENSOR_1220_VCCINP,
  SMB_SENSOR_SW_SERDES_PVDD_VOLT,
  SMB_SENSOR_SW_SERDES_PVDD_CURR,
  SMB_SENSOR_SW_SERDES_PVDD_POWER,
  SMB_SENSOR_SW_SERDES_PVDD_TEMP1,
  SMB_SENSOR_SW_SERDES_TRVDD_VOLT,
  SMB_SENSOR_SW_SERDES_TRVDD_CURR,
  SMB_SENSOR_SW_SERDES_TRVDD_POWER,
  SMB_SENSOR_SW_SERDES_TRVDD_TEMP1,
  SMB_SENSOR_SW_CORE_VOLT,
  SMB_SENSOR_SW_CORE_CURR,
  SMB_SENSOR_SW_CORE_POWER,
  SMB_SENSOR_SW_CORE_TEMP1,
  SMB_SENSOR_TEMP1,
  SMB_SENSOR_TEMP2,
  SMB_SENSOR_TEMP3,
  SMB_SENSOR_TEMP4,
  SMB_SENSOR_TEMP5,
  SMB_SENSOR_TEMP6,
  SMB_SENSOR_SW_DIE_TEMP1,
  SMB_SENSOR_SW_DIE_TEMP2,
  /* Sensors on FCM */
  SMB_SENSOR_FCM_TEMP1,
  SMB_SENSOR_FCM_TEMP2,
  SMB_SENSOR_FCM_HSC_VOLT,
  SMB_SENSOR_FCM_HSC_CURR,
  SMB_SENSOR_FCM_HSC_POWER,
  /* Sensors FAN Speed */
  SMB_SENSOR_FAN1_FRONT_TACH,
  SMB_SENSOR_FAN1_REAR_TACH,
  SMB_SENSOR_FAN2_FRONT_TACH,
  SMB_SENSOR_FAN2_REAR_TACH,
  SMB_SENSOR_FAN3_FRONT_TACH,
  SMB_SENSOR_FAN3_REAR_TACH,
  SMB_SENSOR_FAN4_FRONT_TACH,
  SMB_SENSOR_FAN4_REAR_TACH,
};

const uint8_t w400c_smb_sensor_list[] = {
  SMB_SENSOR_1220_VMON1,
  SMB_SENSOR_1220_VMON2,
  SMB_SENSOR_1220_VMON3,
  SMB_SENSOR_1220_VMON4,
  SMB_SENSOR_1220_VMON5,
  SMB_SENSOR_1220_VMON6,
  SMB_SENSOR_1220_VMON7,
  SMB_SENSOR_1220_VMON8,
  SMB_SENSOR_1220_VMON9,
  SMB_SENSOR_1220_VMON10,
  SMB_SENSOR_1220_VMON11,
  SMB_SENSOR_1220_VMON12,
  SMB_SENSOR_1220_VCCA,
  SMB_SENSOR_1220_VCCINP,
  SMB_SENSOR_SW_SERDES_PVDD_VOLT,
  SMB_SENSOR_SW_SERDES_PVDD_CURR,
  SMB_SENSOR_SW_SERDES_PVDD_POWER,
  SMB_SENSOR_SW_SERDES_PVDD_TEMP1,
  SMB_SENSOR_SW_SERDES_TRVDD_VOLT,
  SMB_SENSOR_SW_SERDES_TRVDD_CURR,
  SMB_SENSOR_SW_SERDES_TRVDD_POWER,
  SMB_SENSOR_SW_SERDES_TRVDD_TEMP1,
  SMB_SENSOR_SW_CORE_VOLT,
  SMB_SENSOR_SW_CORE_CURR,
  SMB_SENSOR_SW_CORE_POWER,
  SMB_SENSOR_TEMP1,
  SMB_SENSOR_TEMP2,
  SMB_SENSOR_TEMP3,
  SMB_SENSOR_TEMP4,
  SMB_SENSOR_TEMP5,
  SMB_SENSOR_TEMP6,
  /* Sensors on FCM */
  SMB_SENSOR_FCM_TEMP1,
  SMB_SENSOR_FCM_TEMP2,
  SMB_SENSOR_FCM_HSC_VOLT,
  SMB_SENSOR_FCM_HSC_CURR,
  SMB_SENSOR_FCM_HSC_POWER,
  /* Sensors FAN Speed */
  SMB_SENSOR_FAN1_FRONT_TACH,
  SMB_SENSOR_FAN1_REAR_TACH,
  SMB_SENSOR_FAN2_FRONT_TACH,
  SMB_SENSOR_FAN2_REAR_TACH,
  SMB_SENSOR_FAN3_FRONT_TACH,
  SMB_SENSOR_FAN3_REAR_TACH,
  SMB_SENSOR_FAN4_FRONT_TACH,
  SMB_SENSOR_FAN4_REAR_TACH,
};

const uint8_t pem1_sensor_list[] = {
  PEM1_SENSOR_IN_VOLT,
  PEM1_SENSOR_OUT_VOLT,
  PEM1_SENSOR_CURR,
  PEM1_SENSOR_POWER,
  PEM1_SENSOR_FAN1_TACH,
  PEM1_SENSOR_FAN2_TACH,
  PEM1_SENSOR_TEMP1,
  PEM1_SENSOR_TEMP2,
  PEM1_SENSOR_TEMP3,
};

const uint8_t pem1_discrete_list[] = {
  /* Discrete fault sensors on PEM1 */
  PEM1_SENSOR_FAULT_OV,
  PEM1_SENSOR_FAULT_UV,
  PEM1_SENSOR_FAULT_OC,
  PEM1_SENSOR_FAULT_POWER,
  PEM1_SENSOR_ON_FAULT,
  PEM1_SENSOR_FAULT_FET_SHORT,
  PEM1_SENSOR_FAULT_FET_BAD,
  PEM1_SENSOR_EEPROM_DONE,
  /* Discrete ADC alert sensors on PEM2 */
  PEM1_SENSOR_POWER_ALARM_HIGH,
  PEM1_SENSOR_POWER_ALARM_LOW,
  PEM1_SENSOR_VSENSE_ALARM_HIGH,
  PEM1_SENSOR_VSENSE_ALARM_LOW,
  PEM1_SENSOR_VSOURCE_ALARM_HIGH,
  PEM1_SENSOR_VSOURCE_ALARM_LOW,
  PEM1_SENSOR_GPIO_ALARM_HIGH,
  PEM1_SENSOR_GPIO_ALARM_LOW,
  /* Discrete status sensors on PEM1 */
  PEM1_SENSOR_ON_STATUS,
  PEM1_SENSOR_STATUS_FET_BAD,
  PEM1_SENSOR_STATUS_FET_SHORT,
  PEM1_SENSOR_STATUS_ON_PIN,
  PEM1_SENSOR_STATUS_POWER_GOOD,
  PEM1_SENSOR_STATUS_OC,
  PEM1_SENSOR_STATUS_UV,
  PEM1_SENSOR_STATUS_OV,
  PEM1_SENSOR_STATUS_GPIO3,
  PEM1_SENSOR_STATUS_GPIO2,
  PEM1_SENSOR_STATUS_GPIO1,
  PEM1_SENSOR_STATUS_ALERT,
  PEM1_SENSOR_STATUS_EEPROM_BUSY,
  PEM1_SENSOR_STATUS_ADC_IDLE,
  PEM1_SENSOR_STATUS_TICKER_OVERFLOW,
  PEM1_SENSOR_STATUS_METER_OVERFLOW,
};

const uint8_t pem2_sensor_list[] = {
  PEM2_SENSOR_IN_VOLT,
  PEM2_SENSOR_OUT_VOLT,
  PEM2_SENSOR_CURR,
  PEM2_SENSOR_POWER,
  PEM2_SENSOR_FAN1_TACH,
  PEM2_SENSOR_FAN2_TACH,
  PEM2_SENSOR_TEMP1,
  PEM2_SENSOR_TEMP2,
  PEM2_SENSOR_TEMP3,
};

const uint8_t pem2_discrete_list[] = {
  /* Discrete fault sensors on PEM2 */
  PEM2_SENSOR_FAULT_OV,
  PEM2_SENSOR_FAULT_UV,
  PEM2_SENSOR_FAULT_OC,
  PEM2_SENSOR_FAULT_POWER,
  PEM2_SENSOR_ON_FAULT,
  PEM2_SENSOR_FAULT_FET_SHORT,
  PEM2_SENSOR_FAULT_FET_BAD,
  PEM2_SENSOR_EEPROM_DONE,
  /* Discrete ADC alert sensors on PEM2 */
  PEM2_SENSOR_POWER_ALARM_HIGH,
  PEM2_SENSOR_POWER_ALARM_LOW,
  PEM2_SENSOR_VSENSE_ALARM_HIGH,
  PEM2_SENSOR_VSENSE_ALARM_LOW,
  PEM2_SENSOR_VSOURCE_ALARM_HIGH,
  PEM2_SENSOR_VSOURCE_ALARM_LOW,
  PEM2_SENSOR_GPIO_ALARM_HIGH,
  PEM2_SENSOR_GPIO_ALARM_LOW,
  /* Discrete status sensors on PEM2 */
  PEM2_SENSOR_ON_STATUS,
  PEM2_SENSOR_STATUS_FET_BAD,
  PEM2_SENSOR_STATUS_FET_SHORT,
  PEM2_SENSOR_STATUS_ON_PIN,
  PEM2_SENSOR_STATUS_POWER_GOOD,
  PEM2_SENSOR_STATUS_OC,
  PEM2_SENSOR_STATUS_UV,
  PEM2_SENSOR_STATUS_OV,
  PEM2_SENSOR_STATUS_GPIO3,
  PEM2_SENSOR_STATUS_GPIO2,
  PEM2_SENSOR_STATUS_GPIO1,
  PEM2_SENSOR_STATUS_ALERT,
  PEM2_SENSOR_STATUS_EEPROM_BUSY,
  PEM2_SENSOR_STATUS_ADC_IDLE,
  PEM2_SENSOR_STATUS_TICKER_OVERFLOW,
  PEM2_SENSOR_STATUS_METER_OVERFLOW,
};

const uint8_t psu1_sensor_list[] = {
  PSU1_SENSOR_IN_VOLT,
  PSU1_SENSOR_12V_VOLT,
  PSU1_SENSOR_STBY_VOLT,
  PSU1_SENSOR_IN_CURR,
  PSU1_SENSOR_12V_CURR,
  PSU1_SENSOR_STBY_CURR,
  PSU1_SENSOR_IN_POWER,
  PSU1_SENSOR_12V_POWER,
  PSU1_SENSOR_STBY_POWER,
  PSU1_SENSOR_FAN_TACH,
  PSU1_SENSOR_TEMP1,
  PSU1_SENSOR_TEMP2,
  PSU1_SENSOR_TEMP3,
};

const uint8_t psu2_sensor_list[] = {
  PSU2_SENSOR_IN_VOLT,
  PSU2_SENSOR_12V_VOLT,
  PSU2_SENSOR_STBY_VOLT,
  PSU2_SENSOR_IN_CURR,
  PSU2_SENSOR_12V_CURR,
  PSU2_SENSOR_STBY_CURR,
  PSU2_SENSOR_IN_POWER,
  PSU2_SENSOR_12V_POWER,
  PSU2_SENSOR_STBY_POWER,
  PSU2_SENSOR_FAN_TACH,
  PSU2_SENSOR_TEMP1,
  PSU2_SENSOR_TEMP2,
  PSU2_SENSOR_TEMP3,
};


float scm_sensor_threshold[MAX_SENSOR_NUM][MAX_SENSOR_THRESHOLD + 1] = {0};
float smb_sensor_threshold[MAX_SENSOR_NUM][MAX_SENSOR_THRESHOLD + 1] = {0};
float pem_sensor_threshold[MAX_SENSOR_NUM][MAX_SENSOR_THRESHOLD + 1] = {0};
float psu_sensor_threshold[MAX_SENSOR_NUM][MAX_SENSOR_THRESHOLD + 1] = {0};

size_t bic_discrete_cnt = sizeof(bic_discrete_list)/sizeof(uint8_t);
size_t scm_sensor_cnt = sizeof(scm_sensor_list)/sizeof(uint8_t);
size_t scm_all_sensor_cnt = sizeof(scm_all_sensor_list)/sizeof(uint8_t);
size_t w400_smb_sensor_cnt = sizeof(w400_smb_sensor_list)/sizeof(uint8_t);
size_t w400c_smb_sensor_cnt = sizeof(w400c_smb_sensor_list)/sizeof(uint8_t);
size_t pem1_sensor_cnt = sizeof(pem1_sensor_list)/sizeof(uint8_t);
size_t pem1_discrete_cnt = sizeof(pem1_discrete_list)/sizeof(uint8_t);
size_t pem2_sensor_cnt = sizeof(pem2_sensor_list)/sizeof(uint8_t);
size_t pem2_discrete_cnt = sizeof(pem2_discrete_list)/sizeof(uint8_t);
size_t psu1_sensor_cnt = sizeof(psu1_sensor_list)/sizeof(uint8_t);
size_t psu2_sensor_cnt = sizeof(psu2_sensor_list)/sizeof(uint8_t);

static sensor_info_t g_sinfo[MAX_NUM_FRUS][MAX_SENSOR_NUM] = {0};

static float hsc_rsense[MAX_NUM_FRUS] = {0};

const char pal_fru_list[] = "all, scm, smb, psu1, psu2, pem1, pem2";

char * key_list[] = {
"pwr_server_last_state",
"sysfw_ver_server",
"timestamp_sled",
"server_por_cfg",
"server_sel_error",
"scm_sensor_health",
"smb_sensor_health",
"fcm_sensor_health",
"pem1_sensor_health",
"pem2_sensor_health",
"psu1_sensor_health",
"psu2_sensor_health",
"fan1_sensor_health",
"fan2_sensor_health",
"fan3_sensor_health",
"fan4_sensor_health",
"slot1_boot_order",
/* Add more Keys here */
LAST_KEY /* This is the last key of the list */
};

char * def_val_list[] = {
  "on", /* pwr_server_last_state */
  "0", /* sysfw_ver_server */
  "0", /* timestamp_sled */
  "lps", /* server_por_cfg */
  "1", /* server_sel_error */
  "1", /* scm_sensor_health */
  "1", /* smb_sensor_health */
  "1", /* fcm_sensor_health */
  "1", /* pem1_sensor_health */
  "1", /* pem2_sensor_health */
  "1", /* psu1_sensor_health */
  "1", /* psu2_sensor_health */
  "1", /* fan1_sensor_health */
  "1", /* fan2_sensor_health */
  "1", /* fan3_sensor_health */
  "1", /* fan4_sensor_health */
  /* Add more def values for the correspoding keys*/
  LAST_KEY /* Same as last entry of the key_list */
};

// Helper Functions
static int
read_device(const char *device, int *value) {
  FILE *fp;
  int rc;

  fp = fopen(device, "r");
  if (!fp) {
    int err = errno;
#ifdef DEBUG
    OBMC_INFO("failed to open device %s", device);
#endif
    return err;
  }

  rc = fscanf(fp, "%i", value);
  fclose(fp);
  if (rc != 1) {
#ifdef DEBUG
    OBMC_INFO("failed to read device %s", device);
#endif
    return ENOENT;
  } else {
    return 0;
  }
}

// Helper Functions
static int
read_device_float(const char *device, float *value) {
  FILE *fp;
  int rc;

  fp = fopen(device, "r");
  if (!fp) {
    int err = errno;
#ifdef DEBUG
    OBMC_INFO("failed to open device %s", device);
#endif
    return err;
  }

  rc = fscanf(fp, "%f", value);
  fclose(fp);
  if (rc != 1) {
#ifdef DEBUG
    OBMC_INFO("failed to read device %s", device);
#endif
    return ENOENT;
  } else {
    return 0;
  }
}

static int
write_device(const char *device, const char *value) {
  FILE *fp;
  int rc;

  fp = fopen(device, "w");
  if (!fp) {
    int err = errno;
#ifdef DEBUG
    OBMC_INFO("failed to open device for write %s", device);
#endif
    return err;
  }

  rc = fputs(value, fp);
  fclose(fp);

  if (rc < 0) {
#ifdef DEBUG
    OBMC_INFO("failed to write device %s", device);
#endif
    return ENOENT;
  } else {
    return 0;
  }
}

int
pal_detect_i2c_device(uint8_t bus, uint8_t addr) {

  int fd = -1, rc = -1;
  char fn[32];

  snprintf(fn, sizeof(fn), "/dev/i2c-%d", bus);
  fd = open(fn, O_RDWR);
  if (fd == -1) {
    OBMC_WARN("Failed to open i2c device %s", fn);
    return -1;
  }

  rc = ioctl(fd, I2C_SLAVE_FORCE, addr);
  if (rc < 0) {
    OBMC_WARN("Failed to open slave @ address 0x%x", addr);
    close(fd);
    return -1;
  }

  rc = i2c_smbus_read_byte(fd);
  close(fd);

  if (rc < 0) {
    return -1;
  } else {
    return 0;
  }
}

int
pal_add_i2c_device(uint8_t bus, uint8_t addr, char *device_name) {

  int ret = -1;
  char cmd[64];

  snprintf(cmd, sizeof(cmd),
            "echo %s %d > /sys/bus/i2c/devices/i2c-%d/new_device",
              device_name, addr, bus);

#ifdef DEBUG
  OBMC_WARN("[%s] Cmd: %s", __func__, cmd);
#endif

  ret = run_command(cmd);

  return ret;
}

int
pal_del_i2c_device(uint8_t bus, uint8_t addr) {

  int ret = -1;
  char cmd[64];

  sprintf(cmd, "echo %d > /sys/bus/i2c/devices/i2c-%d/delete_device",
           addr, bus);

#ifdef DEBUG
  OBMC_WARN("[%s] Cmd: %s", __func__, cmd);
#endif

  ret = run_command(cmd);

  return ret;
}

void
pal_inform_bic_mode(uint8_t fru, uint8_t mode) {
  switch(mode) {
  case BIC_MODE_NORMAL:
    // Bridge IC entered normal mode
    // Inform BIOS that BMC is ready
    bic_set_gpio(fru, BMC_READY_N, 0);
    break;
  case BIC_MODE_UPDATE:
    // Bridge IC entered update mode
    // TODO: Might need to handle in future
    break;
  default:
    break;
  }
}

//For OEM command "CMD_OEM_GET_PLAT_INFO" 0x7e
int
pal_get_plat_sku_id(void){
  return 0x06; // Wedge400/Wedge400-C
}


//Use part of the function for OEM Command "CMD_OEM_GET_POSS_PCIE_CONFIG" 0xF4
int
pal_get_poss_pcie_config(uint8_t slot, uint8_t *req_data, uint8_t req_len,
                         uint8_t *res_data, uint8_t *res_len) {

  uint8_t completion_code = CC_SUCCESS;
  uint8_t pcie_conf = 0x02; // Wedge400/wedge400-C
  uint8_t *data = res_data;
  *data++ = pcie_conf;
  *res_len = data - res_data;
  return completion_code;
}

static int
pal_key_check(char *key) {
  int i;

  i = 0;
  while(strcmp(key_list[i], LAST_KEY)) {
    // If Key is valid, return success
    if (!strcmp(key, key_list[i]))
      return 0;

    i++;
  }

#ifdef DEBUG
  OBMC_WARN("pal_key_check: invalid key - %s", key);
#endif
  return -1;
}

int
pal_get_key_value(char *key, char *value) {
  int ret;
  // Check is key is defined and valid
  if (pal_key_check(key))
    return -1;

  ret = kv_get(key, value, NULL, KV_FPERSIST);
  return ret;
}

int
pal_set_key_value(char *key, char *value) {

  // Check is key is defined and valid
  if (pal_key_check(key))
    return -1;

  return kv_set(key, value, 0, KV_FPERSIST);
}

int
pal_set_sysfw_ver(uint8_t slot, uint8_t *ver) {
  int i;
  char key[MAX_KEY_LEN];
  char str[MAX_VALUE_LEN] = {0};
  char tstr[10];
  sprintf(key, "%s", "sysfw_ver_server");

  for (i = 0; i < SIZE_SYSFW_VER; i++) {
    sprintf(tstr, "%02x", ver[i]);
    strcat(str, tstr);
  }

  return pal_set_key_value(key, str);
}

int
pal_get_sysfw_ver(uint8_t slot, uint8_t *ver) {
  int i;
  int j = 0;
  int ret;
  int msb, lsb;
  char key[MAX_KEY_LEN];
  char str[MAX_VALUE_LEN] = {0};
  char tstr[4];

  sprintf(key, "%s", "sysfw_ver_server");

  ret = pal_get_key_value(key, str);
  if (ret) {
    return ret;
  }

  for (i = 0; i < 2*SIZE_SYSFW_VER; i += 2) {
    sprintf(tstr, "%c\n", str[i]);
    msb = strtol(tstr, NULL, 16);

    sprintf(tstr, "%c\n", str[i+1]);
    lsb = strtol(tstr, NULL, 16);
    ver[j++] = (msb << 4) | lsb;
  }

  return 0;
}

int
pal_get_fru_list(char *list) {
  strcpy(list, pal_fru_list);
  return 0;
}

int
pal_get_fru_id(char *str, uint8_t *fru) {
  if (!strcmp(str, "all")) {
    *fru = FRU_ALL;
  } else if (!strcmp(str, "smb")) {
    *fru = FRU_SMB;
  } else if (!strcmp(str, "scm")) {
    *fru = FRU_SCM;
  } else if (!strcmp(str, "pem1")) {
    *fru = FRU_PEM1;
  } else if (!strcmp(str, "pem2")) {
    *fru = FRU_PEM2;
  } else if (!strcmp(str, "psu1")) {
    *fru = FRU_PSU1;
  } else if (!strcmp(str, "psu2")) {
    *fru = FRU_PSU2;
  } else if (!strcmp(str, "fan1")) {
    *fru = FRU_FAN1;
  } else if (!strcmp(str, "fan2")) {
    *fru = FRU_FAN2;
  } else if (!strcmp(str, "fan3")) {
    *fru = FRU_FAN3;
  } else if (!strcmp(str, "fan4")) {
    *fru = FRU_FAN4;
  } else {
    OBMC_WARN("pal_get_fru_id: Wrong fru#%s", str);
    return -1;
  }

  return 0;
}

int
pal_get_fru_name(uint8_t fru, char *name) {
  switch(fru) {
    case FRU_SMB:
      strcpy(name, "smb");
      break;
    case FRU_SCM:
      strcpy(name, "scm");
      break;
    case FRU_FCM:
      strcpy(name, "fcm");
      break;
    case FRU_PEM1:
      strcpy(name, "pem1");
      break;
    case FRU_PEM2:
      strcpy(name, "pem2");
      break;
    case FRU_PSU1:
      strcpy(name, "psu1");
      break;
    case FRU_PSU2:
      strcpy(name, "psu2");
      break;
    case FRU_FAN1:
      strcpy(name, "fan1");
      break;
    case FRU_FAN2:
      strcpy(name, "fan2");
      break;
    case FRU_FAN3:
      strcpy(name, "fan3");
      break;
    case FRU_FAN4:
      strcpy(name, "fan4");
      break;
    default:
      if (fru > MAX_NUM_FRUS)
        return -1;
      sprintf(name, "fru%d", fru);
      break;
  }
  return 0;
}

// Platform Abstraction Layer (PAL) Functions
int
pal_get_platform_name(char *name) {
  strcpy(name, PLATFORM_NAME);

  return 0;
}

int
pal_is_fru_prsnt(uint8_t fru, uint8_t *status) {
  int val,ext_prsnt;
  char tmp[LARGEST_DEVICE_NAME];
  char path[LARGEST_DEVICE_NAME + 1];
  *status = 0;

  switch (fru) {
    case FRU_SMB:
      *status = 1;
      return 0;
    case FRU_SCM:
      snprintf(path, LARGEST_DEVICE_NAME, SMB_SYSFS, SCM_PRSNT_STATUS);
      break;
    case FRU_FCM:
      *status = 1;
      return 0;
    case FRU_PEM1:
      snprintf(tmp, LARGEST_DEVICE_NAME, SMB_SYSFS, PEM_PRSNT_STATUS);
      snprintf(path, LARGEST_DEVICE_NAME, tmp, 1);
      break;
    case FRU_PEM2:
      snprintf(tmp, LARGEST_DEVICE_NAME, SMB_SYSFS, PEM_PRSNT_STATUS);
      snprintf(path, LARGEST_DEVICE_NAME, tmp, 2);
      break;
    case FRU_PSU1:
      snprintf(tmp, LARGEST_DEVICE_NAME, SMB_SYSFS, PSU_PRSNT_STATUS);
      snprintf(path, LARGEST_DEVICE_NAME, tmp, 1);
      break;
    case FRU_PSU2:
      snprintf(tmp, LARGEST_DEVICE_NAME, SMB_SYSFS, PSU_PRSNT_STATUS);
      snprintf(path, LARGEST_DEVICE_NAME, tmp, 2);
      break;
    case FRU_FAN1:
    case FRU_FAN2:
    case FRU_FAN3:
    case FRU_FAN4:
      snprintf(tmp, LARGEST_DEVICE_NAME, FCM_SYSFS, FAN_PRSNT_STATUS);
      snprintf(path, LARGEST_DEVICE_NAME, tmp, fru - FRU_FAN1 + 1);
      break;
    default:
      printf("unsupported fru id %d\n", fru);
      return -1;
    }

    if (read_device(path, &val)) {
      return -1;
    }

    if (val == 0x0) {
      *status = 1;
    } else {
      *status = 0;
      return 0;
    }

    if ( fru == FRU_PEM1 || fru == FRU_PSU1 ){
      ext_prsnt = pal_detect_i2c_device(22,0x18); // 0 present -1 absent
      if( fru == FRU_PEM1 && ext_prsnt == 0 ){ // for PEM 0x18 should present
        *status = 1;
      } else if ( fru == FRU_PSU1 && ext_prsnt < 0 ){ // for PSU 0x18 should absent
        *status = 1;
      } else {
        *status = 0;
      }
    }
    else if ( fru == FRU_PEM2 || fru == FRU_PSU2 ){
      ext_prsnt = pal_detect_i2c_device(23,0x18); // 0 present -1 absent
      if( fru == FRU_PEM2 && ext_prsnt == 0 ){ // for PEM 0x18 should present
        *status = 1;
      } else if ( fru == FRU_PSU2 && ext_prsnt < 0 ){ // for PSU 0x18 should absent
        *status = 1;
      } else {
        *status = 0;
      }
    }
    return 0;
}

static int check_dir_exist(const char *device);

int
pal_is_fru_ready(uint8_t fru, uint8_t *status) {
  int ret = 0;

  switch(fru) {
    case FRU_PEM1:
      if(!check_dir_exist(PEM1_LTC4282_DIR) && !check_dir_exist(PEM1_MAX6615_DIR)) {
        *status = 1;
      }
      break;
    case FRU_PEM2:
      if(!check_dir_exist(PEM2_LTC4282_DIR) && !check_dir_exist(PEM2_MAX6615_DIR)) {
        *status = 1;
      }
      break;
    case FRU_PSU1:
      if(!check_dir_exist(PSU1_DEVICE)) {
        *status = 1;
      }
      break;
    case FRU_PSU2:
      if(!check_dir_exist(PSU2_DEVICE)) {
        *status = 1;
      }
      break;
    default:
      *status = 1;

      break;
  }

  return ret;
}

int
pal_get_sensor_util_timeout(uint8_t fru) {
  return READ_SENSOR_TIMEOUT;
}

int
pal_get_fru_sensor_list(uint8_t fru, uint8_t **sensor_list, int *cnt) {
  uint8_t brd_type;
  pal_get_board_type(&brd_type);
  switch(fru) {
  case FRU_SCM:
    *sensor_list = (uint8_t *) scm_all_sensor_list;
    *cnt = scm_all_sensor_cnt;
    break;
  case FRU_SMB:
    if(brd_type == BRD_TYPE_WEDGE400){
      *sensor_list = (uint8_t *) w400_smb_sensor_list;
      *cnt = w400_smb_sensor_cnt;
    }else if(brd_type == BRD_TYPE_WEDGE400_2){
      *sensor_list = (uint8_t *) w400c_smb_sensor_list;
      *cnt = w400c_smb_sensor_cnt;
    }
    break;
  case FRU_PEM1:
    *sensor_list = (uint8_t *) pem1_sensor_list;
    *cnt = pem1_sensor_cnt;
    break;
  case FRU_PEM2:
    *sensor_list = (uint8_t *) pem2_sensor_list;
    *cnt = pem2_sensor_cnt;
    break;
  case FRU_PSU1:
    *sensor_list = (uint8_t *) psu1_sensor_list;
    *cnt = psu1_sensor_cnt;
    break;
  case FRU_PSU2:
    *sensor_list = (uint8_t *) psu2_sensor_list;
    *cnt = psu2_sensor_cnt;
    break;
  default:
    if (fru > MAX_NUM_FRUS)
      return -1;
    // Nothing to read yet.
    *sensor_list = NULL;
    *cnt = 0;
  }
  return 0;
}

void
pal_update_ts_sled()
{
  char key[MAX_KEY_LEN];
  char tstr[MAX_VALUE_LEN] = {0};
  struct timespec ts;

  clock_gettime(CLOCK_REALTIME, &ts);
  sprintf(tstr, "%ld", ts.tv_sec);

  sprintf(key, "timestamp_sled");

  pal_set_key_value(key, tstr);
}

int
pal_get_fru_discrete_list(uint8_t fru, uint8_t **sensor_list, int *cnt) {
  switch(fru) {
  case FRU_SCM:
    *sensor_list = (uint8_t *) bic_discrete_list;
    *cnt = bic_discrete_cnt;
    break;
  case FRU_PEM1:
    *sensor_list = (uint8_t *) pem1_discrete_list;
    *cnt = pem1_discrete_cnt;
    break;
  case FRU_PEM2:
    *sensor_list = (uint8_t *) pem2_discrete_list;
    *cnt = pem2_discrete_cnt;
    break;
  default:
    if (fru > MAX_NUM_FRUS)
      return -1;
    // Nothing to read yet.
    *sensor_list = NULL;
    *cnt = 0;
  }
    return 0;
}

static void
_print_sensor_discrete_log(uint8_t fru, uint8_t snr_num, char *snr_name,
    uint8_t val, char *event) {
  if (val) {
    OBMC_CRIT("ASSERT: %s discrete - raised - FRU: %d, num: 0x%X,"
        " snr: %-16s val: %d", event, fru, snr_num, snr_name, val);
  } else {
    OBMC_CRIT("DEASSERT: %s discrete - settled - FRU: %d, num: 0x%X,"
        " snr: %-16s val: %d", event, fru, snr_num, snr_name, val);
  }
  pal_update_ts_sled();
}

int
pal_sensor_discrete_check(uint8_t fru, uint8_t snr_num, char *snr_name,
    uint8_t o_val, uint8_t n_val) {

  char name[32], crisel[128];
  bool valid = false;
  uint8_t diff = o_val ^ n_val;

  if (GETBIT(diff, 0)) {
    switch(snr_num) {
      case BIC_SENSOR_SYSTEM_STATUS:
        sprintf(name, "SOC_Thermal_Trip");
        valid = true;

        sprintf(crisel, "%s - %s,FRU:%u",
                        name, GETBIT(n_val, 0)?"ASSERT":"DEASSERT", fru);
        pal_add_cri_sel(crisel);
        break;
      case BIC_SENSOR_VR_HOT:
        sprintf(name, "SOC_VR_Hot");
        valid = true;
        break;
    }
    if (valid) {
      _print_sensor_discrete_log(fru, snr_num, snr_name,
                                      GETBIT(n_val, 0), name);
      valid = false;
    }
  }

  if (GETBIT(diff, 1)) {
    switch(snr_num) {
      case BIC_SENSOR_SYSTEM_STATUS:
        sprintf(name, "SOC_FIVR_Fault");
        valid = true;
        break;
      case BIC_SENSOR_VR_HOT:
        sprintf(name, "SOC_DIMM_AB_VR_Hot");
        valid = true;
        break;
      case BIC_SENSOR_CPU_DIMM_HOT:
        sprintf(name, "SOC_MEMHOT");
        valid = true;
        break;
    }
    if (valid) {
      _print_sensor_discrete_log(fru, snr_num, snr_name,
                                      GETBIT(n_val, 1), name);
      valid = false;
    }
  }

  if (GETBIT(diff, 2)) {
    switch(snr_num) {
      case BIC_SENSOR_SYSTEM_STATUS:
        sprintf(name, "SYS_Throttle");
        valid = true;
        break;
      case BIC_SENSOR_VR_HOT:
        sprintf(name, "SOC_DIMM_DE_VR_Hot");
        valid = true;
        break;
    }
    if (valid) {
      _print_sensor_discrete_log(fru, snr_num, snr_name,
                                      GETBIT(n_val, 2), name);
      valid = false;
    }
  }

  if (GETBIT(diff, 4)) {
    if (snr_num == BIC_SENSOR_PROC_FAIL) {
        _print_sensor_discrete_log(fru, snr_num, snr_name,
                                        GETBIT(n_val, 4), "FRB3");
    }
  }

  return 0;
}

int
pal_is_debug_card_prsnt(uint8_t *status) {
  int val;
  char path[LARGEST_DEVICE_NAME + 1];

  snprintf(path, LARGEST_DEVICE_NAME, GPIO_DEBUG_PRSNT_N, "value");

  if (read_device(path, &val)) {
    return -1;
  }

  if (val) {
    *status = 1;
  } else {
    *status = 0;
  }

  return 0;
}

// Enable POST buffer for the server in given slot
int
pal_post_enable(uint8_t slot) {
  int ret;

  bic_config_t config = {0};
  bic_config_u *t = (bic_config_u *) &config;

  ret = bic_get_config(IPMB_BUS, &config);
  if (ret) {
#ifdef DEBUG
    OBMC_WARN("post_enable: bic_get_config failed for fru: %d\n", slot);
#endif
    return ret;
  }

  if (0 == t->bits.post) {
    t->bits.post = 1;
    ret = bic_set_config(IPMB_BUS, &config);
    if (ret) {
#ifdef DEBUG
      OBMC_WARN("post_enable: bic_set_config failed\n");
#endif
      return ret;
    }
  }

  return 0;
}

// Get the last post code of the given slot
int
pal_post_get_last(uint8_t slot, uint8_t *status) {
  int ret;
  uint8_t buf[MAX_IPMB_RES_LEN] = {0x0};
  uint8_t len = 0 ;

  ret = bic_get_post_buf(IPMB_BUS, buf, &len);
  if (ret) {
    return ret;
  }

  *status = buf[0];

  return 0;
}

int
pal_post_get_last_and_len(uint8_t slot, uint8_t *data, uint8_t *len) {
  int ret;

  ret = bic_get_post_buf(IPMB_BUS, data, len);
  if (ret) {
    return ret;
  }

  return 0;
}

int
pal_get_80port_record(uint8_t slot, uint8_t *req_data, uint8_t req_len, uint8_t *res_data, uint8_t *res_len)
{
  int ret;

  ret = bic_get_post_buf(IPMB_BUS, res_data, res_len);
  if (ret) {
    return ret;
  }

  return 0;
}

static int
pal_set_post_gpio_out(void) {
  char path[LARGEST_DEVICE_NAME + 1];
  int ret;
  char *val = "out";

  snprintf(path, LARGEST_DEVICE_NAME, GPIO_POSTCODE_0, "direction");
  ret = write_device(path, val);
  if (ret) {
    goto post_exit;
  }

  snprintf(path, LARGEST_DEVICE_NAME, GPIO_POSTCODE_1, "direction");
  ret = write_device(path, val);
  if (ret) {
    goto post_exit;
  }

  snprintf(path, LARGEST_DEVICE_NAME, GPIO_POSTCODE_2, "direction");
  ret = write_device(path, val);
  if (ret) {
    goto post_exit;
  }

  snprintf(path, LARGEST_DEVICE_NAME, GPIO_POSTCODE_3, "direction");
  ret = write_device(path, val);
  if (ret) {
    goto post_exit;
  }

  snprintf(path, LARGEST_DEVICE_NAME, GPIO_POSTCODE_4, "direction");
  ret = write_device(path, val);
  if (ret) {
    goto post_exit;
  }

  snprintf(path, LARGEST_DEVICE_NAME, GPIO_POSTCODE_5, "direction");
  ret = write_device(path, val);
  if (ret) {
    goto post_exit;
  }

  snprintf(path, LARGEST_DEVICE_NAME, GPIO_POSTCODE_6, "direction");
  ret = write_device(path, val);
  if (ret) {
    goto post_exit;
  }

  snprintf(path, LARGEST_DEVICE_NAME, GPIO_POSTCODE_7, "direction");
  ret = write_device(path, val);
  if (ret) {
    goto post_exit;
  }

  post_exit:
  if (ret) {
#ifdef DEBUG
    OBMC_WARN("write_device failed for %s\n", path);
#endif
    return -1;
  } else {
    return 0;
  }
}

// Display the given POST code using GPIO port
static int
pal_post_display(uint8_t status) {
  char path[LARGEST_DEVICE_NAME + 1];
  int ret;
  char *val;

#ifdef DEBUG
  OBMC_WARN("pal_post_display: status is %d\n", status);
#endif

  ret = pal_set_post_gpio_out();
  if (ret) {
    return -1;
  }

  snprintf(path, LARGEST_DEVICE_NAME, GPIO_POSTCODE_0, "value");
  if (BIT(status, 0)) {
    val = "1";
  } else {
    val = "0";
  }

  ret = write_device(path, val);
  if (ret) {
    goto post_exit;
  }

  snprintf(path, LARGEST_DEVICE_NAME, GPIO_POSTCODE_1, "value");
  if (BIT(status, 1)) {
    val = "1";
  } else {
    val = "0";
  }

  ret = write_device(path, val);
  if (ret) {
    goto post_exit;
  }

  snprintf(path, LARGEST_DEVICE_NAME, GPIO_POSTCODE_2, "value");
  if (BIT(status, 2)) {
    val = "1";
  } else {
    val = "0";
  }

  ret = write_device(path, val);
  if (ret) {
    goto post_exit;
  }

  snprintf(path, LARGEST_DEVICE_NAME, GPIO_POSTCODE_3, "value");
  if (BIT(status, 3)) {
    val = "1";
  } else {
    val = "0";
  }

  ret = write_device(path, val);
  if (ret) {
    goto post_exit;
  }

  snprintf(path, LARGEST_DEVICE_NAME, GPIO_POSTCODE_4, "value");
  if (BIT(status, 4)) {
    val = "1";
  } else {
    val = "0";
  }

  ret = write_device(path, val);
  if (ret) {
    goto post_exit;
  }

  snprintf(path, LARGEST_DEVICE_NAME, GPIO_POSTCODE_5, "value");
  if (BIT(status, 5)) {
    val = "1";
  } else {
    val = "0";
  }

  ret = write_device(path, val);
  if (ret) {
    goto post_exit;
  }

  snprintf(path, LARGEST_DEVICE_NAME, GPIO_POSTCODE_6, "value");
  if (BIT(status, 6)) {
    val = "1";
  } else {
    val = "0";
  }

  ret = write_device(path, val);
  if (ret) {
    goto post_exit;
  }

  snprintf(path, LARGEST_DEVICE_NAME, GPIO_POSTCODE_7, "value");
  if (BIT(status, 7)) {
    val = "1";
  } else {
    val = "0";
  }

  ret = write_device(path, val);
  if (ret) {
    goto post_exit;
  }

post_exit:
  if (ret) {
#ifdef DEBUG
    OBMC_WARN("write_device failed for %s\n", path);
#endif
    return -1;
  } else {
    return 0;
  }
}

// Handle the received post code, for now display it on debug card
int
pal_post_handle(uint8_t slot, uint8_t status) {
  uint8_t prsnt;
  int ret;

  // Check for debug card presence
  ret = pal_is_debug_card_prsnt(&prsnt);
  if (ret) {
    return ret;
  }

  // No debug card present, return
  if (!prsnt) {
    return 0;
  }

  // Display the post code in the debug card
  ret = pal_post_display(status);
  if (ret) {
    return ret;
  }

  return 0;
}

// Return the Front panel Power Button
int
pal_get_board_rev(int *rev) {
  char path[LARGEST_DEVICE_NAME + 1];
  int val_id_0, val_id_1, val_id_2;

  snprintf(path, LARGEST_DEVICE_NAME, GPIO_SMB_REV_ID_0, "value");
  if (read_device(path, &val_id_0)) {
    return -1;
  }

  snprintf(path, LARGEST_DEVICE_NAME, GPIO_SMB_REV_ID_1, "value");
  if (read_device(path, &val_id_1)) {
    return -1;
  }

  snprintf(path, LARGEST_DEVICE_NAME, GPIO_SMB_REV_ID_2, "value");
  if (read_device(path, &val_id_2)) {
    return -1;
  }

  *rev = val_id_0 | (val_id_1 << 1) | (val_id_2 << 2);

  return 0;
}

int
pal_get_board_type(uint8_t *brd_type){
  char path[LARGEST_DEVICE_NAME + 1];
  int val;
  snprintf(path, LARGEST_DEVICE_NAME, BRD_TYPE_FILE);
  if (read_device(path, &val)) {
    return CC_UNSPECIFIED_ERROR;
  }
  if(val == 0x0){
    *brd_type = BRD_TYPE_WEDGE400;
    return CC_SUCCESS;
  }else if(val == 0x1){
    *brd_type = BRD_TYPE_WEDGE400_2;
    return CC_SUCCESS;
  }else{
    return CC_UNSPECIFIED_ERROR;
  }
}

int
pal_get_board_id(uint8_t slot, uint8_t *req_data, uint8_t req_len, uint8_t *res_data, uint8_t *res_len)
{
	int board_id = 0, board_rev ;
	unsigned char *data = res_data;
	int completion_code = CC_UNSPECIFIED_ERROR;


  int i = 0, num_chips, num_pins;
  char device[64], path[32];
	gpiochip_desc_t *chips[GPIO_CHIP_MAX];
  gpiochip_desc_t *gcdesc;
  gpio_desc_t *gdesc;
  gpio_value_t value;

	num_chips = gpiochip_list(chips, ARRAY_SIZE(chips));
  if(num_chips < 0){
    *res_len = 0;
		return completion_code;
  }

  gcdesc = gpiochip_lookup(SCM_BRD_ID);
  if (gcdesc == NULL) {
    *res_len = 0;
		return completion_code;
  }

  num_pins = gpiochip_get_ngpio(gcdesc);
  gpiochip_get_device(gcdesc, device, sizeof(device));

  for(i = 0; i < num_pins; i++){
    sprintf(path, "%s%d", "BRD_ID_",i);
    gpio_export_by_offset(device,i,path);
    gdesc = gpio_open_by_shadow(path);
    if (gpio_get_value(gdesc, &value) == 0) {
      board_id  |= (((int)value)<<i);
    }
    gpio_unexport(path);
  }

	if(pal_get_board_rev(&board_rev) == -1){
    *res_len = 0;
		return completion_code;
  }

	*data++ = board_id;
	*data++ = board_rev;
	*data++ = slot;
	*data++ = 0x00; // 1S Server.
	*res_len = data - res_data;
	completion_code = CC_SUCCESS;

	return completion_code;
}

int
pal_get_cpld_board_rev(int *rev, const char *device) {
  char full_name[LARGEST_DEVICE_NAME + 1];

  snprintf(full_name, LARGEST_DEVICE_NAME, device, "board_ver");
  if (read_device(full_name, rev)) {
    return -1;
  }

  return 0;
}

int
pal_set_last_pwr_state(uint8_t fru, char *state) {

  int ret;
  char key[MAX_KEY_LEN];

  sprintf(key, "%s", "pwr_server_last_state");

  ret = pal_set_key_value(key, state);
  if (ret < 0) {
#ifdef DEBUG
    OBMC_WARN("pal_set_last_pwr_state: pal_set_key_value failed for "
        "fru %u", fru);
#endif
  }
  return ret;
}

int
pal_get_last_pwr_state(uint8_t fru, char *state) {
  int ret;
  char key[MAX_KEY_LEN];

  sprintf(key, "%s", "pwr_server_last_state");

  ret = pal_get_key_value(key, state);
  if (ret < 0) {
#ifdef DEBUG
    OBMC_WARN("pal_get_last_pwr_state: pal_get_key_value failed for "
      "fru %u", fru);
#endif
  }
  return ret;
}

int
pal_set_com_pwr_btn_n(char *status) {
  char path[64];
  int ret;
  sprintf(path, SCM_SYSFS, COM_PWR_BTN_N);

  ret = write_device(path, status);
  if (ret) {
#ifdef DEBUG
  OBMC_WARN("write_device failed for %s\n", path);
#endif
    return -1;
  }

  return 0;
}

int
pal_get_num_slots(uint8_t *num)
{
  *num = MAX_NUM_SCM;
  return PAL_EOK;
}

// Power On the COM-E
static int
scm_power_on(uint8_t slot_id) {
  int ret;

  ret = run_command("/usr/local/bin/wedge_power.sh on");
  if (ret)
    return -1;
  return 0;
}

// Power Off the COM-E
static int
scm_power_off(uint8_t slot_id) {
  int ret;

  ret = run_command("/usr/local/bin/wedge_power.sh off");
  if (ret)
    return -1;
  return 0;
}

// Power Button trigger the server in given slot
static int
cpu_power_btn(uint8_t slot_id) {
  int ret;

  ret = pal_set_com_pwr_btn_n("0");
  if (ret)
    return -1;
  sleep(DELAY_POWER_BTN);
  ret = pal_set_com_pwr_btn_n("1");
  if (ret)
    return -1;

  return 0;
}

int
pal_get_server_power(uint8_t slot_id, uint8_t *status) {

  int ret;
  char value[MAX_VALUE_LEN];
  bic_gpio_t gpio;
  uint8_t retry = MAX_READ_RETRY;

  /* check if the CPU is turned on or not */
  while (retry) {
    ret = bic_get_gpio(IPMB_BUS, &gpio);
    if (!ret)
      break;
    msleep(50);
    retry--;
  }
  if (ret) {
    // Check for if the BIC is irresponsive due to 12V_OFF or 12V_CYCLE
    OBMC_INFO("pal_get_server_power: bic_get_gpio returned error hence"
        " reading the kv_store for last power state  for fru %d", slot_id);
    pal_get_last_pwr_state(slot_id, value);
    if (!(strncmp(value, "off", strlen("off")))) {
      *status = SERVER_POWER_OFF;
    } else if (!(strncmp(value, "on", strlen("on")))) {
      *status = SERVER_POWER_ON;
    } else {
      return ret;
    }
    return 0;
  }

  if (gpio.pwrgood_cpu) {
    *status = SERVER_POWER_ON;
  } else {
    *status = SERVER_POWER_OFF;
  }

  return 0;
}

// Power Off, Power On, or Power Reset the server in given slot
int
pal_set_server_power(uint8_t slot_id, uint8_t cmd) {
  int ret;
  uint8_t status;

  if (pal_get_server_power(slot_id, &status) < 0) {
    return -1;
  }

  switch(cmd) {
    case SERVER_POWER_ON:
      if (status == SERVER_POWER_ON)
        return 1;
      else
        return scm_power_on(slot_id);
      break;

    case SERVER_POWER_OFF:
      if (status == SERVER_POWER_OFF)
        return 1;
      else
        return scm_power_off(slot_id);
      break;

    case SERVER_POWER_CYCLE:
      if (status == SERVER_POWER_ON) {
        if (scm_power_off(slot_id))
          return -1;

        sleep(DELAY_POWER_CYCLE);

        return scm_power_on(slot_id);

      } else if (status == SERVER_POWER_OFF) {

        return (scm_power_on(slot_id));
      }
      break;

    case SERVER_POWER_RESET:
      if (status == SERVER_POWER_ON) {
        ret = cpu_power_btn(slot_id);
        if (ret != 0)
          return ret;

        sleep(DELAY_POWER_CYCLE);

        ret = cpu_power_btn(slot_id);
        if (ret != 0)
          return ret;
      } else if (status == SERVER_POWER_OFF) {
        printf("Ignore to execute power reset action when the \
                power status of server is off\n");
        return -2;
      }
      break;

    case SERVER_GRACEFUL_SHUTDOWN:
      if (status == SERVER_POWER_OFF) {
        return 1;
      } else {
        return scm_power_off(slot_id);
      }
      break;

    default:
      return -1;
  }

  return 0;
}

static bool
is_server_on(void) {
  int ret;
  uint8_t status;
  ret = pal_get_server_power(FRU_SCM, &status);
  if (ret) {
    return false;
  }

  if (status == SERVER_POWER_ON) {
    return true;
  } else {
    return false;
  }
}

int
pal_set_th3_power(int option) {
  char path[64];
  int ret;
  uint8_t brd_type;
  char sysfs[32];
  if(pal_get_board_type(&brd_type)){
    return -1;
  }

  if(brd_type == BRD_TYPE_WEDGE400){
    sprintf(sysfs,TH3_POWER);
  }else if(brd_type == BRD_TYPE_WEDGE400_2){
    sprintf(sysfs,GB_POWER);
  }else{
    return -1;
  }

  switch(option) {
    case TH3_POWER_ON:
      sprintf(path, SMB_SYSFS, sysfs);
      ret = write_device(path, "1");
      break;
    case TH3_POWER_OFF:
      sprintf(path, SMB_SYSFS, sysfs);
      ret = write_device(path, "0");
      break;
    case TH3_RESET:
      sprintf(path, SMB_SYSFS, sysfs);
      ret = write_device(path, "0");
      sleep(1);
      ret = write_device(path, "1");
      break;
    default:
      ret = -1;
  }
  if (ret)
    return -1;
  return 0;
}

static int check_dir_exist(const char *device) {
  char cmd[LARGEST_DEVICE_NAME + 1];
  FILE *fp;
  char cmd_ret;
  int ret=-1;

  // Check if device exists
  snprintf(cmd, LARGEST_DEVICE_NAME, "[ -e %s ];echo $?", device);
  fp = popen(cmd, "r");
  if(NULL == fp)
     return -1;

  cmd_ret = fgetc(fp);
  ret = pclose(fp);
  if(-1 == ret)
    return -1;
  if('0' != cmd_ret) {
    return -1;
  }

  return 0;
}

static int
get_current_dir(const char *device, char *dir_name) {
  char cmd[LARGEST_DEVICE_NAME + 1];
  FILE *fp;
  int ret=-1;
  int size;

  // Get current working directory
  snprintf(
      cmd, LARGEST_DEVICE_NAME, "cd %s;pwd", device);

  fp = popen(cmd, "r");
  if(NULL == fp)
     return -1;
  fgets(dir_name, LARGEST_DEVICE_NAME, fp);

  ret = pclose(fp);
  if(-1 == ret)
     OBMC_ERROR(-1, "%s pclose() fail ", __func__);

  // Remove the newline character at the end
  size = strlen(dir_name);
  dir_name[size-1] = '\0';

  return 0;
}

static int
read_attr_integer(const char *device, const char *attr, int *value) {
  char full_name[LARGEST_DEVICE_NAME + 1];
  char dir_name[LARGEST_DEVICE_NAME + 1];

  // Get current working directory
  if (get_current_dir(device, dir_name)) {
    return -1;
  }

  snprintf(
      full_name, LARGEST_DEVICE_NAME, "%s/%s", dir_name, attr);

  if (read_device(full_name, value)) {
    return -1;
  }

  return 0;
}

static int
read_attr(const char *device, const char *attr, float *value) {
  char full_name[LARGEST_DEVICE_NAME + 1];
  char dir_name[LARGEST_DEVICE_NAME + 1];
  int tmp;

  // Get current working directory
  if (get_current_dir(device, dir_name)) {
    return -1;
  }

  snprintf(
      full_name, LARGEST_DEVICE_NAME, "%s/%s", dir_name, attr);

  if (read_device(full_name, &tmp)) {
    return -1;
  }

  *value = ((float)tmp)/UNIT_DIV;

  return 0;
}

static int
read_hsc_attr(const char *device,
              const char* attr, float r_sense, float *value) {
  char full_dir_name[LARGEST_DEVICE_NAME];
  char dir_name[LARGEST_DEVICE_NAME + 1];
  int tmp;

  // Get current working directory
  if (get_current_dir(device, dir_name))
  {
    return -1;
  }
  snprintf(
      full_dir_name, LARGEST_DEVICE_NAME, "%s/%s", dir_name, attr);

  if (read_device(full_dir_name, &tmp)) {
    return -1;
  }

  *value = ((float)tmp)/r_sense/UNIT_DIV;

  return 0;
}

static int
read_hsc_volt(const char *device, float r_sense, float *value) {
  return read_hsc_attr(device, VOLT(1), r_sense, value);
}

static int
read_hsc_curr(const char *device, float r_sense, float *value) {
  return read_hsc_attr(device, CURR(1), r_sense, value);
}

static int
read_hsc_power(const char *device, float r_sense, float *value) {
  return read_hsc_attr(device, POWER(1), r_sense, value);
}

static int
read_fan_rpm_f(const char *device, uint8_t fan, float *value) {
  char full_name[LARGEST_DEVICE_NAME + 1];
  char dir_name[LARGEST_DEVICE_NAME + 1];
  char device_name[11];
  int tmp;

  /* Get current working directory */
  if (get_current_dir(device, dir_name)) {
    return -1;
  }

  snprintf(device_name, 11, "fan%d_input", fan);
  snprintf(full_name, LARGEST_DEVICE_NAME, "%s/%s", dir_name, device_name);
  if (read_device(full_name, &tmp)) {
    return -1;
  }

  *value = (float)tmp;

  return 0;
}

static int
read_fan_rpm(const char *device, uint8_t fan, int *value) {
  char full_name[LARGEST_DEVICE_NAME + 1];
  char dir_name[LARGEST_DEVICE_NAME + 1];
  char device_name[11];
  int tmp;

  /* Get current working directory */
  if (get_current_dir(device, dir_name)) {
    return -1;
  }

  snprintf(device_name, 11, "fan%d_input", fan);
  snprintf(full_name, LARGEST_DEVICE_NAME, "%s/%s", dir_name, device_name);
  if (read_device(full_name, &tmp)) {
    return -1;
  }

  *value = tmp;

  return 0;
}

int
pal_get_fan_speed(uint8_t fan, int *rpm) {
  if (fan >= MAX_NUM_FAN * 2) {
    OBMC_INFO("get_fan_speed: invalid fan#:%d", fan);
    return -1;
  }

  return read_fan_rpm(SMB_FCM_TACH_DEVICE, (fan + 1), rpm);
}

static int
bic_sensor_sdr_path(uint8_t fru, char *path) {

  char fru_name[16];

  switch(fru) {
    case FRU_SCM:
      sprintf(fru_name, "%s", "scm");
      break;
    default:
  #ifdef DEBUG
      OBMC_WARN("bic_sensor_sdr_path: Wrong Slot ID\n");
  #endif
      return -1;
  }

  sprintf(path, WEDGE400_SDR_PATH, fru_name);

  if (access(path, F_OK) == -1) {
    return -1;
  }

  return 0;
}

/* Populates all sensor_info_t struct using the path to SDR dump */
static int
sdr_init(char *path, sensor_info_t *sinfo) {
  int fd;
  int ret = 0;
  uint8_t buf[MAX_SDR_LEN] = {0};
  uint8_t bytes_rd = 0;
  uint8_t snr_num = 0;
  sdr_full_t *sdr;

  while (access(path, F_OK) == -1) {
    sleep(5);
  }

  fd = open(path, O_RDONLY);
  if (fd < 0) {
    OBMC_ERROR(fd, "%s: open failed for %s\n", __func__, path);
    return -1;
  }

  ret = pal_flock_retry(fd);
  if (ret == -1) {
   OBMC_WARN("%s: failed to flock on %s", __func__, path);
   close(fd);
   return -1;
  }

  while ((bytes_rd = read(fd, buf, sizeof(sdr_full_t))) > 0) {
    if (bytes_rd != sizeof(sdr_full_t)) {
      OBMC_ERROR(bytes_rd, "%s: read returns %d bytes\n", __func__, bytes_rd);
      pal_unflock_retry(fd);
      close(fd);
      return -1;
    }

    sdr = (sdr_full_t *) buf;
    snr_num = sdr->sensor_num;
    sinfo[snr_num].valid = true;
    memcpy(&sinfo[snr_num].sdr, sdr, sizeof(sdr_full_t));
  }

  ret = pal_unflock_retry(fd);
  if (ret == -1) {
    OBMC_WARN("%s: failed to unflock on %s", __func__, path);
    close(fd);
    return -1;
  }

  close(fd);
  return 0;
}

static int
bic_sensor_sdr_init(uint8_t fru, sensor_info_t *sinfo) {
  char path[64];
  int retry = 0;

  switch(fru) {
    case FRU_SCM:
      if (bic_sensor_sdr_path(fru, path) < 0) {
#ifdef DEBUG
        OBMC_WARN(
               "bic_sensor_sdr_path: get_fru_sdr_path failed\n");
#endif
        return ERR_NOT_READY;
      }
      while (retry <= 3) {
        if (sdr_init(path, sinfo) < 0) {
          if (retry == 3) { //if the third retry still failed, return -1
#ifdef DEBUG
            OBMC_ERROR(-1, "bic_sensor_sdr_init: sdr_init failed for FRU %d", fru);
#endif
            return -1;
          }
          retry++;
          sleep(1);
        } else {
          break;
        }
      }
      break;
  }

  return 0;
}

static int
bic_sdr_init(uint8_t fru) {

  static bool init_done[MAX_NUM_FRUS] = {false};

  if (!init_done[fru - 1]) {

    sensor_info_t *sinfo = g_sinfo[fru-1];

    if (bic_sensor_sdr_init(fru, sinfo) < 0)
      return ERR_NOT_READY;

    init_done[fru - 1] = true;
  }

  return 0;
}

/* Get the threshold values from the SDRs */
static int
bic_get_sdr_thresh_val(uint8_t fru, uint8_t snr_num,
                       uint8_t thresh, void *value) {
  int ret, retry = 0;
  int8_t b_exp, r_exp;
  uint8_t x, m_lsb, m_msb, b_lsb, b_msb, thresh_val;
  uint16_t m = 0, b = 0;
  sensor_info_t sinfo[MAX_SENSOR_NUM] = {0};
  sdr_full_t *sdr;

  while ((ret = bic_sensor_sdr_init(fru, sinfo)) == ERR_NOT_READY &&
    retry++ < MAX_RETRIES_SDR_INIT) {
    sleep(1);
  }
  if (ret < 0) {
    OBMC_WARN("bic_get_sdr_thresh_val: failed for fru: %d", fru);
    return -1;
  }
  sdr = &sinfo[snr_num].sdr;

  switch (thresh) {
    case UCR_THRESH:
      thresh_val = sdr->uc_thresh;
      break;
    case UNC_THRESH:
      thresh_val = sdr->unc_thresh;
      break;
    case UNR_THRESH:
      thresh_val = sdr->unr_thresh;
      break;
    case LCR_THRESH:
      thresh_val = sdr->lc_thresh;
      break;
    case LNC_THRESH:
      thresh_val = sdr->lnc_thresh;
      break;
    case LNR_THRESH:
      thresh_val = sdr->lnr_thresh;
      break;
    case POS_HYST:
      thresh_val = sdr->pos_hyst;
      break;
    case NEG_HYST:
      thresh_val = sdr->neg_hyst;
      break;
    default:
#ifdef DEBUG
      OBMC_ERROR(-1, "bic_get_sdr_thresh_val: reading unknown threshold val");
#endif
      return -1;
  }

  // y = (mx + b * 10^b_exp) * 10^r_exp
  x = thresh_val;

  m_lsb = sdr->m_val;
  m_msb = sdr->m_tolerance >> 6;
  m = (m_msb << 8) | m_lsb;

  b_lsb = sdr->b_val;
  b_msb = sdr->b_accuracy >> 6;
  b = (b_msb << 8) | b_lsb;

  // exponents are 2's complement 4-bit number
  b_exp = sdr->rb_exp & 0xF;
  if (b_exp > 7) {
    b_exp = (~b_exp + 1) & 0xF;
    b_exp = -b_exp;
  }

  r_exp = (sdr->rb_exp >> 4) & 0xF;
  if (r_exp > 7) {
    r_exp = (~r_exp + 1) & 0xF;
    r_exp = -r_exp;
  }
  * (float *) value = ((m * x) + (b * pow(10, b_exp))) * (pow(10, r_exp));

  return 0;
}

static int
bic_read_sensor_wrapper(uint8_t fru, uint8_t sensor_num, bool discrete,
    void *value) {

  int ret, i;
  ipmi_sensor_reading_t sensor;
  sdr_full_t *sdr;

  ret = bic_read_sensor(IPMB_BUS, sensor_num, &sensor);
  if (ret) {
    return ret;
  }

  if (sensor.flags & BIC_SENSOR_READ_NA) {
#ifdef DEBUG
    OBMC_ERROR(-1, "bic_read_sensor_wrapper: Reading Not Available");
    OBMC_ERROR(-1, "bic_read_sensor_wrapper: sensor_num: 0x%X, flag: 0x%X",
        sensor_num, sensor.flags);
#endif
    return -1;
  }

  if (discrete) {
    *(float *) value = (float) sensor.status;
    return 0;
  }

  sdr = &g_sinfo[fru-1][sensor_num].sdr;

  // If the SDR is not type1, no need for conversion
  if (sdr->type !=1) {
    *(float *) value = sensor.value;
    return 0;
  }

  // y = (mx + b * 10^b_exp) * 10^r_exp
  uint8_t x;
  uint8_t m_lsb, m_msb;
  uint16_t m = 0;
  uint8_t b_lsb, b_msb;
  uint16_t b = 0;
  int8_t b_exp, r_exp;

  x = sensor.value;

  m_lsb = sdr->m_val;
  m_msb = sdr->m_tolerance >> 6;
  m = (m_msb << 8) | m_lsb;

  b_lsb = sdr->b_val;
  b_msb = sdr->b_accuracy >> 6;
  b = (b_msb << 8) | b_lsb;

  // exponents are 2's complement 4-bit number
  b_exp = sdr->rb_exp & 0xF;
  if (b_exp > 7) {
    b_exp = (~b_exp + 1) & 0xF;
    b_exp = -b_exp;
  }
  r_exp = (sdr->rb_exp >> 4) & 0xF;
  if (r_exp > 7) {
    r_exp = (~r_exp + 1) & 0xF;
    r_exp = -r_exp;
  }

  * (float *) value = ((m * x) + (b * pow(10, b_exp))) * (pow(10, r_exp));

  if ((sensor_num == BIC_SENSOR_SOC_THERM_MARGIN) && (* (float *) value > 0)) {
   * (float *) value -= (float) THERMAL_CONSTANT;
  }

  if (*(float *) value > MAX_POS_READING_MARGIN) {     //Negative reading handle
    for(i=0;i<sizeof(bic_neg_reading_sensor_support_list)/sizeof(uint8_t);i++) {
      if (sensor_num == bic_neg_reading_sensor_support_list[i]) {
        * (float *) value -= (float) THERMAL_CONSTANT;
      }
    }
  }

  return 0;
}

static void
hsc_rsense_init(uint8_t hsc_id, const char* device) {
  static bool rsense_inited[MAX_NUM_FRUS] = {false};

  if (!rsense_inited[hsc_id]) {
    int brd_rev = -1;

    pal_get_cpld_board_rev(&brd_rev, device);
    /* R0D or R0E FCM */
    if (brd_rev == 0x4 || brd_rev == 0x5) {
      hsc_rsense[hsc_id] = 1;
    } else {
      hsc_rsense[hsc_id] = 1;
    }

    rsense_inited[hsc_id] = true;
  }
}

static int
scm_sensor_read(uint8_t sensor_num, float *value) {

  int ret = -1;
  int i = 0;
  int j = 0;
  bool discrete = false;
  bool scm_sensor = false;

  while (i < scm_sensor_cnt) {
    if (sensor_num == scm_sensor_list[i++]) {
      scm_sensor = true;
      break;
    }
  }
  if (scm_sensor) {
    switch(sensor_num) {
      case SCM_SENSOR_OUTLET_TEMP:
        ret = read_attr(SCM_OUTLET_TEMP_DEVICE, TEMP(1), value);
        break;
      case SCM_SENSOR_INLET_TEMP:
        ret = read_attr(SCM_INLET_TEMP_DEVICE, TEMP(1), value);
        break;
      case SCM_SENSOR_HSC_VOLT:
        ret = read_hsc_volt(SCM_HSC_DEVICE, 1, value);
        break;
      case SCM_SENSOR_HSC_CURR:
        ret = read_hsc_curr(SCM_HSC_DEVICE, SCM_RSENSE, value);
        *value = *value * 1.0036 - 0.1189;
        break;
      case SCM_SENSOR_HSC_POWER:
        ret = read_hsc_power(SCM_HSC_DEVICE, SCM_RSENSE, value);
        *value = *value / 1000 * 0.86;
        break;
      default:
        ret = READING_NA;
        break;
    }
  } else if (!scm_sensor && is_server_on()) {
    ret = bic_sdr_init(FRU_SCM);
    if (ret < 0) {
#ifdef DEBUG
    OBMC_INFO("bic_sdr_init fail\n");
#endif
      return ret;
    }

    while (j < bic_discrete_cnt) {
      if (sensor_num == bic_discrete_list[j++]) {
        discrete = true;
        break;
      }
    }
    ret = bic_read_sensor_wrapper(FRU_SCM, sensor_num, discrete, value);
  } else {
    ret = READING_NA;
  }
  return ret;
}

static int
cor_th3_volt(void) {
  int tmp_volt, i;
  int val_volt = 0;
  char str[32];
  char tmp[LARGEST_DEVICE_NAME];
  char path[LARGEST_DEVICE_NAME + 1];
  snprintf(tmp, LARGEST_DEVICE_NAME, SMB_SYSFS, SMB_MAC_CPLD_ROV);

  for(i = SMB_MAC_CPLD_ROV_NUM - 1; i >= 0; i--) {
    snprintf(path, LARGEST_DEVICE_NAME, tmp, i);
    if(read_device(path, &tmp_volt)) {
      OBMC_ERROR(-1, "%s, Cannot read th3 voltage from smbcpld\n", __func__);
      return -1;
    }
    val_volt += tmp_volt;
    if(i)
      val_volt = (val_volt << 1);
  }
  val_volt = (int)(round((1.6 - (((double)val_volt - 3) * 0.00625)) * 1000));

  if(val_volt > TH3_VOL_MAX || val_volt < TH3_VOL_MIN)
    return -1;

  snprintf(str, sizeof(str), "%d", val_volt);
  snprintf(path, LARGEST_DEVICE_NAME, SMB_ISL_DEVICE"/%s", VOLT_SET(2));
  if(write_device(path, str)) {
    OBMC_ERROR(-1, "%s, Cannot write th3 voltage into ISL68127\n", __func__);
    return -1;
  }

  return 0;
}

static int
smb_sensor_read(uint8_t sensor_num, float *value) {

  int ret = -1, th3_ret = -1;
  static uint8_t bootup_check = 0;
  uint8_t brd_type;
  pal_get_board_type(&brd_type);
  switch(sensor_num) {
    case SMB_SENSOR_SW_SERDES_PVDD_TEMP1:
      ret = read_attr(SMB_SW_SERDES_PVDD_DEVICE, TEMP(2), value);
      break;
    case SMB_SENSOR_SW_SERDES_TRVDD_TEMP1:
      ret = read_attr(SMB_SW_SERDES_TRVDD_DEVICE, TEMP(2), value);
      break;
    case SMB_SENSOR_SW_CORE_TEMP1:
      ret = read_attr(SMB_ISL_DEVICE, TEMP(1), value);
      break;
    case SMB_SENSOR_TEMP1:
      ret = read_attr(SMB_TEMP1_DEVICE, TEMP(1), value);
      break;
    case SMB_SENSOR_TEMP2:
      ret = read_attr(SMB_TEMP2_DEVICE, TEMP(1), value);
      break;
    case SMB_SENSOR_TEMP3:
      ret = read_attr(SMB_TEMP3_DEVICE, TEMP(1), value);
      break;
    case SMB_SENSOR_TEMP4:
      ret = read_attr(SMB_TEMP4_DEVICE, TEMP(1), value);
      break;
    case SMB_SENSOR_TEMP5:
      ret = read_attr(SMB_TEMP5_DEVICE, TEMP(1), value);
      break;
    case SMB_SENSOR_TEMP6:
      ret = read_attr(SMB_TEMP6_DEVICE, TEMP(1), value);
      break;
    case SMB_SENSOR_SW_DIE_TEMP1:
      ret = read_attr(SMB_SW_TEMP_DEVICE, TEMP(2), value);
      break;
    case SMB_SENSOR_SW_DIE_TEMP2:
      ret = read_attr(SMB_SW_TEMP_DEVICE, TEMP(3), value);
      break;
    case SMB_SENSOR_FCM_TEMP1:
      ret = read_attr(SMB_FCM_TEMP1_DEVICE, TEMP(1), value);
      break;
    case SMB_SENSOR_FCM_TEMP2:
      ret = read_attr(SMB_FCM_TEMP2_DEVICE, TEMP(1), value);
      break;
    case SMB_SENSOR_1220_VMON1:
      ret = read_attr(SMB_1220_DEVICE, VOLT(0), value);
      *value *= 4.3;
      break;
    case SMB_SENSOR_1220_VMON2:
      ret = read_attr(SMB_1220_DEVICE, VOLT(1), value);
      break;
    case SMB_SENSOR_1220_VMON3:
      ret = read_attr(SMB_1220_DEVICE, VOLT(2), value);
      break;
    case SMB_SENSOR_1220_VMON4:
      ret = read_attr(SMB_1220_DEVICE, VOLT(3), value);
      break;
    case SMB_SENSOR_1220_VMON5:
      ret = read_attr(SMB_1220_DEVICE, VOLT(4), value);
      break;
    case SMB_SENSOR_1220_VMON6:
      ret = read_attr(SMB_1220_DEVICE, VOLT(5), value);
      break;
    case SMB_SENSOR_1220_VMON7:
      ret = read_attr(SMB_1220_DEVICE, VOLT(6), value);
      break;
    case SMB_SENSOR_1220_VMON8:
      ret = read_attr(SMB_1220_DEVICE, VOLT(7), value);
      break;
    case SMB_SENSOR_1220_VMON9:
      ret = read_attr(SMB_1220_DEVICE, VOLT(8), value);
      break;
    case SMB_SENSOR_1220_VMON10:
      ret = read_attr(SMB_1220_DEVICE, VOLT(9), value);
      break;
    case SMB_SENSOR_1220_VMON11:
      ret = read_attr(SMB_1220_DEVICE, VOLT(10), value);
      break;
    case SMB_SENSOR_1220_VMON12:
      ret = read_attr(SMB_1220_DEVICE, VOLT(11), value);
      break;
    case SMB_SENSOR_1220_VCCA:
      ret = read_attr(SMB_1220_DEVICE, VOLT(12), value);
      break;
    case SMB_SENSOR_1220_VCCINP:
      ret = read_attr(SMB_1220_DEVICE, VOLT(13), value);
      break;
    case SMB_SENSOR_SW_SERDES_PVDD_VOLT:
      if( brd_type == BRD_TYPE_WEDGE400 ){
        ret = read_attr(SMB_SW_SERDES_PVDD_DEVICE, VOLT(3), value);
      }
      if( brd_type == BRD_TYPE_WEDGE400_2 ){
        ret = read_attr(SMB_SW_SERDES_PVDD_DEVICE, VOLT(2), value);
        *value *= 2;
      }
      break;
    case SMB_SENSOR_SW_SERDES_TRVDD_VOLT:
      if( brd_type == BRD_TYPE_WEDGE400 ){
        ret = read_attr(SMB_SW_SERDES_TRVDD_DEVICE, VOLT(3), value);
      }
      if( brd_type == BRD_TYPE_WEDGE400_2 ){
        ret = read_attr(SMB_SW_SERDES_TRVDD_DEVICE, VOLT(2), value);
        *value *= 2;
      }
      break;
    case SMB_SENSOR_SW_CORE_VOLT:
      if( brd_type == BRD_TYPE_WEDGE400 ){
        ret = read_attr(SMB_ISL_DEVICE, VOLT(2), value);
        int board_rev = -1;
        if((pal_get_board_rev(&board_rev) != -1) && (board_rev != 4)) {
          if (bootup_check == 0) {
            th3_ret = cor_th3_volt();
            if (!th3_ret)
              bootup_check = 1;
          }
        }
      }
      else if( brd_type == BRD_TYPE_WEDGE400_2 ){
        ret = read_attr(SMB_XPDE_DEVICE, VOLT(2), value);
      }
      break;
    case SMB_SENSOR_FCM_HSC_VOLT:
      ret = read_hsc_volt(SMB_FCM_HSC_DEVICE, 1, value);
      break;
    case SMB_SENSOR_SW_SERDES_PVDD_CURR:
      ret = read_attr(SMB_SW_SERDES_PVDD_DEVICE, CURR(3), value);
      *value = *value * 1.0433 + 0.3926;
      break;
    case SMB_SENSOR_SW_SERDES_TRVDD_CURR:
      ret = read_attr(SMB_SW_SERDES_TRVDD_DEVICE, CURR(3), value);
      *value = *value * 0.9994 + 1.0221;
      break;
    case SMB_SENSOR_SW_CORE_CURR:
      if( brd_type == BRD_TYPE_WEDGE400 ){
        ret = read_attr(SMB_ISL_DEVICE,  CURR(2), value);
      }
      else if( brd_type == BRD_TYPE_WEDGE400_2 ){
        ret = read_attr(SMB_XPDE_DEVICE, CURR(2), value);
      }
      break;
    case SMB_SENSOR_FCM_HSC_CURR:
      hsc_rsense_init(HSC_FCM, FCM_SYSFS);
      ret = read_hsc_curr(SMB_FCM_HSC_DEVICE, hsc_rsense[HSC_FCM], value);
      *value = *value * 4.4254 - 0.2048;
      break;
    case SMB_SENSOR_SW_SERDES_PVDD_POWER:
      ret = read_attr(SMB_SW_SERDES_PVDD_DEVICE,  POWER(3), value);
      *value /= 1000;
      break;
    case SMB_SENSOR_SW_SERDES_TRVDD_POWER:
      ret = read_attr(SMB_SW_SERDES_TRVDD_DEVICE,  POWER(3), value);
      *value /= 1000;
      break;
    case SMB_SENSOR_SW_CORE_POWER:
      if( brd_type == BRD_TYPE_WEDGE400 ){
        ret = read_attr(SMB_ISL_DEVICE,  POWER(2), value);
      }
      else if( brd_type == BRD_TYPE_WEDGE400_2 ){
        ret = read_attr(SMB_XPDE_DEVICE, POWER(2), value);
      }
      *value /= 1000;
      break;
    case SMB_SENSOR_FCM_HSC_POWER:
      hsc_rsense_init(HSC_FCM, FCM_SYSFS);
      ret = read_hsc_power(SMB_FCM_HSC_DEVICE, hsc_rsense[HSC_FCM], value);
      *value = *value / 1000 * 3.03;
      break;
    case SMB_SENSOR_FAN1_FRONT_TACH:
      ret = read_fan_rpm_f(SMB_FCM_TACH_DEVICE, 1, value);
      break;
    case SMB_SENSOR_FAN1_REAR_TACH:
      ret = read_fan_rpm_f(SMB_FCM_TACH_DEVICE, 2, value);
      break;
    case SMB_SENSOR_FAN2_FRONT_TACH:
      ret = read_fan_rpm_f(SMB_FCM_TACH_DEVICE, 1, value);
      break;
    case SMB_SENSOR_FAN2_REAR_TACH:
      ret = read_fan_rpm_f(SMB_FCM_TACH_DEVICE, 2, value);
      break;
    case SMB_SENSOR_FAN3_FRONT_TACH:
      ret = read_fan_rpm_f(SMB_FCM_TACH_DEVICE, 3, value);
      break;
    case SMB_SENSOR_FAN3_REAR_TACH:
      ret = read_fan_rpm_f(SMB_FCM_TACH_DEVICE, 4, value);
      break;
    case SMB_SENSOR_FAN4_FRONT_TACH:
      ret = read_fan_rpm_f(SMB_FCM_TACH_DEVICE, 3, value);
      break;
    case SMB_SENSOR_FAN4_REAR_TACH:
      ret = read_fan_rpm_f(SMB_FCM_TACH_DEVICE, 4, value);
      break;
    default:
      ret = READING_NA;
      break;
  }
  return ret;
}

static int
pem_sensor_read(uint8_t sensor_num, void *value) {

  int ret = -1;

  switch(sensor_num) {
    case PEM1_SENSOR_IN_VOLT:
      ret = read_attr(PEM1_DEVICE, VOLT(1), value);
      break;
    case PEM1_SENSOR_OUT_VOLT:
      ret = read_attr(PEM1_DEVICE, VOLT(2), value);
      break;
    case PEM1_SENSOR_CURR:
      ret = read_attr(PEM1_DEVICE, CURR(1), value);
      break;
    case PEM1_SENSOR_POWER:
      ret = read_attr(PEM1_DEVICE, POWER(1), value);
      *(float *)value /= 1000;
      break;
    case PEM1_SENSOR_TEMP1:
      ret = read_attr(PEM1_DEVICE, TEMP(1), value);
      break;
    case PEM1_SENSOR_FAN1_TACH:
      ret = read_fan_rpm_f(PEM1_DEVICE_EXT, 1, value);
      break;
    case PEM1_SENSOR_FAN2_TACH:
      ret = read_fan_rpm_f(PEM1_DEVICE_EXT, 2, value);
      break;
    case PEM1_SENSOR_TEMP2:
      ret = read_attr(PEM1_DEVICE_EXT, TEMP(1), value);
      break;
    case PEM1_SENSOR_TEMP3:
      ret = read_attr(PEM1_DEVICE_EXT, TEMP(2), value);
      break;
    case PEM1_SENSOR_FAULT_OV:
      ret = read_attr_integer(PEM1_DEVICE, "fault_ov", value);
      break;
    case PEM1_SENSOR_FAULT_UV:
      ret = read_attr_integer(PEM1_DEVICE, "fault_uv", value);
      break;
    case PEM1_SENSOR_FAULT_OC:
      ret = read_attr_integer(PEM1_DEVICE, "fault_oc", value);
      break;
    case PEM1_SENSOR_FAULT_POWER:
      ret = read_attr_integer(PEM1_DEVICE, "fault_power", value);
      break;
    case PEM1_SENSOR_ON_FAULT:
      ret = read_attr_integer(PEM1_DEVICE, "on_fault", value);
      break;
    case PEM1_SENSOR_FAULT_FET_SHORT:
      ret = read_attr_integer(PEM1_DEVICE, "fault_fet_short", value);
      break;
    case PEM1_SENSOR_FAULT_FET_BAD:
      ret = read_attr_integer(PEM1_DEVICE, "fault_fet_bad", value);
      break;
    case PEM1_SENSOR_EEPROM_DONE:
      ret = read_attr_integer(PEM1_DEVICE, "eeprom_done", value);
      break;
    case PEM1_SENSOR_POWER_ALARM_HIGH:
      ret = read_attr_integer(PEM1_DEVICE, "power_alarm_high", value);
      break;
    case PEM1_SENSOR_POWER_ALARM_LOW:
      ret = read_attr_integer(PEM1_DEVICE, "power_alarm_low", value);
      break;
    case PEM1_SENSOR_VSENSE_ALARM_HIGH:
      ret = read_attr_integer(PEM1_DEVICE, "vsense_alarm_high", value);
      break;
    case PEM1_SENSOR_VSENSE_ALARM_LOW:
      ret = read_attr_integer(PEM1_DEVICE, "vsense_alarm_low", value);
      break;
    case PEM1_SENSOR_VSOURCE_ALARM_HIGH:
      ret = read_attr_integer(PEM1_DEVICE, "vsource_alarm_high", value);
      break;
    case PEM1_SENSOR_VSOURCE_ALARM_LOW:
      ret = read_attr_integer(PEM1_DEVICE, "vsource_alarm_low", value);
      break;
    case PEM1_SENSOR_GPIO_ALARM_HIGH:
      ret = read_attr_integer(PEM1_DEVICE, "gpio_alarm_high", value);
      break;
    case PEM1_SENSOR_GPIO_ALARM_LOW:
      ret = read_attr_integer(PEM1_DEVICE, "gpio_alarm_low", value);
      break;
    case PEM1_SENSOR_ON_STATUS:
      ret = read_attr_integer(PEM1_DEVICE, "on_status", value);
      break;
    case PEM1_SENSOR_STATUS_FET_BAD:
      ret = read_attr_integer(PEM1_DEVICE, "fet_bad", value);
      break;
    case PEM1_SENSOR_STATUS_FET_SHORT:
      ret = read_attr_integer(PEM1_DEVICE, "fet_short", value);
      break;
    case PEM1_SENSOR_STATUS_ON_PIN:
      ret = read_attr_integer(PEM1_DEVICE, "on_pin_status", value);
      break;
    case PEM1_SENSOR_STATUS_POWER_GOOD:
      ret = read_attr_integer(PEM1_DEVICE, "power_good", value);
      break;
    case PEM1_SENSOR_STATUS_OC:
      ret = read_attr_integer(PEM1_DEVICE, "oc_status", value);
      break;
    case PEM1_SENSOR_STATUS_UV:
      ret = read_attr_integer(PEM1_DEVICE, "uv_status", value);
      break;
    case PEM1_SENSOR_STATUS_OV:
      ret = read_attr_integer(PEM1_DEVICE, "ov_status", value);
      break;
    case PEM1_SENSOR_STATUS_GPIO3:
      ret = read_attr_integer(PEM1_DEVICE, "gpio3_status", value);
      break;
    case PEM1_SENSOR_STATUS_GPIO2:
      ret = read_attr_integer(PEM1_DEVICE, "gpio2_status", value);
      break;
    case PEM1_SENSOR_STATUS_GPIO1:
      ret = read_attr_integer(PEM1_DEVICE, "gpio1_status", value);
      break;
    case PEM1_SENSOR_STATUS_ALERT:
      ret = read_attr_integer(PEM1_DEVICE, "alert_status", value);
      break;
    case PEM1_SENSOR_STATUS_EEPROM_BUSY:
      ret = read_attr_integer(PEM1_DEVICE, "eeprom_busy", value);
      break;
    case PEM1_SENSOR_STATUS_ADC_IDLE:
      ret = read_attr_integer(PEM1_DEVICE, "adc_idle", value);
      break;
    case PEM1_SENSOR_STATUS_TICKER_OVERFLOW:
      ret = read_attr_integer(PEM1_DEVICE, "ticker_overflow", value);
      break;
    case PEM1_SENSOR_STATUS_METER_OVERFLOW:
      ret = read_attr_integer(PEM1_DEVICE, "meter_overflow", value);
      break;

    case PEM2_SENSOR_IN_VOLT:
      ret = read_attr(PEM2_DEVICE, VOLT(1), value);
      break;
    case PEM2_SENSOR_OUT_VOLT:
      ret = read_attr(PEM2_DEVICE, VOLT(2), value);
      break;
    case PEM2_SENSOR_CURR:
      ret = read_attr(PEM2_DEVICE, CURR(1), value);
      break;
    case PEM2_SENSOR_POWER:
      ret = read_attr(PEM2_DEVICE, POWER(1), value);
      *(float *)value /= 1000;
      break;
    case PEM2_SENSOR_TEMP1:
      ret = read_attr(PEM2_DEVICE, TEMP(1), value);
      break;
    case PEM2_SENSOR_FAN1_TACH:
      ret = read_fan_rpm_f(PEM2_DEVICE_EXT, 1, value);
      break;
    case PEM2_SENSOR_FAN2_TACH:
      ret = read_fan_rpm_f(PEM2_DEVICE_EXT, 2, value);
      break;
    case PEM2_SENSOR_TEMP2:
      ret = read_attr(PEM2_DEVICE_EXT, TEMP(1), value);
      break;
    case PEM2_SENSOR_TEMP3:
      ret = read_attr(PEM2_DEVICE_EXT, TEMP(2), value);
      break;
    case PEM2_SENSOR_FAULT_OV:
      ret = read_attr_integer(PEM2_DEVICE, "fault_ov", value);
      break;
    case PEM2_SENSOR_FAULT_UV:
      ret = read_attr_integer(PEM2_DEVICE, "fault_uv", value);
      break;
    case PEM2_SENSOR_FAULT_OC:
      ret = read_attr_integer(PEM2_DEVICE, "fault_oc", value);
      break;
    case PEM2_SENSOR_FAULT_POWER:
      ret = read_attr_integer(PEM2_DEVICE, "fault_power", value);
      break;
    case PEM2_SENSOR_ON_FAULT:
      ret = read_attr_integer(PEM2_DEVICE, "on_fault", value);
      break;
    case PEM2_SENSOR_FAULT_FET_SHORT:
      ret = read_attr_integer(PEM2_DEVICE, "fault_fet_short", value);
      break;
    case PEM2_SENSOR_FAULT_FET_BAD:
      ret = read_attr_integer(PEM2_DEVICE, "fault_fet_bad", value);
      break;
    case PEM2_SENSOR_EEPROM_DONE:
      ret = read_attr_integer(PEM2_DEVICE, "eeprom_done", value);
      break;
    case PEM2_SENSOR_POWER_ALARM_HIGH:
      ret = read_attr_integer(PEM2_DEVICE, "power_alrm_high", value);
      break;
    case PEM2_SENSOR_POWER_ALARM_LOW:
      ret = read_attr_integer(PEM2_DEVICE, "power_alrm_low", value);
      break;
    case PEM2_SENSOR_VSENSE_ALARM_HIGH:
      ret = read_attr_integer(PEM2_DEVICE, "vsense_alrm_high", value);
      break;
    case PEM2_SENSOR_VSENSE_ALARM_LOW:
      ret = read_attr_integer(PEM2_DEVICE, "vsense_alrm_low", value);
      break;
    case PEM2_SENSOR_VSOURCE_ALARM_HIGH:
      ret = read_attr_integer(PEM2_DEVICE, "vsource_alrm_high", value);
      break;
    case PEM2_SENSOR_VSOURCE_ALARM_LOW:
      ret = read_attr_integer(PEM2_DEVICE, "vsource_alrm_low", value);
      break;
    case PEM2_SENSOR_GPIO_ALARM_HIGH:
      ret = read_attr_integer(PEM2_DEVICE, "gpio_alrm_high", value);
      break;
    case PEM2_SENSOR_GPIO_ALARM_LOW:
      ret = read_attr_integer(PEM2_DEVICE, "gpio_alrm_low", value);
      break;
    case PEM2_SENSOR_ON_STATUS:
      ret = read_attr_integer(PEM2_DEVICE, "on_status", value);
      break;
    case PEM2_SENSOR_STATUS_FET_BAD:
      ret = read_attr_integer(PEM2_DEVICE, "fet_bad", value);
      break;
    case PEM2_SENSOR_STATUS_FET_SHORT:
      ret = read_attr_integer(PEM2_DEVICE, "fet_short", value);
      break;
    case PEM2_SENSOR_STATUS_ON_PIN:
      ret = read_attr_integer(PEM2_DEVICE, "on_pin_status", value);
      break;
    case PEM2_SENSOR_STATUS_POWER_GOOD:
      ret = read_attr_integer(PEM2_DEVICE, "power_good", value);
      break;
    case PEM2_SENSOR_STATUS_OC:
      ret = read_attr_integer(PEM2_DEVICE, "oc_status", value);
      break;
    case PEM2_SENSOR_STATUS_UV:
      ret = read_attr_integer(PEM2_DEVICE, "uv_status", value);
      break;
    case PEM2_SENSOR_STATUS_OV:
      ret = read_attr_integer(PEM2_DEVICE, "ov_status", value);
      break;
    case PEM2_SENSOR_STATUS_GPIO3:
      ret = read_attr_integer(PEM2_DEVICE, "gpio3_status", value);
      break;
    case PEM2_SENSOR_STATUS_GPIO2:
      ret = read_attr_integer(PEM2_DEVICE, "gpio2_status", value);
      break;
    case PEM2_SENSOR_STATUS_GPIO1:
      ret = read_attr_integer(PEM2_DEVICE, "gpio1_status", value);
      break;
    case PEM2_SENSOR_STATUS_ALERT:
      ret = read_attr_integer(PEM2_DEVICE, "alert_status", value);
      break;
    case PEM2_SENSOR_STATUS_EEPROM_BUSY:
      ret = read_attr_integer(PEM2_DEVICE, "eeprom_busy", value);
      break;
    case PEM2_SENSOR_STATUS_ADC_IDLE:
      ret = read_attr_integer(PEM2_DEVICE, "adc_idle", value);
      break;
    case PEM2_SENSOR_STATUS_TICKER_OVERFLOW:
      ret = read_attr_integer(PEM2_DEVICE, "ticker_overflow", value);
      break;
    case PEM2_SENSOR_STATUS_METER_OVERFLOW:
      ret = read_attr_integer(PEM2_DEVICE, "meter_overflow", value);
      break;

    default:
      ret = READING_NA;
      break;
  }
  return ret;
}

static int
psu_sensor_read(uint8_t sensor_num, float *value) {

  int ret = -1;

  switch(sensor_num) {
    case PSU1_SENSOR_IN_VOLT:
      ret = read_attr(PSU1_DEVICE, VOLT(0), value);
      break;
    case PSU1_SENSOR_12V_VOLT:
      ret = read_attr(PSU1_DEVICE, VOLT(1), value);
      break;
    case PSU1_SENSOR_STBY_VOLT:
      ret = read_attr(PSU1_DEVICE, VOLT(2), value);
      break;
    case PSU1_SENSOR_IN_CURR:
      ret = read_attr(PSU1_DEVICE, CURR(1), value);
      break;
    case PSU1_SENSOR_12V_CURR:
      ret = read_attr(PSU1_DEVICE, CURR(2), value);
      break;
    case PSU1_SENSOR_STBY_CURR:
      ret = read_attr(PSU1_DEVICE, CURR(3), value);
      break;
    case PSU1_SENSOR_IN_POWER:
      ret = read_attr(PSU1_DEVICE, POWER(1), value);
      break;
    case PSU1_SENSOR_12V_POWER:
      ret = read_attr(PSU1_DEVICE, POWER(2), value);
      break;
    case PSU1_SENSOR_STBY_POWER:
      ret = read_attr(PSU1_DEVICE, POWER(3), value);
      break;
    case PSU1_SENSOR_FAN_TACH:
      ret = read_fan_rpm_f(PSU1_DEVICE, 1, value);
      break;
    case PSU1_SENSOR_TEMP1:
      ret = read_attr(PSU1_DEVICE, TEMP(1), value);
      break;
    case PSU1_SENSOR_TEMP2:
      ret = read_attr(PSU1_DEVICE, TEMP(2), value);
      break;
    case PSU1_SENSOR_TEMP3:
      ret = read_attr(PSU1_DEVICE, TEMP(3), value);
      break;
    case PSU2_SENSOR_IN_VOLT:
      ret = read_attr(PSU2_DEVICE, VOLT(0), value);
      break;
    case PSU2_SENSOR_12V_VOLT:
      ret = read_attr(PSU2_DEVICE, VOLT(1), value);
      break;
    case PSU2_SENSOR_STBY_VOLT:
      ret = read_attr(PSU2_DEVICE, VOLT(2), value);
      break;
    case PSU2_SENSOR_IN_CURR:
      ret = read_attr(PSU2_DEVICE, CURR(1), value);
      break;
    case PSU2_SENSOR_12V_CURR:
      ret = read_attr(PSU2_DEVICE, CURR(2), value);
      break;
    case PSU2_SENSOR_STBY_CURR:
      ret = read_attr(PSU2_DEVICE, CURR(3), value);
      break;
    case PSU2_SENSOR_IN_POWER:
      ret = read_attr(PSU2_DEVICE, POWER(1), value);
      break;
    case PSU2_SENSOR_12V_POWER:
      ret = read_attr(PSU2_DEVICE, POWER(2), value);
      break;
    case PSU2_SENSOR_STBY_POWER:
      ret = read_attr(PSU2_DEVICE, POWER(3), value);
      break;
    case PSU2_SENSOR_FAN_TACH:
      ret = read_fan_rpm_f(PSU2_DEVICE, 1, value);
      break;
    case PSU2_SENSOR_TEMP1:
      ret = read_attr(PSU2_DEVICE, TEMP(1), value);
      break;
    case PSU2_SENSOR_TEMP2:
      ret = read_attr(PSU2_DEVICE, TEMP(2), value);
      break;
    case PSU2_SENSOR_TEMP3:
      ret = read_attr(PSU2_DEVICE, TEMP(3), value);
      break;
    default:
      ret = READING_NA;
      break;
  }
  return ret;
}

int
pal_sensor_read_raw(uint8_t fru, uint8_t sensor_num, void *value) {

  char key[MAX_KEY_LEN];
  char fru_name[32];
  int ret , delay = 500;
  uint8_t prsnt = 0;
  uint8_t status = 0;

  pal_get_fru_name(fru, fru_name);

  ret = pal_is_fru_prsnt(fru, &prsnt);
  if (ret) {
    return ret;
  }
  if (!prsnt) {
#ifdef DEBUG
  OBMC_INFO("pal_sensor_read_raw(): %s is not present\n", fru_name);
#endif
    return -1;
  }

  ret = pal_is_fru_ready(fru, &status);
  if (ret) {
    return ret;
  }
  if (!status) {
#ifdef DEBUG
  OBMC_INFO("pal_sensor_read_raw(): %s is not ready\n", fru_name);
#endif
    return -1;
  }

  sprintf(key, "%s_sensor%d", fru_name, sensor_num);
  switch(fru) {
    case FRU_SCM:
      ret = scm_sensor_read(sensor_num, value);
      if (sensor_num == SCM_SENSOR_INLET_TEMP) {
        delay = 100;
      }
      break;
    case FRU_SMB:
      ret = smb_sensor_read(sensor_num, value);
      if (sensor_num == SMB_SENSOR_SW_DIE_TEMP1 ||
          sensor_num == SMB_SENSOR_SW_DIE_TEMP2 ||
          (sensor_num >= SMB_SENSOR_FAN1_FRONT_TACH &&
           sensor_num <= SMB_SENSOR_FAN4_REAR_TACH)) {
        delay = 100;
      }
      break;
    case FRU_PEM1:
    case FRU_PEM2:
      ret = pem_sensor_read(sensor_num, value);
      break;
    case FRU_PSU1:
    case FRU_PSU2:
      ret = psu_sensor_read(sensor_num, value);
      break;
    default:
      return -1;
  }

  if (ret == READING_NA || ret == -1) {
    return READING_NA;
  }
  msleep(delay);

  return 0;
}

int
pal_sensor_discrete_read_raw(uint8_t fru, uint8_t sensor_num, void *value) {

  char key[MAX_KEY_LEN];
  char fru_name[32];
  int ret , delay = 500;
  uint8_t prsnt = 0;
  uint8_t status = 0;

  pal_get_fru_name(fru, fru_name);

  ret = pal_is_fru_prsnt(fru, &prsnt);
  if (ret) {
    return ret;
  }
  if (!prsnt) {
#ifdef DEBUG
  OBMC_INFO("pal_sensor_discrete_read_raw(): %s is not present\n", fru_name);
#endif
    return -1;
  }

  ret = pal_is_fru_ready(fru, &status);
  if (ret) {
    return ret;
  }
  if (!status) {
#ifdef DEBUG
  OBMC_INFO("pal_sensor_discrete_read_raw(): %s is not ready\n", fru_name);
#endif
    return -1;
  }

  sprintf(key, "%s_sensor%d", fru_name, sensor_num);
  switch(fru) {
    case FRU_PEM1:
    case FRU_PEM2:
      ret = pem_sensor_read(sensor_num, value);
      break;

    default:
      return -1;
  }

  if (ret == READING_NA || ret == -1) {
    return READING_NA;
  }
  msleep(delay);

  return 0;
};

static int
get_scm_sensor_name(uint8_t sensor_num, char *name) {

  switch(sensor_num) {
    case SCM_SENSOR_OUTLET_TEMP:
      sprintf(name, "SCM_OUTLET_TEMP");
      break;
    case SCM_SENSOR_INLET_TEMP:
      sprintf(name, "SCM_INLET_TEMP");
      break;
    case SCM_SENSOR_HSC_VOLT:
      sprintf(name, "SCM_HSC_VOLT");
      break;
    case SCM_SENSOR_HSC_CURR:
      sprintf(name, "SCM_HSC_CURR");
      break;
    case SCM_SENSOR_HSC_POWER:
      sprintf(name, "SCM_HSC_POWER");
      break;
    case BIC_SENSOR_MB_OUTLET_TEMP:
      sprintf(name, "MB_OUTLET_TEMP");
      break;
    case BIC_SENSOR_MB_INLET_TEMP:
      sprintf(name, "MB_INLET_TEMP");
      break;
    case BIC_SENSOR_PCH_TEMP:
      sprintf(name, "PCH_TEMP");
      break;
    case BIC_SENSOR_VCCIN_VR_TEMP:
      sprintf(name, "VCCIN_VR_TEMP");
      break;
    case BIC_SENSOR_1V05COMB_VR_TEMP:
      sprintf(name, "1V05COMB_VR_TEMP");
      break;
    case BIC_SENSOR_SOC_TEMP:
      sprintf(name, "SOC_TEMP");
      break;
    case BIC_SENSOR_SOC_THERM_MARGIN:
      sprintf(name, "SOC_THERM_MARGIN_TEMP");
      break;
    case BIC_SENSOR_VDDR_VR_TEMP:
      sprintf(name, "VDDR_VR_TEMP");
      break;
    case BIC_SENSOR_SOC_DIMMA_TEMP:
      sprintf(name, "SOC_DIMMA_TEMP");
      break;
    case BIC_SENSOR_SOC_DIMMB_TEMP:
      sprintf(name, "SOC_DIMMB_TEMP");
      break;
    case BIC_SENSOR_SOC_PACKAGE_PWR:
      sprintf(name, "SOC_PACKAGE_POWER");
      break;
    case BIC_SENSOR_VCCIN_VR_POUT:
      sprintf(name, "VCCIN_VR_OUT_POWER");
      break;
      case BIC_SENSOR_VDDR_VR_POUT:
      sprintf(name, "VDDR_VR_OUT_POWER");
      break;
    case BIC_SENSOR_SOC_TJMAX:
      sprintf(name, "SOC_TJMAX_TEMP");
      break;
    case BIC_SENSOR_P3V3_MB:
      sprintf(name, "P3V3_MB_VOLT");
      break;
    case BIC_SENSOR_P12V_MB:
      sprintf(name, "P12V_MB_VOLT");
      break;
    case BIC_SENSOR_P1V05_PCH:
      sprintf(name, "P1V05_PCH_VOLT");
      break;
    case BIC_SENSOR_P3V3_STBY_MB:
      sprintf(name, "P3V3_STBY_MB_VOLT");
      break;
    case BIC_SENSOR_P5V_STBY_MB:
      sprintf(name, "P5V_STBY_MB_VOLT");
      break;
    case BIC_SENSOR_PV_BAT:
      sprintf(name, "PV_BAT_VOLT");
      break;
    case BIC_SENSOR_PVDDR:
      sprintf(name, "PVDDR_VOLT");
      break;
    case BIC_SENSOR_P1V05_COMB:
      sprintf(name, "P1V05_COMB_VOLT");
      break;
    case BIC_SENSOR_1V05COMB_VR_CURR:
      sprintf(name, "1V05COMB_VR_CURR");
      break;
    case BIC_SENSOR_VDDR_VR_CURR:
      sprintf(name, "VDDR_VR_CURR");
      break;
    case BIC_SENSOR_VCCIN_VR_CURR:
      sprintf(name, "VCCIN_VR_CURR");
      break;
    case BIC_SENSOR_VCCIN_VR_VOL:
      sprintf(name, "VCCIN_VR_VOLT");
      break;
    case BIC_SENSOR_VDDR_VR_VOL:
      sprintf(name, "VDDR_VR_VOLT");
      break;
    case BIC_SENSOR_P1V05COMB_VR_VOL:
      sprintf(name, "1V05COMB_VR_VOLT");
      break;
    case BIC_SENSOR_P1V05COMB_VR_POUT:
      sprintf(name, "1V05COMB_VR_OUT_POWER");
      break;
    case BIC_SENSOR_INA230_POWER:
      sprintf(name, "INA230_POWER");
      break;
    case BIC_SENSOR_INA230_VOL:
      sprintf(name, "INA230_VOLT");
      break;
    case BIC_SENSOR_SYSTEM_STATUS:
      sprintf(name, "SYSTEM_STATUS");
      break;
    case BIC_SENSOR_SYS_BOOT_STAT:
      sprintf(name, "SYS_BOOT_STAT");
      break;
    case BIC_SENSOR_CPU_DIMM_HOT:
      sprintf(name, "CPU_DIMM_HOT");
      break;
    case BIC_SENSOR_PROC_FAIL:
      sprintf(name, "PROC_FAIL");
      break;
    case BIC_SENSOR_VR_HOT:
      sprintf(name, "VR_HOT");
      break;
    default:
      return -1;
  }
  return 0;
}

static int
get_smb_sensor_name(uint8_t sensor_num, char *name) {
  uint8_t brd_type;
  if(pal_get_board_type(&brd_type)){
    return -1;
  }
  switch(sensor_num) {
    case SMB_SENSOR_SW_SERDES_PVDD_TEMP1:
      if(brd_type == BRD_TYPE_WEDGE400){
        sprintf(name, "TH3_SERDES_TEMP1(PVDD)");
      }else if(brd_type == BRD_TYPE_WEDGE400_2){
        sprintf(name, "GB_SERDES_TEMP1(PVDD)");
      }
      break;
    case SMB_SENSOR_SW_SERDES_TRVDD_TEMP1:
      if(brd_type == BRD_TYPE_WEDGE400){
        sprintf(name, "TH3_SERDES_TEMP1(TRVDD)");
      }else if(brd_type == BRD_TYPE_WEDGE400_2){
        sprintf(name, "GB_SERDES_TEMP1(TRVDD)");
      }
      break;
    case SMB_SENSOR_SW_CORE_TEMP1:
      if(brd_type == BRD_TYPE_WEDGE400){
        sprintf(name, "TH3_CORE_TEMP1");
      }else if(brd_type == BRD_TYPE_WEDGE400_2){
        sprintf(name, SENSOR_NAME_ERR);
      }
      break;
    case SMB_SENSOR_TEMP1:
      sprintf(name, "SMB_TEMP1");
      break;
    case SMB_SENSOR_TEMP2:
      sprintf(name, "SMB_TEMP2");
      break;
    case SMB_SENSOR_TEMP3:
      sprintf(name, "SMB_TEMP3");
      break;
    case SMB_SENSOR_TEMP4:
      sprintf(name, "SMB_TEMP4");
      break;
    case SMB_SENSOR_TEMP5:
      sprintf(name, "SMB_TEMP5");
      break;
    case SMB_SENSOR_TEMP6:
      sprintf(name, "SMB_TEMP6");
      break;
    case SMB_SENSOR_SW_DIE_TEMP1:
      if(brd_type == BRD_TYPE_WEDGE400){
        sprintf(name, "TH3_DIE_TEMP1");
      }else if(brd_type == BRD_TYPE_WEDGE400_2){
        sprintf(name, SENSOR_NAME_ERR);
      }
      break;
    case SMB_SENSOR_SW_DIE_TEMP2:
      if(brd_type == BRD_TYPE_WEDGE400){
        sprintf(name, "TH3_DIE_TEMP2");
      }else if(brd_type == BRD_TYPE_WEDGE400_2){
        sprintf(name, SENSOR_NAME_ERR);
      }
      break;
    case SMB_SENSOR_FCM_TEMP1:
      sprintf(name, "FCM_TEMP1");
      break;
    case SMB_SENSOR_FCM_TEMP2:
      sprintf(name, "FCM_TEMP2");
      break;
    case SMB_SENSOR_1220_VMON1:
    if(brd_type == BRD_TYPE_WEDGE400){
        sprintf(name, "XP12R0V (12V)");
      }else if(brd_type == BRD_TYPE_WEDGE400_2){
        sprintf(name, "PWR12R0V (12V)");
      }
      break;
    case SMB_SENSOR_1220_VMON2:
      sprintf(name, "XP5R0V (5V)");
      break;
    case SMB_SENSOR_1220_VMON3:
      sprintf(name, "XP3R3V_BMC (3.3V)");
      break;
    case SMB_SENSOR_1220_VMON4:
      if(brd_type == BRD_TYPE_WEDGE400){
        sprintf(name, "XP2R5V_BMC (2.5V)");
      }else if(brd_type == BRD_TYPE_WEDGE400_2){
        sprintf(name, "XP3R3V_FPGA (3.3V)");
      }
      break;
    case SMB_SENSOR_1220_VMON5:
      sprintf(name, "XP1R2V_BMC (1.2V)");
      break;
    case SMB_SENSOR_1220_VMON6:
      if(brd_type == BRD_TYPE_WEDGE400){
        sprintf(name, "XP1R15V_BMC (1.15V)");
      }else if(brd_type == BRD_TYPE_WEDGE400_2){
        sprintf(name, "XP1R8V_FPGA (1.8V)");
      }
      break;
    case SMB_SENSOR_1220_VMON7:
      if(brd_type == BRD_TYPE_WEDGE400){
        sprintf(name, "XP1R2V_TH3 (1.2V)");
      }else if(brd_type == BRD_TYPE_WEDGE400_2){
        sprintf(name, "XP1R8V_IO (1.8V)");
      }
      break;
    case SMB_SENSOR_1220_VMON8:
      if(brd_type == BRD_TYPE_WEDGE400){
        sprintf(name, "PVDD0P8_TH3 (0.8V)");
      }else if(brd_type == BRD_TYPE_WEDGE400_2){
        sprintf(name, "XP2R5V_HBM (2.5V)");
      }
      break;
    case SMB_SENSOR_1220_VMON9:
      if(brd_type == BRD_TYPE_WEDGE400){
        sprintf(name, "XP3R3V_TH3 (3.3V)");
      }else if(brd_type == BRD_TYPE_WEDGE400_2){
        sprintf(name, "XP0R94V_VDDA (0.94V)");
      }
      break;
    case SMB_SENSOR_1220_VMON10:
      if(brd_type == BRD_TYPE_WEDGE400){
        sprintf(name, "VDD_CORE_TH3 (0.75~0.9V)");
      }else if(brd_type == BRD_TYPE_WEDGE400_2){
        sprintf(name, "VDD_CORE_GB (0.85V)");
      }
      break;
    case SMB_SENSOR_1220_VMON11:
      if(brd_type == BRD_TYPE_WEDGE400){
        sprintf(name, "TRVDD0P8_TH3 (0.8V)");
      }else if(brd_type == BRD_TYPE_WEDGE400_2){
        sprintf(name, "XP0R75V_PCIE (0.75V)");
      }
      break;
    case SMB_SENSOR_1220_VMON12:
      if(brd_type == BRD_TYPE_WEDGE400){
        sprintf(name, "XP1R8V_TH3 (1.8V)");
      }else if(brd_type == BRD_TYPE_WEDGE400_2){
        sprintf(name, "XP1R15V_VDDCK (1.15V)");
      }
      break;
    case SMB_SENSOR_1220_VCCA:
      sprintf(name, "POWR1220 VCCA (3.3V)");
      break;
    case SMB_SENSOR_1220_VCCINP:
      sprintf(name, "POWR1220 VCCINP (3.3V)");
      break;
    case SMB_SENSOR_SW_SERDES_PVDD_VOLT:
      if(brd_type == BRD_TYPE_WEDGE400){
        sprintf(name, "TH3_SERDES_VOLT(PVDD)");
      }else if(brd_type == BRD_TYPE_WEDGE400_2){
        sprintf(name, "XP3R3V_RIGHT_VOLT");
      }
      break;
    case SMB_SENSOR_SW_SERDES_TRVDD_VOLT:
      if(brd_type == BRD_TYPE_WEDGE400){
        sprintf(name, "TH3_SERDES_VOLT(TRVDD)");
      }else if(brd_type == BRD_TYPE_WEDGE400_2){
        sprintf(name, "XP3R3V_LEFT_VOLT");
      }
      break;
    case SMB_SENSOR_SW_CORE_VOLT:
      if(brd_type == BRD_TYPE_WEDGE400){
        sprintf(name, "TH3_CORE_VOLT");
      }else if(brd_type == BRD_TYPE_WEDGE400_2){
        sprintf(name, "VDD_CORE_VOLT");
      }
      break;
    case SMB_SENSOR_FCM_HSC_VOLT:
      sprintf(name, "FCM_HSC_VOLT");
      break;
    case SMB_SENSOR_SW_SERDES_PVDD_CURR:
      if(brd_type == BRD_TYPE_WEDGE400){
        sprintf(name, "TH3_SERDES_CURR(PVDD)");
      }else if(brd_type == BRD_TYPE_WEDGE400_2){
        sprintf(name, "XP3R3V_RIGHT_CURR");
      }
      break;
    case SMB_SENSOR_SW_SERDES_TRVDD_CURR:
      if(brd_type == BRD_TYPE_WEDGE400){
        sprintf(name, "TH3_SERDES_CURR(TRVDD)");
      }else if(brd_type == BRD_TYPE_WEDGE400_2){
        sprintf(name, "XP3R3V_LEFT_CURR");
      }
      break;
    case SMB_SENSOR_SW_CORE_CURR:
      if(brd_type == BRD_TYPE_WEDGE400){
        sprintf(name, "TH3_CORE_CURR");
      }else if(brd_type == BRD_TYPE_WEDGE400_2){
        sprintf(name, "VDD_CORE_CURR");
      }
      break;
    case SMB_SENSOR_FCM_HSC_CURR:
      sprintf(name, "FCM_HSC_CURR");
      break;
    case SMB_SENSOR_SW_SERDES_PVDD_POWER:
      if(brd_type == BRD_TYPE_WEDGE400){
        sprintf(name, "TH3_SERDES_POWER(PVDD)");
      }else if(brd_type == BRD_TYPE_WEDGE400_2){
        sprintf(name, "XP3R3V_RIGHT_POWER");
      }
      break;
    case SMB_SENSOR_SW_SERDES_TRVDD_POWER:
      if(brd_type == BRD_TYPE_WEDGE400){
        sprintf(name, "TH3_SERDES_POWER(TRVDD)");
      }else if(brd_type == BRD_TYPE_WEDGE400_2){
        sprintf(name, "XP3R3V_LEFT_POWER");
      }
      break;
    case SMB_SENSOR_SW_CORE_POWER:
      if(brd_type == BRD_TYPE_WEDGE400){
        sprintf(name, "TH3_CORE_POWER");
      }else if(brd_type == BRD_TYPE_WEDGE400_2){
        sprintf(name, "VDD_CORE_POWER");
      }
      break;
    case SMB_SENSOR_FCM_HSC_POWER:
      sprintf(name, "FCM_HSC_POWER");
      break;
    case SMB_SENSOR_FAN1_FRONT_TACH:
      sprintf(name, "FAN1_FRONT_SPEED");
      break;
    case SMB_SENSOR_FAN1_REAR_TACH:
      sprintf(name, "FAN1_REAR_SPEED");
      break;
    case SMB_SENSOR_FAN2_FRONT_TACH:
      sprintf(name, "FAN2_FRONT_SPEED");
      break;
    case SMB_SENSOR_FAN2_REAR_TACH:
      sprintf(name, "FAN2_REAR_SPEED");
      break;
    case SMB_SENSOR_FAN3_FRONT_TACH:
      sprintf(name, "FAN3_FRONT_SPEED");
      break;
    case SMB_SENSOR_FAN3_REAR_TACH:
      sprintf(name, "FAN3_REAR_SPEED");
      break;
    case SMB_SENSOR_FAN4_FRONT_TACH:
      sprintf(name, "FAN4_FRONT_SPEED");
      break;
    case SMB_SENSOR_FAN4_REAR_TACH:
      sprintf(name, "FAN4_REAR_SPEED");
      break;
    default:
      return -1;
  }
  return 0;
}

static int
get_pem_sensor_name(uint8_t sensor_num, char *name) {

  switch(sensor_num) {
    case PEM1_SENSOR_IN_VOLT:
      sprintf(name, "PEM1_IN_VOLT");
      break;
    case PEM1_SENSOR_OUT_VOLT:
      sprintf(name, "PEM1_OUT_VOLT");
      break;
    case PEM1_SENSOR_CURR:
      sprintf(name, "PEM1_CURR");
      break;
    case PEM1_SENSOR_POWER:
      sprintf(name, "PEM1_POWER");
      break;
    case PEM1_SENSOR_FAN1_TACH:
      sprintf(name, "PEM1_FAN1_SPEED");
      break;
    case PEM1_SENSOR_FAN2_TACH:
      sprintf(name, "PEM1_FAN2_SPEED");
      break;
    case PEM1_SENSOR_TEMP1:
      sprintf(name, "PEM1_HOT_SWAP_TEMP");
      break;
    case PEM1_SENSOR_TEMP2:
      sprintf(name, "PEM1_AIR_INLET_TEMP");
      break;
    case PEM1_SENSOR_TEMP3:
      sprintf(name, "PEM1_AIR_OUTLET_TEMP");
      break;

    case PEM1_SENSOR_FAULT_OV:
      sprintf(name, "PEM1_FAULT_OV");
      break;
    case PEM1_SENSOR_FAULT_UV:
      sprintf(name, "PEM1_FAULT_UV");
      break;
    case PEM1_SENSOR_FAULT_OC:
      sprintf(name, "PEM1_FAULT_OC");
      break;
    case PEM1_SENSOR_FAULT_POWER:
      sprintf(name, "PEM1_FAULT_POWER");
      break;
    case PEM1_SENSOR_ON_FAULT:
      sprintf(name, "PEM1_FAULT_ON_PIN");
      break;
    case PEM1_SENSOR_FAULT_FET_SHORT:
      sprintf(name, "PEM1_FAULT_FET_SHORT");
      break;
    case PEM1_SENSOR_FAULT_FET_BAD:
      sprintf(name, "PEM1_FAULT_FET_BAD");
      break;
    case PEM1_SENSOR_EEPROM_DONE:
      sprintf(name, "PEM1_EEPROM_DONE");
      break;

    case PEM1_SENSOR_POWER_ALARM_HIGH:
      sprintf(name, "PEM1_POWER_ALARM_HIGH");
      break;
    case PEM1_SENSOR_POWER_ALARM_LOW:
      sprintf(name, "PEM1_POWER_ALARM_LOW");
      break;
    case PEM1_SENSOR_VSENSE_ALARM_HIGH:
      sprintf(name, "PEM1_VSENSE_ALARM_HIGH");
      break;
    case PEM1_SENSOR_VSENSE_ALARM_LOW:
      sprintf(name, "PEM1_VSENSE_ALARM_LOW");
      break;
    case PEM1_SENSOR_VSOURCE_ALARM_HIGH:
      sprintf(name, "PEM1_VSOURCE_ALARM_HIGH");
      break;
    case PEM1_SENSOR_VSOURCE_ALARM_LOW:
      sprintf(name, "PEM1_VSOURCE_ALARM_LOW");
      break;
    case PEM1_SENSOR_GPIO_ALARM_HIGH:
      sprintf(name, "PEM1_GPIO_ALARM_HIGH");
      break;
    case PEM1_SENSOR_GPIO_ALARM_LOW:
      sprintf(name, "PEM1_GPIO_ALARM_LOW");
      break;

    case PEM1_SENSOR_ON_STATUS:
      sprintf(name, "PEM1_ON_STATUS");
      break;
    case PEM1_SENSOR_STATUS_FET_BAD:
      sprintf(name, "PEM1_STATUS_FET_BAD");
      break;
    case PEM1_SENSOR_STATUS_FET_SHORT:
      sprintf(name, "PEM1_STATUS_FET_SHORT");
      break;
    case PEM1_SENSOR_STATUS_ON_PIN:
      sprintf(name, "PEM1_STATUS_ON_PIN");
      break;
    case PEM1_SENSOR_STATUS_POWER_GOOD:
      sprintf(name, "PEM1_STATUS_POWER_GOOD");
      break;
    case PEM1_SENSOR_STATUS_OC:
      sprintf(name, "PEM1_STATUS_OC");
      break;
    case PEM1_SENSOR_STATUS_UV:
      sprintf(name, "PEM1_STATUS_UV");
      break;
    case PEM1_SENSOR_STATUS_OV:
      sprintf(name, "PEM1_STATUS_OV");
      break;
    case PEM1_SENSOR_STATUS_GPIO3:
      sprintf(name, "PEM1_STATUS_GPIO3");
      break;
    case PEM1_SENSOR_STATUS_GPIO2:
      sprintf(name, "PEM1_STATUS_GPIO2");
      break;
    case PEM1_SENSOR_STATUS_GPIO1:
      sprintf(name, "PEM1_STATUS_GPIO1");
      break;
    case PEM1_SENSOR_STATUS_ALERT:
      sprintf(name, "PEM1_STATUS_ALERT");
      break;
    case PEM1_SENSOR_STATUS_EEPROM_BUSY:
      sprintf(name, "PEM1_STATUS_EEPROM_BUSY");
      break;
    case PEM1_SENSOR_STATUS_ADC_IDLE:
      sprintf(name, "PEM1_STATUS_ADC_IDLE");
      break;
    case PEM1_SENSOR_STATUS_TICKER_OVERFLOW:
      sprintf(name, "PEM1_STATUS_TICKER_OVERFLOW");
      break;
    case PEM1_SENSOR_STATUS_METER_OVERFLOW:
      sprintf(name, "PEM1_STATUS_METER_OVERFLOW");
      break;

    case PEM2_SENSOR_IN_VOLT:
      sprintf(name, "PEM2_IN_VOLT");
      break;
    case PEM2_SENSOR_OUT_VOLT:
      sprintf(name, "PEM2_OUT_VOLT");
      break;
    case PEM2_SENSOR_CURR:
      sprintf(name, "PEM2_CURR");
      break;
    case PEM2_SENSOR_POWER:
      sprintf(name, "PEM2_POWER");
      break;
    case PEM2_SENSOR_FAN1_TACH:
      sprintf(name, "PEM2_FAN1_SPEED");
      break;
    case PEM2_SENSOR_FAN2_TACH:
      sprintf(name, "PEM2_FAN2_SPEED");
      break;
    case PEM2_SENSOR_TEMP1:
      sprintf(name, "PEM2_HOT_SWAP_TEMP");
      break;
    case PEM2_SENSOR_TEMP2:
      sprintf(name, "PEM2_AIR_INLET_TEMP");
      break;
    case PEM2_SENSOR_TEMP3:
      sprintf(name, "PEM2_AIR_OUTLET_TEMP");
      break;

    case PEM2_SENSOR_FAULT_OV:
      sprintf(name, "PEM2_FAULT_OV");
      break;
    case PEM2_SENSOR_FAULT_UV:
      sprintf(name, "PEM2_FAULT_UV");
      break;
    case PEM2_SENSOR_FAULT_OC:
      sprintf(name, "PEM2_FAULT_OC");
      break;
    case PEM2_SENSOR_FAULT_POWER:
      sprintf(name, "PEM2_FAULT_POWER");
      break;
    case PEM2_SENSOR_ON_FAULT:
      sprintf(name, "PEM2_FAULT_ON_PIN");
      break;
    case PEM2_SENSOR_FAULT_FET_SHORT:
      sprintf(name, "PEM2_FAULT_FET_SHOET");
      break;
    case PEM2_SENSOR_FAULT_FET_BAD:
      sprintf(name, "PEM2_FAULT_FET_BAD");
      break;
    case PEM2_SENSOR_EEPROM_DONE:
      sprintf(name, "PEM2_EEPROM_DONE");
      break;

    case PEM2_SENSOR_POWER_ALARM_HIGH:
      sprintf(name, "PEM2_POWER_ALARM_HIGH");
      break;
    case PEM2_SENSOR_POWER_ALARM_LOW:
      sprintf(name, "PEM2_POWER_ALARM_LOW");
      break;
    case PEM2_SENSOR_VSENSE_ALARM_HIGH:
      sprintf(name, "PEM2_VSENSE_ALARM_HIGH");
      break;
    case PEM2_SENSOR_VSENSE_ALARM_LOW:
      sprintf(name, "PEM2_VSENSE_ALARM_LOW");
      break;
    case PEM2_SENSOR_VSOURCE_ALARM_HIGH:
      sprintf(name, "PEM2_VSOURCE_ALARM_HIGH");
      break;
    case PEM2_SENSOR_VSOURCE_ALARM_LOW:
      sprintf(name, "PEM2_VSOURCE_ALARM_LOW");
      break;
    case PEM2_SENSOR_GPIO_ALARM_HIGH:
      sprintf(name, "PEM2_GPIO_ALARM_HIGH");
      break;
    case PEM2_SENSOR_GPIO_ALARM_LOW:
      sprintf(name, "PEM2_GPIO_ALARM_LOW");
      break;

    case PEM2_SENSOR_ON_STATUS:
      sprintf(name, "PEM2_ON_STATUS");
      break;
    case PEM2_SENSOR_STATUS_FET_BAD:
      sprintf(name, "PEM2_STATUS_FET_BAD");
      break;
    case PEM2_SENSOR_STATUS_FET_SHORT:
      sprintf(name, "PEM2_STATUS_FET_SHORT");
      break;
    case PEM2_SENSOR_STATUS_ON_PIN:
      sprintf(name, "PEM2_STATUS_ON_PIN");
      break;
    case PEM2_SENSOR_STATUS_POWER_GOOD:
      sprintf(name, "PEM2_STATUS_POWER_GOOD");
      break;
    case PEM2_SENSOR_STATUS_OC:
      sprintf(name, "PEM2_STATUS_OC");
      break;
    case PEM2_SENSOR_STATUS_UV:
      sprintf(name, "PEM2_STATUS_UV");
      break;
    case PEM2_SENSOR_STATUS_OV:
      sprintf(name, "PEM2_STATUS_OV");
      break;
    case PEM2_SENSOR_STATUS_GPIO3:
      sprintf(name, "PEM2_STATUS_GPIO3");
      break;
    case PEM2_SENSOR_STATUS_GPIO2:
      sprintf(name, "PEM2_STATUS_GPIO2");
      break;
    case PEM2_SENSOR_STATUS_GPIO1:
      sprintf(name, "PEM2_STATUS_GPIO1");
      break;
    case PEM2_SENSOR_STATUS_ALERT:
      sprintf(name, "PEM2_STATUS_ALERT");
      break;
    case PEM2_SENSOR_STATUS_EEPROM_BUSY:
      sprintf(name, "PEM2_STATUS_EEPROM_BUSY");
      break;
    case PEM2_SENSOR_STATUS_ADC_IDLE:
      sprintf(name, "PEM2_STATUS_ADC_IDLE");
      break;
    case PEM2_SENSOR_STATUS_TICKER_OVERFLOW:
      sprintf(name, "PEM2_STATUS_TICKER_OVERFLOW");
      break;
    case PEM2_SENSOR_STATUS_METER_OVERFLOW:
      sprintf(name, "PEM2_STATUS_METER_OVERFLOW");
      break;
    default:
      return -1;
  }
  return 0;
}

static int
get_psu_sensor_name(uint8_t sensor_num, char *name) {

  switch(sensor_num) {
    case PSU1_SENSOR_IN_VOLT:
      sprintf(name, "PSU1_IN_VOLT");
      break;
    case PSU1_SENSOR_12V_VOLT:
      sprintf(name, "PSU1_12V_VOLT");
      break;
    case PSU1_SENSOR_STBY_VOLT:
      sprintf(name, "PSU1_STBY_VOLT");
      break;
    case PSU1_SENSOR_IN_CURR:
      sprintf(name, "PSU1_IN_CURR");
      break;
    case PSU1_SENSOR_12V_CURR:
      sprintf(name, "PSU1_12V_CURR");
      break;
    case PSU1_SENSOR_STBY_CURR:
      sprintf(name, "PSU1_STBY_CURR");
      break;
    case PSU1_SENSOR_IN_POWER:
      sprintf(name, "PSU1_IN_POWER");
      break;
    case PSU1_SENSOR_12V_POWER:
      sprintf(name, "PSU1_12V_POWER");
      break;
    case PSU1_SENSOR_STBY_POWER:
      sprintf(name, "PSU1_STBY_POWER");
      break;
    case PSU1_SENSOR_FAN_TACH:
      sprintf(name, "PSU1_FAN_SPEED");
      break;
    case PSU1_SENSOR_TEMP1:
      sprintf(name, "PSU1_TEMP1");
      break;
    case PSU1_SENSOR_TEMP2:
      sprintf(name, "PSU1_TEMP2");
      break;
    case PSU1_SENSOR_TEMP3:
      sprintf(name, "PSU1_TEMP3");
      break;
    case PSU2_SENSOR_IN_VOLT:
      sprintf(name, "PSU2_IN_VOLT");
      break;
    case PSU2_SENSOR_12V_VOLT:
      sprintf(name, "PSU2_12V_VOLT");
      break;
    case PSU2_SENSOR_STBY_VOLT:
      sprintf(name, "PSU2_STBY_VOLT");
      break;
    case PSU2_SENSOR_IN_CURR:
      sprintf(name, "PSU2_IN_CURR");
      break;
    case PSU2_SENSOR_12V_CURR:
      sprintf(name, "PSU2_12V_CURR");
      break;
    case PSU2_SENSOR_STBY_CURR:
      sprintf(name, "PSU2_STBY_CURR");
      break;
    case PSU2_SENSOR_IN_POWER:
      sprintf(name, "PSU2_IN_POWER");
      break;
    case PSU2_SENSOR_12V_POWER:
      sprintf(name, "PSU2_12V_POWER");
      break;
    case PSU2_SENSOR_STBY_POWER:
      sprintf(name, "PSU2_STBY_POWER");
      break;
    case PSU2_SENSOR_FAN_TACH:
      sprintf(name, "PSU2_FAN_SPEED");
      break;
    case PSU2_SENSOR_TEMP1:
      sprintf(name, "PSU2_TEMP1");
      break;
    case PSU2_SENSOR_TEMP2:
      sprintf(name, "PSU2_TEMP2");
      break;
    case PSU2_SENSOR_TEMP3:
      sprintf(name, "PSU2_TEMP3");
      break;
    default:
      return -1;
  }
  return 0;
}

int
pal_get_sensor_name(uint8_t fru, uint8_t sensor_num, char *name) {

  int ret = -1;

  switch(fru) {
    case FRU_SCM:
      ret = get_scm_sensor_name(sensor_num, name);
      break;
    case FRU_SMB:
      ret = get_smb_sensor_name(sensor_num, name);
      break;
    case FRU_PEM1:
    case FRU_PEM2:
      ret = get_pem_sensor_name(sensor_num, name);
      break;
    case FRU_PSU1:
    case FRU_PSU2:
      ret = get_psu_sensor_name(sensor_num, name);
      break;
    default:
      return -1;
  }
  return ret;
}

static int
get_scm_sensor_units(uint8_t sensor_num, char *units) {

  switch(sensor_num) {
    case SCM_SENSOR_OUTLET_TEMP:
    case SCM_SENSOR_INLET_TEMP:
    case BIC_SENSOR_MB_OUTLET_TEMP:
    case BIC_SENSOR_MB_INLET_TEMP:
    case BIC_SENSOR_PCH_TEMP:
    case BIC_SENSOR_VCCIN_VR_TEMP:
    case BIC_SENSOR_1V05COMB_VR_TEMP:
    case BIC_SENSOR_SOC_TEMP:
    case BIC_SENSOR_SOC_THERM_MARGIN:
    case BIC_SENSOR_VDDR_VR_TEMP:
    case BIC_SENSOR_SOC_DIMMA_TEMP:
    case BIC_SENSOR_SOC_DIMMB_TEMP:
    case BIC_SENSOR_SOC_TJMAX:
      sprintf(units, "C");
      break;
    case SCM_SENSOR_HSC_VOLT:
    case BIC_SENSOR_P3V3_MB:
    case BIC_SENSOR_P12V_MB:
    case BIC_SENSOR_P1V05_PCH:
    case BIC_SENSOR_P3V3_STBY_MB:
    case BIC_SENSOR_P5V_STBY_MB:
    case BIC_SENSOR_PV_BAT:
    case BIC_SENSOR_PVDDR:
    case BIC_SENSOR_P1V05_COMB:
    case BIC_SENSOR_VCCIN_VR_VOL:
    case BIC_SENSOR_VDDR_VR_VOL:
    case BIC_SENSOR_P1V05COMB_VR_VOL:
    case BIC_SENSOR_INA230_VOL:
      sprintf(units, "Volts");
      break;
    case SCM_SENSOR_HSC_CURR:
    case BIC_SENSOR_1V05COMB_VR_CURR:
    case BIC_SENSOR_VDDR_VR_CURR:
    case BIC_SENSOR_VCCIN_VR_CURR:
      sprintf(units, "Amps");
      break;
    case SCM_SENSOR_HSC_POWER:
    case BIC_SENSOR_SOC_PACKAGE_PWR:
    case BIC_SENSOR_VCCIN_VR_POUT:
    case BIC_SENSOR_VDDR_VR_POUT:
    case BIC_SENSOR_P1V05COMB_VR_POUT:
    case BIC_SENSOR_INA230_POWER:
      sprintf(units, "Watts");
      break;
    default:
      return -1;
  }
  return 0;
}

static int
get_smb_sensor_units(uint8_t sensor_num, char *units) {

  switch(sensor_num) {
    case SMB_SENSOR_SW_SERDES_PVDD_TEMP1:
    case SMB_SENSOR_SW_SERDES_TRVDD_TEMP1:
    case SMB_SENSOR_SW_CORE_TEMP1:
    case SMB_SENSOR_TEMP1:
    case SMB_SENSOR_TEMP2:
    case SMB_SENSOR_TEMP3:
    case SMB_SENSOR_TEMP4:
    case SMB_SENSOR_TEMP5:
    case SMB_SENSOR_TEMP6:
    case SMB_SENSOR_SW_DIE_TEMP1:
    case SMB_SENSOR_SW_DIE_TEMP2:
    case SMB_SENSOR_FCM_TEMP1:
    case SMB_SENSOR_FCM_TEMP2:
      sprintf(units, "C");
      break;
    case SMB_SENSOR_1220_VMON1:
    case SMB_SENSOR_1220_VMON2:
    case SMB_SENSOR_1220_VMON3:
    case SMB_SENSOR_1220_VMON4:
    case SMB_SENSOR_1220_VMON5:
    case SMB_SENSOR_1220_VMON6:
    case SMB_SENSOR_1220_VMON7:
    case SMB_SENSOR_1220_VMON8:
    case SMB_SENSOR_1220_VMON9:
    case SMB_SENSOR_1220_VMON10:
    case SMB_SENSOR_1220_VMON11:
    case SMB_SENSOR_1220_VMON12:
    case SMB_SENSOR_1220_VCCA:
    case SMB_SENSOR_1220_VCCINP:
    case SMB_SENSOR_SW_SERDES_PVDD_VOLT:
    case SMB_SENSOR_SW_SERDES_TRVDD_VOLT:
    case SMB_SENSOR_SW_CORE_VOLT:
    case SMB_SENSOR_FCM_HSC_VOLT:
      sprintf(units, "Volts");
      break;
    case SMB_SENSOR_SW_SERDES_PVDD_CURR:
    case SMB_SENSOR_SW_SERDES_TRVDD_CURR:
    case SMB_SENSOR_SW_CORE_CURR:
    case SMB_SENSOR_FCM_HSC_CURR:
      sprintf(units, "Amps");
      break;
    case SMB_SENSOR_SW_SERDES_PVDD_POWER:
    case SMB_SENSOR_SW_SERDES_TRVDD_POWER:
    case SMB_SENSOR_SW_CORE_POWER:
    case SMB_SENSOR_FCM_HSC_POWER:
      sprintf(units, "Watts");
      break;
    case SMB_SENSOR_FAN1_FRONT_TACH:
    case SMB_SENSOR_FAN1_REAR_TACH:
    case SMB_SENSOR_FAN2_FRONT_TACH:
    case SMB_SENSOR_FAN2_REAR_TACH:
    case SMB_SENSOR_FAN3_FRONT_TACH:
    case SMB_SENSOR_FAN3_REAR_TACH:
    case SMB_SENSOR_FAN4_FRONT_TACH:
    case SMB_SENSOR_FAN4_REAR_TACH:
      sprintf(units, "RPM");
      break;
    default:
      return -1;
  }
  return 0;
}

static int
get_pem_sensor_units(uint8_t sensor_num, char *units) {

  switch(sensor_num) {
    case PEM1_SENSOR_IN_VOLT:
    case PEM1_SENSOR_OUT_VOLT:
    case PEM2_SENSOR_IN_VOLT:
    case PEM2_SENSOR_OUT_VOLT:
      sprintf(units, "Volts");
      break;
    case PEM1_SENSOR_CURR:
    case PEM2_SENSOR_CURR:
      sprintf(units, "Amps");
      break;
    case PEM1_SENSOR_POWER:
    case PEM2_SENSOR_POWER:
      sprintf(units, "Watts");
      break;
    case PEM1_SENSOR_FAN1_TACH:
    case PEM1_SENSOR_FAN2_TACH:
    case PEM2_SENSOR_FAN1_TACH:
    case PEM2_SENSOR_FAN2_TACH:
      sprintf(units, "RPM");
      break;
    case PEM1_SENSOR_TEMP1:
    case PEM1_SENSOR_TEMP2:
    case PEM1_SENSOR_TEMP3:
    case PEM2_SENSOR_TEMP1:
    case PEM2_SENSOR_TEMP2:
    case PEM2_SENSOR_TEMP3:
      sprintf(units, "C");
      break;
     default:
      return -1;
  }
  return 0;
}

static int
get_psu_sensor_units(uint8_t sensor_num, char *units) {

  switch(sensor_num) {
    case PSU1_SENSOR_IN_VOLT:
    case PSU1_SENSOR_12V_VOLT:
    case PSU1_SENSOR_STBY_VOLT:
    case PSU2_SENSOR_IN_VOLT:
    case PSU2_SENSOR_12V_VOLT:
    case PSU2_SENSOR_STBY_VOLT:
      sprintf(units, "Volts");
      break;
    case PSU1_SENSOR_IN_CURR:
    case PSU1_SENSOR_12V_CURR:
    case PSU1_SENSOR_STBY_CURR:
    case PSU2_SENSOR_IN_CURR:
    case PSU2_SENSOR_12V_CURR:
    case PSU2_SENSOR_STBY_CURR:
      sprintf(units, "Amps");
      break;
    case PSU1_SENSOR_IN_POWER:
    case PSU1_SENSOR_12V_POWER:
    case PSU1_SENSOR_STBY_POWER:
    case PSU2_SENSOR_IN_POWER:
    case PSU2_SENSOR_12V_POWER:
    case PSU2_SENSOR_STBY_POWER:
      sprintf(units, "Watts");
      break;
    case PSU1_SENSOR_FAN_TACH:
    case PSU2_SENSOR_FAN_TACH:
      sprintf(units, "RPM");
      break;
    case PSU1_SENSOR_TEMP1:
    case PSU1_SENSOR_TEMP2:
    case PSU1_SENSOR_TEMP3:
    case PSU2_SENSOR_TEMP1:
    case PSU2_SENSOR_TEMP2:
    case PSU2_SENSOR_TEMP3:
      sprintf(units, "C");
      break;
     default:
      return -1;
  }
  return 0;
}

int
pal_get_sensor_units(uint8_t fru, uint8_t sensor_num, char *units) {
  int ret = -1;

  switch(fru) {
    case FRU_SCM:
      ret = get_scm_sensor_units(sensor_num, units);
      break;
    case FRU_SMB:
      ret = get_smb_sensor_units(sensor_num, units);
      break;
    case FRU_PEM1:
    case FRU_PEM2:
      ret = get_pem_sensor_units(sensor_num, units);
      break;
    case FRU_PSU1:
    case FRU_PSU2:
      ret = get_psu_sensor_units(sensor_num, units);
      break;
    default:
      return -1;
  }
  return ret;
}

static void
sensor_thresh_array_init(uint8_t fru) {
  static bool init_done[MAX_NUM_FRUS] = {false};
  int i = 0, j;
  float fvalue;
  uint8_t brd_type;
  pal_get_board_type(&brd_type);
  if (init_done[fru])
    return;

  switch (fru) {
    case FRU_SCM:
      scm_sensor_threshold[SCM_SENSOR_OUTLET_TEMP][UCR_THRESH] = 80;
      scm_sensor_threshold[SCM_SENSOR_INLET_TEMP][UCR_THRESH] = 80;
      scm_sensor_threshold[SCM_SENSOR_HSC_VOLT][UCR_THRESH] = 14.13;
      scm_sensor_threshold[SCM_SENSOR_HSC_VOLT][LCR_THRESH] = 7.5;
      scm_sensor_threshold[SCM_SENSOR_HSC_CURR][UCR_THRESH] = 24.7;
      scm_sensor_threshold[SCM_SENSOR_HSC_POWER][UCR_THRESH] = 96;
      for (i = scm_sensor_cnt; i < scm_all_sensor_cnt; i++) {
        for (j = 1; j <= MAX_SENSOR_THRESHOLD + 1; j++) {
          if (!bic_get_sdr_thresh_val(fru, scm_all_sensor_list[i], j, &fvalue)){
            scm_sensor_threshold[scm_all_sensor_list[i]][j] = fvalue;
          }
        }
      }
      break;
    case FRU_SMB:
      if(brd_type == BRD_TYPE_WEDGE400){
        smb_sensor_threshold[SMB_SENSOR_1220_VMON1][UCR_THRESH] = 13.2;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON1][LCR_THRESH] = 10.8;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON2][UCR_THRESH] = 5.5;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON2][LCR_THRESH] = 4.5;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON3][UCR_THRESH] = 3.465;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON3][LCR_THRESH] = 3.135;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON4][UCR_THRESH] = 2.625;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON4][LCR_THRESH] = 2.375;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON5][UCR_THRESH] = 1.26;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON5][LCR_THRESH] = 1.14;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON6][UCR_THRESH] = 1.2075;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON6][LCR_THRESH] = 1.0925;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON7][UCR_THRESH] = 1.236;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON7][LCR_THRESH] = 1.164;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON8][UCR_THRESH] = 0.824;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON8][LCR_THRESH] = 0.776;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON9][UCR_THRESH] = 3.465;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON9][LCR_THRESH] = 3.135;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON10][UCR_THRESH] = 0.927;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON10][LCR_THRESH] = 0.7275;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON11][UCR_THRESH] = 0.824;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON11][LCR_THRESH] = 0.776;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON12][UCR_THRESH] = 1.89;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON12][LCR_THRESH] = 1.71;
      } else if (brd_type == BRD_TYPE_WEDGE400_2){
        smb_sensor_threshold[SMB_SENSOR_1220_VMON1][UCR_THRESH] = 12.6;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON1][LCR_THRESH] = 11.4;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON2][UCR_THRESH] = 5.25;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON2][LCR_THRESH] = 4.75;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON3][UCR_THRESH] = 3.465;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON3][LCR_THRESH] = 3.135;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON4][UCR_THRESH] = 3.465;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON4][LCR_THRESH] = 3.135;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON5][UCR_THRESH] = 1.26;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON5][LCR_THRESH] = 1.14;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON6][UCR_THRESH] = 1.89;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON6][LCR_THRESH] = 1.71;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON7][UCR_THRESH] = 1.89;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON7][LCR_THRESH] = 1.71;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON8][UCR_THRESH] = 2.625;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON8][LCR_THRESH] = 2.375;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON9][UCR_THRESH] = 0.987;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON9][LCR_THRESH] = 0.893;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON10][UCR_THRESH] = 0.8925;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON10][LCR_THRESH] = 0.8075;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON11][UCR_THRESH] = 0.7875;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON11][LCR_THRESH] = 0.7125;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON12][UCR_THRESH] = 1.2075;
        smb_sensor_threshold[SMB_SENSOR_1220_VMON12][LCR_THRESH] = 1.0925;
      }
      smb_sensor_threshold[SMB_SENSOR_1220_VCCA][UCR_THRESH] = 3.465;
      smb_sensor_threshold[SMB_SENSOR_1220_VCCA][LCR_THRESH] = 3.135;
      smb_sensor_threshold[SMB_SENSOR_1220_VCCINP][UCR_THRESH] = 3.465;
      smb_sensor_threshold[SMB_SENSOR_1220_VCCINP][LCR_THRESH] = 3.135;

      if (brd_type == BRD_TYPE_WEDGE400){
        smb_sensor_threshold[SMB_SENSOR_SW_SERDES_PVDD_VOLT][UCR_THRESH] = 0.824;
        smb_sensor_threshold[SMB_SENSOR_SW_SERDES_PVDD_VOLT][LCR_THRESH] = 0.776;
        smb_sensor_threshold[SMB_SENSOR_SW_SERDES_PVDD_CURR][UCR_THRESH] = 14;
        smb_sensor_threshold[SMB_SENSOR_SW_SERDES_PVDD_POWER][UCR_THRESH] = 300;
        smb_sensor_threshold[SMB_SENSOR_SW_SERDES_PVDD_TEMP1][UCR_THRESH] = 125;
        smb_sensor_threshold[SMB_SENSOR_SW_SERDES_TRVDD_VOLT][UCR_THRESH] = 0.824;
        smb_sensor_threshold[SMB_SENSOR_SW_SERDES_TRVDD_VOLT][LCR_THRESH] = 0.776;
        smb_sensor_threshold[SMB_SENSOR_SW_SERDES_TRVDD_CURR][UCR_THRESH] = 90;
        smb_sensor_threshold[SMB_SENSOR_SW_SERDES_TRVDD_POWER][UCR_THRESH] = 300;
        smb_sensor_threshold[SMB_SENSOR_SW_SERDES_TRVDD_TEMP1][UCR_THRESH] = 125;
        smb_sensor_threshold[SMB_SENSOR_SW_CORE_VOLT][UCR_THRESH] = 0.927;
        smb_sensor_threshold[SMB_SENSOR_SW_CORE_VOLT][LCR_THRESH] =	0.7275;
      } else if (brd_type == BRD_TYPE_WEDGE400_2){
        smb_sensor_threshold[SMB_SENSOR_SW_SERDES_PVDD_VOLT][UCR_THRESH] = 3.465;
        smb_sensor_threshold[SMB_SENSOR_SW_SERDES_PVDD_VOLT][LCR_THRESH] = 3.135;
        smb_sensor_threshold[SMB_SENSOR_SW_SERDES_PVDD_CURR][UCR_THRESH] = 14;
        smb_sensor_threshold[SMB_SENSOR_SW_SERDES_PVDD_POWER][UCR_THRESH] = 300;
        smb_sensor_threshold[SMB_SENSOR_SW_SERDES_PVDD_TEMP1][UCR_THRESH] = 125;
        smb_sensor_threshold[SMB_SENSOR_SW_SERDES_TRVDD_VOLT][UCR_THRESH] = 3.465;
        smb_sensor_threshold[SMB_SENSOR_SW_SERDES_TRVDD_VOLT][LCR_THRESH] = 3.135;
        smb_sensor_threshold[SMB_SENSOR_SW_SERDES_TRVDD_CURR][UCR_THRESH] = 90;
        smb_sensor_threshold[SMB_SENSOR_SW_SERDES_TRVDD_POWER][UCR_THRESH] = 300;
        smb_sensor_threshold[SMB_SENSOR_SW_SERDES_TRVDD_TEMP1][UCR_THRESH] = 125;
        smb_sensor_threshold[SMB_SENSOR_SW_CORE_VOLT][UCR_THRESH] = 0.8925;
        smb_sensor_threshold[SMB_SENSOR_SW_CORE_VOLT][LCR_THRESH] =	0.8075;
      }
      smb_sensor_threshold[SMB_SENSOR_SW_CORE_CURR][UCR_THRESH] = 450;
      smb_sensor_threshold[SMB_SENSOR_SW_CORE_POWER][UCR_THRESH] = 300;
      smb_sensor_threshold[SMB_SENSOR_SW_CORE_TEMP1][UCR_THRESH] = 125;
      smb_sensor_threshold[SMB_SENSOR_TEMP1][UCR_THRESH] = 80;
      smb_sensor_threshold[SMB_SENSOR_TEMP2][UCR_THRESH] = 80;
      smb_sensor_threshold[SMB_SENSOR_TEMP3][UCR_THRESH] = 80;
      smb_sensor_threshold[SMB_SENSOR_TEMP4][UCR_THRESH] = 80;
      smb_sensor_threshold[SMB_SENSOR_TEMP5][UCR_THRESH] = 80;
      smb_sensor_threshold[SMB_SENSOR_TEMP6][UCR_THRESH] = 80;
      smb_sensor_threshold[SMB_SENSOR_SW_DIE_TEMP1][UCR_THRESH] = 125;
      smb_sensor_threshold[SMB_SENSOR_SW_DIE_TEMP2][UCR_THRESH] = 125;
      smb_sensor_threshold[SMB_SENSOR_FCM_TEMP1][UCR_THRESH] = 80;
      smb_sensor_threshold[SMB_SENSOR_FCM_TEMP2][UCR_THRESH] = 80;
      smb_sensor_threshold[SMB_SENSOR_FCM_HSC_VOLT][UCR_THRESH] = 14.13;
      smb_sensor_threshold[SMB_SENSOR_FCM_HSC_VOLT][LCR_THRESH] = 7.5;
      smb_sensor_threshold[SMB_SENSOR_FCM_HSC_CURR][UCR_THRESH] = 40;
      smb_sensor_threshold[SMB_SENSOR_FCM_HSC_POWER][UCR_THRESH] = 288;
      smb_sensor_threshold[SMB_SENSOR_FAN1_FRONT_TACH][UCR_THRESH] = 15000;
      smb_sensor_threshold[SMB_SENSOR_FAN1_FRONT_TACH][UNC_THRESH] = 8500;
      smb_sensor_threshold[SMB_SENSOR_FAN1_FRONT_TACH][LCR_THRESH] = 1000;
      smb_sensor_threshold[SMB_SENSOR_FAN1_REAR_TACH][UCR_THRESH] = 15000;
      smb_sensor_threshold[SMB_SENSOR_FAN1_REAR_TACH][UNC_THRESH] = 8500;
      smb_sensor_threshold[SMB_SENSOR_FAN1_REAR_TACH][LCR_THRESH] = 1000;
      smb_sensor_threshold[SMB_SENSOR_FAN2_FRONT_TACH][UCR_THRESH] = 15000;
      smb_sensor_threshold[SMB_SENSOR_FAN2_FRONT_TACH][UNC_THRESH] = 8500;
      smb_sensor_threshold[SMB_SENSOR_FAN2_FRONT_TACH][LCR_THRESH] = 1000;
      smb_sensor_threshold[SMB_SENSOR_FAN2_REAR_TACH][UCR_THRESH] = 15000;
      smb_sensor_threshold[SMB_SENSOR_FAN2_REAR_TACH][UNC_THRESH] = 8500;
      smb_sensor_threshold[SMB_SENSOR_FAN2_REAR_TACH][LCR_THRESH] = 1000;
      smb_sensor_threshold[SMB_SENSOR_FAN3_FRONT_TACH][UCR_THRESH] = 15000;
      smb_sensor_threshold[SMB_SENSOR_FAN3_FRONT_TACH][UNC_THRESH] = 8500;
      smb_sensor_threshold[SMB_SENSOR_FAN3_FRONT_TACH][LCR_THRESH] = 1000;
      smb_sensor_threshold[SMB_SENSOR_FAN3_REAR_TACH][UCR_THRESH] = 15000;
      smb_sensor_threshold[SMB_SENSOR_FAN3_REAR_TACH][UNC_THRESH] = 8500;
      smb_sensor_threshold[SMB_SENSOR_FAN3_REAR_TACH][LCR_THRESH] = 1000;
      smb_sensor_threshold[SMB_SENSOR_FAN4_FRONT_TACH][UCR_THRESH] = 15000;
      smb_sensor_threshold[SMB_SENSOR_FAN4_FRONT_TACH][UNC_THRESH] = 8500;
      smb_sensor_threshold[SMB_SENSOR_FAN4_FRONT_TACH][LCR_THRESH] = 1000;
      smb_sensor_threshold[SMB_SENSOR_FAN4_REAR_TACH][UCR_THRESH] = 15000;
      smb_sensor_threshold[SMB_SENSOR_FAN4_REAR_TACH][UNC_THRESH] = 8500;
      smb_sensor_threshold[SMB_SENSOR_FAN4_REAR_TACH][LCR_THRESH] = 1000;
      break;
    case FRU_PEM1:
    case FRU_PEM2:
      i = fru - FRU_PEM1;
      pem_sensor_threshold[PEM1_SENSOR_IN_VOLT+(i*PEM1_SENSOR_CNT)][UCR_THRESH] = 13.75;
      pem_sensor_threshold[PEM1_SENSOR_IN_VOLT+(i*PEM1_SENSOR_CNT)][LCR_THRESH] = 9;
      pem_sensor_threshold[PEM1_SENSOR_OUT_VOLT+(i*PEM1_SENSOR_CNT)][UCR_THRESH] = 13.2;
      pem_sensor_threshold[PEM1_SENSOR_OUT_VOLT+(i*PEM1_SENSOR_CNT)][LCR_THRESH] = 10.8;
      pem_sensor_threshold[PEM1_SENSOR_CURR+(i*PEM1_SENSOR_CNT)][UCR_THRESH] = 83.2;
      pem_sensor_threshold[PEM1_SENSOR_POWER+(i*PEM1_SENSOR_CNT)][UCR_THRESH] = 1144;
      pem_sensor_threshold[PEM1_SENSOR_FAN1_TACH+(i*PEM1_SENSOR_CNT)][UCR_THRESH] = 23000;
      pem_sensor_threshold[PEM1_SENSOR_FAN1_TACH+(i*PEM1_SENSOR_CNT)][UNC_THRESH] = 23000;
      pem_sensor_threshold[PEM1_SENSOR_FAN1_TACH+(i*PEM1_SENSOR_CNT)][LCR_THRESH] = 1000;
      pem_sensor_threshold[PEM1_SENSOR_FAN2_TACH+(i*PEM1_SENSOR_CNT)][UCR_THRESH] = 23000;
      pem_sensor_threshold[PEM1_SENSOR_FAN2_TACH+(i*PEM1_SENSOR_CNT)][UNC_THRESH] = 23000;
      pem_sensor_threshold[PEM1_SENSOR_FAN2_TACH+(i*PEM1_SENSOR_CNT)][LCR_THRESH] = 1000;
      pem_sensor_threshold[PEM1_SENSOR_TEMP1+(i*PEM1_SENSOR_CNT)][UCR_THRESH] = 95;
      pem_sensor_threshold[PEM1_SENSOR_TEMP1+(i*PEM1_SENSOR_CNT)][UNC_THRESH] = 85;
      pem_sensor_threshold[PEM1_SENSOR_TEMP2+(i*PEM1_SENSOR_CNT)][UCR_THRESH] = 45;
      pem_sensor_threshold[PEM1_SENSOR_TEMP3+(i*PEM1_SENSOR_CNT)][UCR_THRESH] = 65;
      break;
    case FRU_PSU1:
    case FRU_PSU2:
      i = fru - FRU_PSU1;
      psu_sensor_threshold[PSU1_SENSOR_IN_VOLT+(i*PSU1_SENSOR_CNT)][UCR_THRESH] = 300;
      psu_sensor_threshold[PSU1_SENSOR_IN_VOLT+(i*PSU1_SENSOR_CNT)][LCR_THRESH] = 90;
      psu_sensor_threshold[PSU1_SENSOR_12V_VOLT+(i*PSU1_SENSOR_CNT)][UCR_THRESH] = 14.8;
      psu_sensor_threshold[PSU1_SENSOR_12V_VOLT+(i*PSU1_SENSOR_CNT)][LCR_THRESH] = 0;
      psu_sensor_threshold[PSU1_SENSOR_STBY_VOLT+(i*PSU1_SENSOR_CNT)][UCR_THRESH] = 4.2;
      psu_sensor_threshold[PSU1_SENSOR_STBY_VOLT+(i*PSU1_SENSOR_CNT)][LCR_THRESH] = 0;
      psu_sensor_threshold[PSU1_SENSOR_IN_CURR+(i*PSU1_SENSOR_CNT)][UCR_THRESH] = 9;
      psu_sensor_threshold[PSU1_SENSOR_12V_CURR+(i*PSU1_SENSOR_CNT)][UCR_THRESH] = 125;
      psu_sensor_threshold[PSU1_SENSOR_STBY_CURR+(i*PSU1_SENSOR_CNT)][UCR_THRESH] = 5;
      psu_sensor_threshold[PSU1_SENSOR_IN_POWER+(i*PSU1_SENSOR_CNT)][UCR_THRESH] = 1500;
      psu_sensor_threshold[PSU1_SENSOR_12V_POWER+(i*PSU1_SENSOR_CNT)][UCR_THRESH] = 1500;
      psu_sensor_threshold[PSU1_SENSOR_STBY_POWER+(i*PSU1_SENSOR_CNT)][UCR_THRESH] = 16.5;
      psu_sensor_threshold[PSU1_SENSOR_FAN_TACH+(i*PSU1_SENSOR_CNT)][UCR_THRESH] = 26500;
      psu_sensor_threshold[PSU1_SENSOR_FAN_TACH+(i*PSU1_SENSOR_CNT)][UNC_THRESH] = 8500;
      psu_sensor_threshold[PSU1_SENSOR_FAN_TACH+(i*PSU1_SENSOR_CNT)][LCR_THRESH] = 1000;
      psu_sensor_threshold[PSU1_SENSOR_TEMP1+(i*PSU1_SENSOR_CNT)][UCR_THRESH] = 65;
      psu_sensor_threshold[PSU1_SENSOR_TEMP2+(i*PSU1_SENSOR_CNT)][UCR_THRESH] = 100;
      psu_sensor_threshold[PSU1_SENSOR_TEMP3+(i*PSU1_SENSOR_CNT)][UCR_THRESH] = 125;
      break;
  }
  init_done[fru] = true;
}

int
pal_get_sensor_threshold(uint8_t fru, uint8_t sensor_num,
                                      uint8_t thresh, void *value) {
  float *val = (float*) value;

  sensor_thresh_array_init(fru);

  switch(fru) {
  case FRU_SCM:
    *val = scm_sensor_threshold[sensor_num][thresh];
    break;
  case FRU_SMB:
    *val = smb_sensor_threshold[sensor_num][thresh];
    break;
  case FRU_PEM1:
  case FRU_PEM2:
    *val = pem_sensor_threshold[sensor_num][thresh];
    break;
  case FRU_PSU1:
  case FRU_PSU2:
    *val = psu_sensor_threshold[sensor_num][thresh];
    break;
  default:
    return -1;
  }
  return 0;
}

bool
pal_is_fw_update_ongoing(uint8_t fru) {

  char key[MAX_KEY_LEN];
  char value[MAX_VALUE_LEN] = {0};
  int ret;
  struct timespec ts;

  switch (fru) {
    case FRU_SCM:
      sprintf(key, "slot%d_fwupd", IPMB_BUS);
      break;
    default:
      return false;
  }

  ret = kv_get(key, value, NULL, 0);
  if (ret < 0) {
     return false;
  }

  clock_gettime(CLOCK_MONOTONIC, &ts);
  if (strtoul(value, NULL, 10) > ts.tv_sec)
     return true;

  return false;
}

int
pal_get_fw_info(uint8_t fru, unsigned char target,
                unsigned char* res, unsigned char* res_len) {
  return -1;
}

static sensor_desc_t *
get_sensor_desc(uint8_t fru, uint8_t snr_num) {

  if (fru < 1 || fru > MAX_NUM_FRUS) {
    OBMC_WARN("get_sensor_desc: Wrong FRU ID %d\n", fru);
    return NULL;
  }

  return &m_snr_desc[fru-1][snr_num];
}


void
pal_sensor_assert_handle(uint8_t fru, uint8_t snr_num,
                                      float val, uint8_t thresh) {
  char crisel[128];
  char thresh_name[10];
  sensor_desc_t *snr_desc;

  switch (thresh) {
    case UNR_THRESH:
        sprintf(thresh_name, "UNR");
      break;
    case UCR_THRESH:
        sprintf(thresh_name, "UCR");
      break;
    case UNC_THRESH:
        sprintf(thresh_name, "UNCR");
      break;
    case LNR_THRESH:
        sprintf(thresh_name, "LNR");
      break;
    case LCR_THRESH:
        sprintf(thresh_name, "LCR");
      break;
    case LNC_THRESH:
        sprintf(thresh_name, "LNCR");
      break;
    default:
      OBMC_WARN("pal_sensor_assert_handle: wrong thresh enum value");
      exit(-1);
  }

  switch(snr_num) {
    case BIC_SENSOR_P3V3_MB:
    case BIC_SENSOR_P12V_MB:
    case BIC_SENSOR_P1V05_PCH:
    case BIC_SENSOR_P3V3_STBY_MB:
    case BIC_SENSOR_PV_BAT:
    case BIC_SENSOR_PVDDR:
    case BIC_SENSOR_VCCIN_VR_VOL:
    case BIC_SENSOR_VDDR_VR_VOL:
    case BIC_SENSOR_P1V05COMB_VR_VOL:
    case BIC_SENSOR_INA230_VOL:
      snr_desc = get_sensor_desc(fru, snr_num);
      sprintf(crisel, "%s %s %.2fV - ASSERT,FRU:%u",
                      snr_desc->name, thresh_name, val, fru);
      break;
    default:
      return;
  }
  pal_add_cri_sel(crisel);
  return;
}

void
pal_sensor_deassert_handle(uint8_t fru, uint8_t snr_num,
                                        float val, uint8_t thresh) {
  char crisel[128];
  char thresh_name[8];
  sensor_desc_t *snr_desc;

  switch (thresh) {
    case UNR_THRESH:
      sprintf(thresh_name, "UNR");
      break;
    case UCR_THRESH:
      sprintf(thresh_name, "UCR");
      break;
    case UNC_THRESH:
      sprintf(thresh_name, "UNCR");
      break;
    case LNR_THRESH:
      sprintf(thresh_name, "LNR");
      break;
    case LCR_THRESH:
      sprintf(thresh_name, "LCR");
      break;
    case LNC_THRESH:
      sprintf(thresh_name, "LNCR");
      break;
    default:
      OBMC_WARN(
             "pal_sensor_deassert_handle: wrong thresh enum value");
      return;
  }
  switch (fru) {
    case FRU_SCM:
      switch (snr_num) {
        case BIC_SENSOR_SOC_TEMP:
          sprintf(crisel, "SOC Temp %s %.0fC - DEASSERT,FRU:%u",
                          thresh_name, val, fru);
          break;
        case BIC_SENSOR_P3V3_MB:
        case BIC_SENSOR_P12V_MB:
        case BIC_SENSOR_P1V05_PCH:
        case BIC_SENSOR_P3V3_STBY_MB:
        case BIC_SENSOR_PV_BAT:
        case BIC_SENSOR_PVDDR:
        case BIC_SENSOR_VCCIN_VR_VOL:
        case BIC_SENSOR_VDDR_VR_VOL:
        case BIC_SENSOR_P1V05COMB_VR_VOL:
        case BIC_SENSOR_INA230_VOL:
          snr_desc = get_sensor_desc(FRU_SCM, snr_num);
          sprintf(crisel, "%s %s %.2fV - DEASSERT,FRU:%u",
                          snr_desc->name, thresh_name, val, fru);
          break;
        default:
          return;
      }
      break;
  }
  pal_add_cri_sel(crisel);
  return;
}

int
pal_sensor_threshold_flag(uint8_t fru, uint8_t snr_num, uint16_t *flag) {

  switch(fru) {
    case FRU_SCM:
      if (snr_num == BIC_SENSOR_SOC_THERM_MARGIN)
        *flag = GETMASK(SENSOR_VALID) | GETMASK(UCR_THRESH);
      else if (snr_num == BIC_SENSOR_SOC_PACKAGE_PWR)
        *flag = GETMASK(SENSOR_VALID);
      else if (snr_num == BIC_SENSOR_SOC_TJMAX)
        *flag = GETMASK(SENSOR_VALID);
      break;
    default:
      break;
  }
  return 0;
}

static void
scm_sensor_poll_interval(uint8_t sensor_num, uint32_t *value) {

  switch(sensor_num) {
    case SCM_SENSOR_OUTLET_TEMP:
    case SCM_SENSOR_INLET_TEMP:
      *value = 30;
      break;
    case SCM_SENSOR_HSC_VOLT:
    case SCM_SENSOR_HSC_CURR:
    case SCM_SENSOR_HSC_POWER:
      *value = 30;
      break;
    case BIC_SENSOR_SOC_TEMP:
    case BIC_SENSOR_SOC_THERM_MARGIN:
    case BIC_SENSOR_SOC_TJMAX:
      *value = 2;
      break;
    default:
      *value = 30;
      break;
  }
}

static void
smb_sensor_poll_interval(uint8_t sensor_num, uint32_t *value) {

  switch(sensor_num) {
    case SMB_SENSOR_SW_CORE_TEMP1:
    case SMB_SENSOR_SW_SERDES_PVDD_TEMP1:
    case SMB_SENSOR_SW_SERDES_TRVDD_TEMP1:
    case SMB_SENSOR_TEMP1:
    case SMB_SENSOR_TEMP2:
    case SMB_SENSOR_TEMP3:
    case SMB_SENSOR_TEMP4:
    case SMB_SENSOR_TEMP5:
    case SMB_SENSOR_TEMP6:
    case SMB_SENSOR_FCM_TEMP1:
    case SMB_SENSOR_FCM_TEMP2:
      *value = 30;
      break;
    case SMB_SENSOR_SW_DIE_TEMP1:
    case SMB_SENSOR_SW_DIE_TEMP2:
      *value = 2;
      break;
    case SMB_SENSOR_1220_VMON1:
    case SMB_SENSOR_1220_VMON2:
    case SMB_SENSOR_1220_VMON3:
    case SMB_SENSOR_1220_VMON4:
    case SMB_SENSOR_1220_VMON5:
    case SMB_SENSOR_1220_VMON6:
    case SMB_SENSOR_1220_VMON7:
    case SMB_SENSOR_1220_VMON8:
    case SMB_SENSOR_1220_VMON9:
    case SMB_SENSOR_1220_VMON10:
    case SMB_SENSOR_1220_VMON11:
    case SMB_SENSOR_1220_VMON12:
    case SMB_SENSOR_1220_VCCA:
    case SMB_SENSOR_1220_VCCINP:
    case SMB_SENSOR_SW_SERDES_PVDD_VOLT:
    case SMB_SENSOR_SW_SERDES_TRVDD_VOLT:
    case SMB_SENSOR_SW_CORE_VOLT:
    case SMB_SENSOR_FCM_HSC_VOLT:
    case SMB_SENSOR_SW_SERDES_PVDD_CURR:
    case SMB_SENSOR_SW_SERDES_TRVDD_CURR:
    case SMB_SENSOR_SW_CORE_CURR:
    case SMB_SENSOR_FCM_HSC_CURR:
      *value = 30;
      break;
    case SMB_SENSOR_FAN1_FRONT_TACH:
    case SMB_SENSOR_FAN1_REAR_TACH:
    case SMB_SENSOR_FAN2_FRONT_TACH:
    case SMB_SENSOR_FAN2_REAR_TACH:
    case SMB_SENSOR_FAN3_FRONT_TACH:
    case SMB_SENSOR_FAN3_REAR_TACH:
    case SMB_SENSOR_FAN4_FRONT_TACH:
    case SMB_SENSOR_FAN4_REAR_TACH:
      *value = 2;
      break;
    default:
      *value = 10;
      break;
  }
}

static void
pem_sensor_poll_interval(uint8_t sensor_num, uint32_t *value) {

  switch(sensor_num) {
    case PEM1_SENSOR_IN_VOLT:
    case PEM1_SENSOR_OUT_VOLT:
    case PEM1_SENSOR_CURR:
    case PEM1_SENSOR_POWER:
    case PEM1_SENSOR_FAN1_TACH:
    case PEM1_SENSOR_FAN2_TACH:
    case PEM1_SENSOR_TEMP1:
    case PEM1_SENSOR_TEMP2:
    case PEM1_SENSOR_TEMP3:

    case PEM2_SENSOR_IN_VOLT:
    case PEM2_SENSOR_OUT_VOLT:
    case PEM2_SENSOR_CURR:
    case PEM2_SENSOR_POWER:
    case PEM2_SENSOR_FAN1_TACH:
    case PEM2_SENSOR_FAN2_TACH:
    case PEM2_SENSOR_TEMP1:
    case PEM2_SENSOR_TEMP2:
    case PEM2_SENSOR_TEMP3:
      *value = 30;
      break;
    default:
      *value = 10;
      break;
  }
}

static void
psu_sensor_poll_interval(uint8_t sensor_num, uint32_t *value) {

  switch(sensor_num) {
    case PSU1_SENSOR_IN_VOLT:
    case PSU1_SENSOR_12V_VOLT:
    case PSU1_SENSOR_STBY_VOLT:
    case PSU1_SENSOR_IN_CURR:
    case PSU1_SENSOR_12V_CURR:
    case PSU1_SENSOR_STBY_CURR:
    case PSU1_SENSOR_IN_POWER:
    case PSU1_SENSOR_12V_POWER:
    case PSU1_SENSOR_STBY_POWER:
    case PSU1_SENSOR_FAN_TACH:
    case PSU1_SENSOR_TEMP1:
    case PSU1_SENSOR_TEMP2:
    case PSU1_SENSOR_TEMP3:
    case PSU2_SENSOR_IN_VOLT:
    case PSU2_SENSOR_12V_VOLT:
    case PSU2_SENSOR_STBY_VOLT:
    case PSU2_SENSOR_IN_CURR:
    case PSU2_SENSOR_12V_CURR:
    case PSU2_SENSOR_STBY_CURR:
    case PSU2_SENSOR_IN_POWER:
    case PSU2_SENSOR_12V_POWER:
    case PSU2_SENSOR_STBY_POWER:
    case PSU2_SENSOR_FAN_TACH:
    case PSU2_SENSOR_TEMP1:
    case PSU2_SENSOR_TEMP2:
    case PSU2_SENSOR_TEMP3:
      *value = 30;
      break;
    default:
      *value = 10;
      break;
  }
}

int
pal_get_sensor_poll_interval(uint8_t fru, uint8_t sensor_num, uint32_t *value) {

  switch(fru) {
    case FRU_SCM:
      scm_sensor_poll_interval(sensor_num, value);
      break;
    case FRU_SMB:
      smb_sensor_poll_interval(sensor_num, value);
      break;
    case FRU_PEM1:
    case FRU_PEM2:
      pem_sensor_poll_interval(sensor_num, value);
      break;
    case FRU_PSU1:
    case FRU_PSU2:
      psu_sensor_poll_interval(sensor_num, value);
      break;
    default:
      *value = 2;
      break;
  }
  return 0;
}

void
pal_get_chassis_status(uint8_t slot, uint8_t *req_data, uint8_t *res_data, uint8_t *res_len) {

  char key[MAX_KEY_LEN];
  char buff[MAX_VALUE_LEN];
  int policy = 3;
  uint8_t status, ret;
  unsigned char *data = res_data;

  /* Platform Power Policy */
  sprintf(key, "%s", "server_por_cfg");
  if (pal_get_key_value(key, buff) == 0)
  {
    if (!memcmp(buff, "off", strlen("off")))
      policy = 0;
    else if (!memcmp(buff, "lps", strlen("lps")))
      policy = 1;
    else if (!memcmp(buff, "on", strlen("on")))
      policy = 2;
    else
      policy = 3;
  }

  /* Current Power State */
  ret = pal_get_server_power(FRU_SCM, &status);
  if (ret >= 0) {
    *data++ = status | (policy << 5);
  } else {
    /* load default */
    OBMC_WARN("ipmid: pal_get_server_power failed for server\n");
    *data++ = 0x00 | (policy << 5);
  }
  *data++ = 0x00;   /* Last Power Event */
  *data++ = 0x40;   /* Misc. Chassis Status */
  *data++ = 0x00;   /* Front Panel Button Disable */
  *res_len = data - res_data;
}

uint8_t
pal_set_power_restore_policy(uint8_t slot, uint8_t *pwr_policy, uint8_t *res_data) {

    uint8_t completion_code;
    char key[MAX_KEY_LEN];
    unsigned char policy = *pwr_policy & 0x07;  /* Power restore policy */

  completion_code = CC_SUCCESS;   /* Fill response with default values */
    sprintf(key, "%s", "server_por_cfg");
    switch (policy)
    {
      case 0:
        if (pal_set_key_value(key, "off") != 0)
          completion_code = CC_UNSPECIFIED_ERROR;
        break;
      case 1:
        if (pal_set_key_value(key, "lps") != 0)
          completion_code = CC_UNSPECIFIED_ERROR;
        break;
      case 2:
        if (pal_set_key_value(key, "on") != 0)
          completion_code = CC_UNSPECIFIED_ERROR;
        break;
      case 3:
        /* no change (just get present policy support) */
        break;
      default:
          completion_code = CC_PARAM_OUT_OF_RANGE;
        break;
    }
    return completion_code;
}

void *
generate_dump(void *arg) {

  uint8_t fru = *(uint8_t *) arg;
  char cmd[128];
  char fname[128];
  char fruname[16];

  // Usually the pthread cancel state are enable by default but
  // here we explicitly would like to enable them
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  pal_get_fru_name(fru, fruname);//scm

  memset(fname, 0, sizeof(fname));
  snprintf(fname, 128, "/var/run/autodump%d.pid", fru);
  if (access(fname, F_OK) == 0) {
    memset(cmd, 0, sizeof(cmd));
    sprintf(cmd,"rm %s",fname);
    system(cmd);
  }

  // Execute automatic crashdump
  memset(cmd, 0, 128);
  sprintf(cmd, "%s %s", CRASHDUMP_BIN, fruname);
  system(cmd);

  OBMC_CRIT("Crashdump for FRU: %d is generated.", fru);

  t_dump[fru-1].is_running = 0;
  return 0;
}

static int
pal_store_crashdump(uint8_t fru) {

  int ret;
  char cmd[100];

  // Check if the crashdump script exist
  if (access(CRASHDUMP_BIN, F_OK) == -1) {
    OBMC_CRIT("Crashdump for FRU: %d failed : "
        "auto crashdump binary is not preset", fru);
    return 0;
  }

  // Check if a crashdump for that fru is already running.
  // If yes, kill that thread and start a new one.
  if (t_dump[fru-1].is_running) {
    ret = pthread_cancel(t_dump[fru-1].pt);
    if (ret == ESRCH) {
      OBMC_INFO("pal_store_crashdump: No Crashdump pthread exists");
    } else {
      pthread_join(t_dump[fru-1].pt, NULL);
      sprintf(cmd,
              "ps | grep '{dump.sh}' | grep 'scm' "
              "| awk '{print $1}'| xargs kill");
      system(cmd);
      sprintf(cmd,
              "ps | grep 'bic-util' | grep 'scm' "
              "| awk '{print $1}'| xargs kill");
      system(cmd);
#ifdef DEBUG
      OBMC_INFO("pal_store_crashdump:"
                       " Previous crashdump thread is cancelled");
#endif
    }
  }

  // Start a thread to generate the crashdump
  t_dump[fru-1].fru = fru;
  if (pthread_create(&(t_dump[fru-1].pt), NULL, generate_dump,
      (void*) &t_dump[fru-1].fru) < 0) {
    OBMC_WARN("pal_store_crashdump: pthread_create for"
        " FRU %d failed\n", fru);
    return -1;
  }

  t_dump[fru-1].is_running = 1;

  OBMC_INFO("Crashdump for FRU: %d is being generated.", fru);

  return 0;
}

int
pal_sel_handler(uint8_t fru, uint8_t snr_num, uint8_t *event_data) {

  char key[MAX_KEY_LEN] = {0};
  char cvalue[MAX_VALUE_LEN] = {0};
  static int assert_cnt[WEDGE400_MAX_NUM_SLOTS] = {0};

  switch(fru) {
    case FRU_SCM:
      switch(snr_num) {
        case CATERR_B:
          pal_store_crashdump(fru);
          break;

        case 0x00:  // don't care sensor number 00h
          return 0;
      }
      sprintf(key, "server_sel_error");

      if ((event_data[2] & 0x80) == 0) {  // 0: Assertion,  1: Deassertion
         assert_cnt[fru-1]++;
      } else {
        if (--assert_cnt[fru-1] < 0)
           assert_cnt[fru-1] = 0;
      }
      sprintf(cvalue, "%s", (assert_cnt[fru-1] > 0) ? "0" : "1");
      break;

    default:
      return -1;
  }

  /* Write the value "0" which means FRU_STATUS_BAD */
  return pal_set_key_value(key, cvalue);

}

void
init_led(void)
{
  int dev, ret;
  dev = open(LED_DEV, O_RDWR);
  if(dev < 0) {
    OBMC_ERROR(-1, "%s: open() failed\n", __func__);
    return;
  }

  ret = ioctl(dev, I2C_SLAVE, I2C_ADDR_SIM_LED);
  if(ret < 0) {
    OBMC_ERROR(-1, "%s: ioctl() assigned i2c addr failed\n", __func__);
    close(dev);
    return;
  }

  i2c_smbus_write_byte_data(dev, 0x06, 0x00);
  i2c_smbus_write_byte_data(dev, 0x07, 0x00);
  close(dev);

  return;
}

int
set_sled(int brd_rev, uint8_t color, int led_name)
{
  int dev, ret;
  uint8_t io0_reg = 0x02, io1_reg = 0x03;
  uint8_t clr_val, val_io0, val_io1;
  dev = open(LED_DEV, O_RDWR);
  if(dev < 0) {
    OBMC_ERROR(-1, "%s: open() failed\n", __func__);
    return -1;
  }

  ret = ioctl(dev, I2C_SLAVE, I2C_ADDR_SIM_LED);
  if(ret < 0) {
    OBMC_ERROR(-1, "%s: ioctl() assigned i2c addr 0x%x failed\n", __func__, I2C_ADDR_SIM_LED);
    close(dev);
    return -1;
  }
  val_io0 = i2c_smbus_read_byte_data(dev, 0x02);
  if(val_io0 < 0) {
    close(dev);
    OBMC_ERROR(-1, "%s: i2c_smbus_read_byte_data failed\n", __func__);
    return -1;
  }

  val_io1 = i2c_smbus_read_byte_data(dev, 0x03);
  if(val_io1 < 0) {
    close(dev);
    OBMC_ERROR(-1, "%s: i2c_smbus_read_byte_data failed\n", __func__);
    return -1;
  }

  clr_val = color;
  if(brd_rev == 0 || brd_rev == 4) {
    if(led_name == SLED_SMB || led_name == SLED_FAN) {
      clr_val = clr_val << 3;
      val_io0 = (val_io0 & 0x7) | clr_val;
      val_io1 = (val_io1 & 0x7) | clr_val;
    }
    else if(led_name == SLED_SYS || led_name == SLED_PSU) {
      val_io0 = (val_io0 & 0x38) | clr_val;
      val_io1 = (val_io1 & 0x38) | clr_val;
    }
    else {
      OBMC_WARN("%s: unknown led name\n", __func__);
    }

    if(led_name == SLED_SYS || led_name == SLED_FAN) {
      i2c_smbus_write_byte_data(dev, io0_reg, val_io0);
    } else {
      i2c_smbus_write_byte_data(dev, io1_reg, val_io1);
    }
  }
  else {
    if(led_name == SLED_FAN || led_name == SLED_SMB) {
      clr_val = clr_val << 3;
      val_io0 = (val_io0 & 0x7) | clr_val;
      val_io1 = (val_io1 & 0x7) | clr_val;
    }
    else if(led_name == SLED_SYS || led_name == SLED_PSU) {
      val_io0 = (val_io0 & 0x38) | clr_val;
      val_io1 = (val_io1 & 0x38) | clr_val;
    }
    else {
      OBMC_WARN("%s: unknown led name\n", __func__);
    }

    if(led_name == SLED_SYS || led_name == SLED_FAN) {
      i2c_smbus_write_byte_data(dev, io0_reg, val_io0);
    } else {
      i2c_smbus_write_byte_data(dev, io1_reg, val_io1);
    }
  }

  close(dev);
  return 0;
}


static void
upgrade_led_blink(int brd_rev,
                uint8_t sys_ug, uint8_t fan_ug, uint8_t psu_ug, uint8_t smb_ug)
{
  static uint8_t sys_alter = 0, fan_alter = 0, psu_alter = 0, smb_alter = 0;

  if(sys_ug) {
    if(sys_alter == 0) {
      set_sled(brd_rev, SLED_CLR_BLUE, SLED_SYS);
      sys_alter = 1;
    } else {
      set_sled(brd_rev, SLED_CLR_YELLOW, SLED_SYS);
      sys_alter = 0;
    }
  }
  if(fan_ug) {
    if(fan_alter == 0) {
      set_sled(brd_rev, SLED_CLR_BLUE, SLED_FAN);
      fan_alter = 1;
    } else {
      set_sled(brd_rev, SLED_CLR_YELLOW, SLED_FAN);
      fan_alter = 0;
    }
  }
  if(psu_ug) {
    if(psu_alter == 0) {
      set_sled(brd_rev, SLED_CLR_BLUE, SLED_PSU);
      psu_alter = 1;
    } else {
      set_sled(brd_rev, SLED_CLR_YELLOW, SLED_PSU);
      psu_alter = 0;
    }
  }
  if(smb_ug) {
    if(smb_alter == 0) {
      set_sled(brd_rev, SLED_CLR_BLUE, SLED_SMB);
      smb_alter = 1;
    } else {
      set_sled(brd_rev, SLED_CLR_YELLOW, SLED_SMB);
      smb_alter = 0;
    }
  }
}

int
pal_mon_fw_upgrade
(int brd_rev, uint8_t *sys_ug, uint8_t *fan_ug,
              uint8_t *psu_ug, uint8_t *smb_ug)
{
  char cmd[5];
  FILE *fp;
  int ret=-1;
  char *buf_ptr;
  int buf_size = 1000;
  int str_size = 200;
  int tmp_size;
  char str[200];
  snprintf(cmd, sizeof(cmd), "ps w");
  fp = popen(cmd, "r");
  if(NULL == fp)
     return -1;

  buf_ptr = (char *)malloc(buf_size * sizeof(char) + sizeof(char));
  memset(buf_ptr, 0, sizeof(char));
  tmp_size = str_size;
  while(fgets(str, str_size, fp) != NULL) {
    tmp_size = tmp_size + str_size;
    if(tmp_size + str_size >= buf_size) {
      buf_ptr = realloc(buf_ptr, sizeof(char) * buf_size * 2 + sizeof(char));
      buf_size *= 2;
    }
    if(!buf_ptr) {
      OBMC_ERROR(-1, 
             "%s realloc() fail, please check memory remaining", __func__);
      goto free_buf;
    }
    strncat(buf_ptr, str, str_size);
  }

  //check whether sys led need to blink
  *sys_ug = strstr(buf_ptr, "write spi2") != NULL ? 1 : 0;
  if(*sys_ug) goto fan_state;

  *sys_ug = strstr(buf_ptr, "write spi1 BACKUP_BIOS") != NULL ? 1 : 0;
  if(*sys_ug) goto fan_state;

  *sys_ug = (strstr(buf_ptr, "scmcpld_update") != NULL) ? 1 : 0;
  if(*sys_ug) goto fan_state;

  *sys_ug = (strstr(buf_ptr, "fw-util") != NULL) ?
          ((strstr(buf_ptr, "--update") != NULL) ? 1 : 0) : 0;
  if(*sys_ug) goto fan_state;

  //check whether fan led need to blink
fan_state:
  *fan_ug = (strstr(buf_ptr, "fcmcpld_update") != NULL) ? 1 : 0;

  //check whether fan led need to blink
  *psu_ug = (strstr(buf_ptr, "psu-util") != NULL) ?
          ((strstr(buf_ptr, "--update") != NULL) ? 1 : 0) : 0;

  //check whether smb led need to blink
  *smb_ug = (strstr(buf_ptr, "smbcpld_update") != NULL) ? 1 : 0;
  if(*smb_ug) goto close_fp;

  *smb_ug = (strstr(buf_ptr, "pwrcpld_update") != NULL) ? 1 : 0;
  if(*smb_ug) goto close_fp;

  *smb_ug = (strstr(buf_ptr, "flashcp") != NULL) ? 1 : 0;
  if(*smb_ug) goto close_fp;

  *smb_ug = strstr(buf_ptr, "write spi1 DOM_FPGA_FLASH") != NULL ? 1 : 0;
  if(*smb_ug) goto close_fp;

  *smb_ug = strstr(buf_ptr, "write spi1 TH3_PCIE_FLASH") != NULL ? 1 : 0;
  if(*smb_ug) goto close_fp;

  *smb_ug = strstr(buf_ptr, "write spi1 GB_PCIE_FLASH") != NULL ? 1 : 0;
  if(*smb_ug) goto close_fp;

  *smb_ug = strstr(buf_ptr, "write spi1 BCM5389_EE") != NULL ? 1 : 0;
  if(*smb_ug) goto close_fp;

close_fp:
  ret = pclose(fp);
  if(-1 == ret)
     OBMC_ERROR(-1, "%s pclose() fail ", __func__);

  if( 0 )
    upgrade_led_blink(brd_rev, *sys_ug, *fan_ug, *psu_ug, *smb_ug);

free_buf:
  free(buf_ptr);
  return 0;
}

void set_sys_led(int brd_rev)
{
  uint8_t ret = 0;
  uint8_t prsnt = 0;
  uint8_t sys_ug = 0, fan_ug = 0, psu_ug = 0, smb_ug = 0;
  static uint8_t alter_sys = 0;
  ret = pal_is_fru_prsnt(FRU_SCM, &prsnt);
  if (ret) {
    set_sled(brd_rev, SLED_CLR_YELLOW, SLED_SYS);
    OBMC_WARN("%s: can't get SCM status\n",__func__);
    return;
  }
  if (!prsnt) {
    set_sled(brd_rev, SLED_CLR_YELLOW, SLED_SYS);
    OBMC_WARN("%s: SCM isn't presence\n",__func__);
    return;
  }
  pal_mon_fw_upgrade(brd_rev, &sys_ug, &fan_ug, &psu_ug, &smb_ug);
  if( sys_ug || fan_ug || psu_ug || smb_ug ){
    OBMC_WARN("firmware upgrading in progress\n");
    if(alter_sys==0){
      alter_sys = 1;
      set_sled(brd_rev, SLED_CLR_YELLOW, SLED_SYS);
      return;
    }else{
      alter_sys = 0;
      set_sled(brd_rev, SLED_CLR_BLUE, SLED_SYS);
      return;
    }
  }
  set_sled(brd_rev, SLED_CLR_BLUE, SLED_SYS);
  return;
}

void set_fan_led(int brd_rev)
{
  int i, value;
  float unc, lcr;
  uint8_t prsnt;
  uint8_t fan_num = MAX_NUM_FAN * 2;//rear & front: MAX_NUM_FAN * 2
  char path[LARGEST_DEVICE_NAME + 1];
  char sensor_name[LARGEST_DEVICE_NAME];
  int sensor_num[] = { SMB_SENSOR_FAN1_FRONT_TACH,
                       SMB_SENSOR_FAN1_REAR_TACH ,
                       SMB_SENSOR_FAN2_FRONT_TACH,
                       SMB_SENSOR_FAN2_REAR_TACH ,
                       SMB_SENSOR_FAN3_FRONT_TACH,
                       SMB_SENSOR_FAN3_REAR_TACH ,
                       SMB_SENSOR_FAN4_FRONT_TACH,
                       SMB_SENSOR_FAN4_REAR_TACH };

  for(i = FRU_FAN1; i <= FRU_FAN4; i++) {
    pal_is_fru_prsnt(i, &prsnt);
#ifdef DEBUG
    OBMC_INFO("fan %d : %d\n",i,prsnt);
#endif
    if(!prsnt) {
      set_sled(brd_rev, SLED_CLR_YELLOW, SLED_FAN);
      OBMC_WARN("%s: fan %d isn't presence\n",__func__,i-FRU_FAN1+1);
      return;
    }
  }
  for(i = 0; i < fan_num; i++) {
    pal_get_sensor_name(FRU_SMB,sensor_num[i],sensor_name);
    snprintf(path, LARGEST_DEVICE_NAME, SENSORD_FILE_SMB, sensor_num[i]);
    if(read_device(path, &value)) {
      set_sled(brd_rev, SLED_CLR_YELLOW, SLED_FAN);
      OBMC_WARN("%s: can't access %s\n",__func__,path);
      return;
    }
    pal_get_sensor_threshold(FRU_SMB, sensor_num[i], UNC_THRESH, &unc);
    pal_get_sensor_threshold(FRU_SMB, sensor_num[i], LCR_THRESH, &lcr);
#ifdef DEBUG
    OBMC_INFO("fan %d\n",i);
    OBMC_INFO(" val %d\n",value);
    OBMC_INFO(" unc %f\n",smb_sensor_threshold[sensor_num[i]][UNC_THRESH]);
    OBMC_INFO(" lcr %f\n",smb_sensor_threshold[sensor_num[i]][LCR_THRESH]);
#endif
    if(value == 0) {
      set_sled(brd_rev, SLED_CLR_YELLOW, SLED_FAN);
      OBMC_WARN("%s: can't access %s\n",__func__,path);
      return;
    }
    else if(value > unc){
      set_sled(brd_rev, SLED_CLR_YELLOW, SLED_FAN);
      OBMC_WARN("%s: %s value is over than UNC ( %d > %.2f )\n",
      __func__,sensor_name,value,unc);
      return;
    }
    else if(value < lcr){
      set_sled(brd_rev, SLED_CLR_YELLOW, SLED_FAN);
      OBMC_WARN("%s: %s value is under than LCR ( %d < %.2f )\n",
      __func__,sensor_name,value,lcr);
      return;
    }
  }
  set_sled(brd_rev, SLED_CLR_BLUE, SLED_FAN);
  return;
}

void set_psu_led(int brd_rev)
{
  int i;
  float value,ucr,lcr;
  uint8_t prsnt,fru,ready[4] = { 0 };
  char path[LARGEST_DEVICE_NAME + 1];
  int psu1_sensor_num[] = { PSU1_SENSOR_IN_VOLT,
                            PSU1_SENSOR_12V_VOLT,
                            PSU1_SENSOR_12V_VOLT};
  int psu2_sensor_num[] = { PSU2_SENSOR_IN_VOLT,
                            PSU2_SENSOR_12V_VOLT,
                            PSU2_SENSOR_12V_VOLT};
  int pem1_sensor_num[] = { PEM1_SENSOR_IN_VOLT,
                            PEM1_SENSOR_OUT_VOLT};
  int pem2_sensor_num[] = { PEM1_SENSOR_IN_VOLT,
                            PEM1_SENSOR_OUT_VOLT};
  char sensor_name[LARGEST_DEVICE_NAME];
  for(i = FRU_PSU1; i <= FRU_PSU2; i++) {
    pal_is_fru_prsnt(i, &prsnt);
    if(!prsnt) {
      set_sled(brd_rev, SLED_CLR_YELLOW, SLED_PSU);
      OBMC_WARN("%s: PSU%d or PEM%d isn't presence\n",__func__,i-FRU_PSU1+1,i-FRU_PSU1+1);
      return;
    }
  }
  for( fru = FRU_PEM1 ; fru <= FRU_PSU2 ; fru++){
    int *sensor_num,sensor_cnt;
    pal_is_fru_ready(fru,&ready[fru-FRU_PEM1]);
    if(!ready[fru-FRU_PEM1]){
      if(fru >= FRU_PSU1 && !ready[fru-2]){     // if PSU and PEM aren't ready both
        set_sled(brd_rev, SLED_CLR_YELLOW, SLED_PSU);
        OBMC_WARN("%s: PSU%d and PEM%d can't access\n",
        __func__,fru-FRU_PSU1+1,fru-FRU_PSU1+1);
        return;
      }
      continue;
    }

    switch(fru){
      case FRU_PEM1:
        sensor_cnt = sizeof(pem1_sensor_num)/sizeof(pem1_sensor_num[0]);
        sensor_num = pem1_sensor_num;  break;
      case FRU_PEM2:
        sensor_cnt = sizeof(pem2_sensor_num)/sizeof(pem2_sensor_num[0]);
        sensor_num = pem2_sensor_num;  break;
      case FRU_PSU1:
        sensor_cnt = sizeof(psu1_sensor_num)/sizeof(psu1_sensor_num[0]);
        sensor_num = psu1_sensor_num;  break;
      case FRU_PSU2:
        sensor_cnt = sizeof(psu2_sensor_num)/sizeof(psu2_sensor_num[0]);
        sensor_num = psu2_sensor_num;  break;
      default :
        set_sled(brd_rev, SLED_CLR_YELLOW, SLED_PSU);
        OBMC_WARN("%s: fru id error %d\n",__func__,fru);
        return;
    }
    for(i = 0; i < sensor_cnt ; i++) {
      pal_get_sensor_name(fru,sensor_num[i],sensor_name);
      pal_get_sensor_threshold(fru, sensor_num[i], UCR_THRESH, &ucr);
      pal_get_sensor_threshold(fru, sensor_num[i], LCR_THRESH, &lcr);
      if(fru == FRU_PEM1 || fru == FRU_PEM2){
        snprintf(path, LARGEST_DEVICE_NAME, SENSORD_FILE_PEM, fru+1-FRU_PEM1,sensor_num[i]);
      }else if(fru == FRU_PSU1 || fru == FRU_PSU2){
        snprintf(path, LARGEST_DEVICE_NAME, SENSORD_FILE_PSU, fru+1-FRU_PSU1,sensor_num[i]);
      }else{
        set_sled(brd_rev, SLED_CLR_YELLOW, SLED_PSU);
        OBMC_WARN("%s: fru id error %d\n",__func__,fru);
        return;
      }
      if(read_device_float(path, &value)) {
        set_sled(brd_rev, SLED_CLR_YELLOW, SLED_PSU);
        OBMC_WARN("%s: fru %d sensor id %d (%s) can't access %s\n",
        __func__,fru,sensor_num[i],sensor_name,path);
        return;
      }

      if(value > ucr) {
        set_sled(brd_rev, SLED_CLR_YELLOW, SLED_PSU);
        OBMC_WARN("%s: %s value is over than UCR ( %.2f > %.2f )\n",
        __func__,sensor_name,value,ucr);
        return;
      }else if(value < lcr){
        set_sled(brd_rev, SLED_CLR_YELLOW, SLED_PSU);
        OBMC_WARN("%s: %s value is under than LCR ( %.2f < %.2f )\n",
        __func__,sensor_name,value,lcr);
        return;
      }
    }
  }
  set_sled(brd_rev, SLED_CLR_BLUE, SLED_PSU);

  return;
}

void set_smb_led(int brd_rev)
{
  char sensor_name[LARGEST_DEVICE_NAME];
  float ucr, lcr;
  char path[LARGEST_DEVICE_NAME+1];
  uint8_t brd_type;
  uint8_t *smb_sensor_list;
  uint8_t smb_sensor_list_wedge400[] = {
    SMB_SENSOR_1220_VMON1,              SMB_SENSOR_1220_VMON2,
    SMB_SENSOR_1220_VMON3,              SMB_SENSOR_1220_VMON4,
    SMB_SENSOR_1220_VMON5,              SMB_SENSOR_1220_VMON6,
    SMB_SENSOR_1220_VMON7,              SMB_SENSOR_1220_VMON8,
    SMB_SENSOR_1220_VMON9,              SMB_SENSOR_1220_VMON10,
    SMB_SENSOR_1220_VMON11,             SMB_SENSOR_1220_VMON12,
    SMB_SENSOR_1220_VCCA,               SMB_SENSOR_1220_VCCINP,
    SMB_SENSOR_SW_SERDES_PVDD_VOLT,     SMB_SENSOR_SW_SERDES_PVDD_CURR,
    SMB_SENSOR_SW_SERDES_PVDD_POWER,    SMB_SENSOR_SW_SERDES_PVDD_TEMP1,
    SMB_SENSOR_SW_SERDES_TRVDD_VOLT,    SMB_SENSOR_SW_SERDES_TRVDD_CURR,
    SMB_SENSOR_SW_SERDES_TRVDD_POWER,   SMB_SENSOR_SW_SERDES_TRVDD_TEMP1,
    SMB_SENSOR_SW_CORE_VOLT,            SMB_SENSOR_SW_CORE_CURR,
    SMB_SENSOR_TEMP1,                   SMB_SENSOR_TEMP2,
    SMB_SENSOR_TEMP3,                   SMB_SENSOR_TEMP4,
    SMB_SENSOR_TEMP5,                   SMB_SENSOR_TEMP6,
    SMB_SENSOR_SW_DIE_TEMP1,            SMB_SENSOR_SW_DIE_TEMP2,
    SMB_SENSOR_FCM_TEMP1,               SMB_SENSOR_FCM_TEMP2,
    SMB_SENSOR_FCM_HSC_VOLT,            SMB_SENSOR_FCM_HSC_CURR,
    SMB_SENSOR_FCM_HSC_POWER,
  };
  uint8_t smb_sensor_list_wedge400_2[] = {
    SMB_SENSOR_1220_VMON1,              SMB_SENSOR_1220_VMON2,
    SMB_SENSOR_1220_VMON3,              SMB_SENSOR_1220_VMON4,
    SMB_SENSOR_1220_VMON5,              SMB_SENSOR_1220_VMON6,
    SMB_SENSOR_1220_VMON7,              SMB_SENSOR_1220_VMON8,
    SMB_SENSOR_1220_VMON9,              SMB_SENSOR_1220_VMON10,
    SMB_SENSOR_1220_VMON11,             SMB_SENSOR_1220_VMON12,
    SMB_SENSOR_1220_VCCA,               SMB_SENSOR_1220_VCCINP,
    SMB_SENSOR_SW_SERDES_PVDD_VOLT,     SMB_SENSOR_SW_SERDES_PVDD_CURR,
    SMB_SENSOR_SW_SERDES_PVDD_POWER,    SMB_SENSOR_SW_SERDES_PVDD_TEMP1,
    SMB_SENSOR_SW_SERDES_TRVDD_VOLT,    SMB_SENSOR_SW_SERDES_TRVDD_CURR,
    SMB_SENSOR_SW_SERDES_TRVDD_POWER,   SMB_SENSOR_SW_SERDES_TRVDD_TEMP1,
    SMB_SENSOR_SW_CORE_VOLT,            SMB_SENSOR_SW_CORE_CURR,
    SMB_SENSOR_TEMP1,                   SMB_SENSOR_TEMP2,
    SMB_SENSOR_TEMP3,                   SMB_SENSOR_TEMP4,
    SMB_SENSOR_TEMP5,                   SMB_SENSOR_TEMP6,
    SMB_SENSOR_FCM_TEMP1,               SMB_SENSOR_FCM_TEMP2,
    SMB_SENSOR_FCM_HSC_VOLT,            SMB_SENSOR_FCM_HSC_CURR,
    SMB_SENSOR_FCM_HSC_POWER,
  };
  pal_get_board_type(&brd_type);
  if ( brd_type == BRD_TYPE_WEDGE400 )
    smb_sensor_list = smb_sensor_list_wedge400;
  if ( brd_type == BRD_TYPE_WEDGE400_2 )
    smb_sensor_list = smb_sensor_list_wedge400_2;
  for(uint8_t index = 0 ; index < sizeof(smb_sensor_list); index++)
  {
    uint8_t sensor = smb_sensor_list[index];
    pal_get_sensor_name(FRU_SMB,sensor,sensor_name);
    float value = 0;
    pal_get_sensor_threshold(FRU_SMB, sensor, UCR_THRESH, &ucr);
    pal_get_sensor_threshold(FRU_SMB, sensor, LCR_THRESH, &lcr);
    snprintf(path, LARGEST_DEVICE_NAME, SENSORD_FILE_SMB, sensor);
    if(read_device_float(path, &value)) {
      set_sled(brd_rev, SLED_CLR_YELLOW, SLED_SMB);
      OBMC_WARN("%s: can't access %s\n",__func__,path);
      return;
    }

    if( value > ucr ){
      set_sled(brd_rev, SLED_CLR_YELLOW, SLED_SMB);
      OBMC_WARN("%s: %s value is over than UCR ( %.2f > %.2f )\n",
      __func__,sensor_name,value,ucr);
      return;
    }else if( lcr != 0 && value < lcr ){
      set_sled(brd_rev, SLED_CLR_YELLOW, SLED_SMB);
      OBMC_WARN("%s: %s value is under than LCR ( %.2f < %.2f )\n",
      __func__,sensor_name,value,lcr);
      return;
    }
  }

  set_sled(brd_rev, SLED_CLR_BLUE, SLED_SMB);
  return;
}

int
pal_light_scm_led(uint8_t led_color)
{
  char path[64];
  int ret;
  char *val;
  sprintf(path, SCM_SYSFS, SYS_LED_COLOR);
  if(led_color == SCM_LED_BLUE)
    val = "0";
  else
    val = "1";
  ret = write_device(path, val);
  if (ret) {
#ifdef DEBUG
  OBMC_WARN("write_device failed for %s\n", path);
#endif
    return -1;
  }

  return 0;
}

int
pal_set_def_key_value(void) {

  int i, ret;
  char path[LARGEST_DEVICE_NAME + 1];

  for (i = 0; strcmp(key_list[i], LAST_KEY) != 0; i++) {
    snprintf(path, LARGEST_DEVICE_NAME, KV_PATH, key_list[i]);
    if ((ret = kv_set(key_list[i], def_val_list[i],
                    0, KV_FPERSIST | KV_FCREATE)) < 0) {
#ifdef DEBUG
      OBMC_WARN("pal_set_def_key_value: kv_set failed. %d", ret);
#endif
    }
  }
  return 0;
 }

int
pal_init_sensor_check(uint8_t fru, uint8_t snr_num, void *snr) {
  pal_set_def_key_value();
  return 0;
}

int
pal_get_fru_health(uint8_t fru, uint8_t *value) {

  char cvalue[MAX_VALUE_LEN] = {0};
  char key[MAX_KEY_LEN] = {0};
  int ret;

  switch(fru) {
    case FRU_SCM:
      sprintf(key, "scm_sensor_health");
      break;
    case FRU_SMB:
      sprintf(key, "smb_sensor_health");
      break;
    case FRU_FCM:
      sprintf(key, "fcm_sensor_health");
      break;

    case FRU_PEM1:
        sprintf(key, "pem1_sensor_health");
        break;
    case FRU_PEM2:
        sprintf(key, "pem2_sensor_health");
        break;

    case FRU_PSU1:
        sprintf(key, "psu1_sensor_health");
        break;
    case FRU_PSU2:
        sprintf(key, "psu2_sensor_health");
        break;

    case FRU_FAN1:
        sprintf(key, "fan1_sensor_health");
        break;
    case FRU_FAN2:
        sprintf(key, "fan2_sensor_health");
        break;
    case FRU_FAN3:
        sprintf(key, "fan3_sensor_health");
        break;
    case FRU_FAN4:
        sprintf(key, "fan4_sensor_health");
        break;

    default:
      return -1;
  }

  ret = pal_get_key_value(key, cvalue);
  if (ret) {
    return ret;
  }

  *value = atoi(cvalue);
  *value = *value & atoi(cvalue);
  return 0;
}

int
pal_set_sensor_health(uint8_t fru, uint8_t value) {

  char key[MAX_KEY_LEN] = {0};
  char cvalue[MAX_VALUE_LEN] = {0};

  switch(fru) {
    case FRU_SCM:
      sprintf(key, "scm_sensor_health");
      break;
    case FRU_SMB:
      sprintf(key, "smb_sensor_health");
      break;
    case FRU_PEM1:
      sprintf(key, "pem1_sensor_health");
      break;
    case FRU_PEM2:
      sprintf(key, "pem2_sensor_health");
      break;
    case FRU_PSU1:
      sprintf(key, "psu1_sensor_health");
      break;
    case FRU_PSU2:
      sprintf(key, "psu2_sensor_health");
      break;
    case FRU_FAN1:
      sprintf(key, "fan1_sensor_health");
      break;
    case FRU_FAN2:
      sprintf(key, "fan2_sensor_health");
      break;
    case FRU_FAN3:
      sprintf(key, "fan3_sensor_health");
      break;
    case FRU_FAN4:
      sprintf(key, "fan4_sensor_health");
      break;

    default:
      return -1;
  }

  sprintf(cvalue, (value > 0) ? "1": "0");

  return pal_set_key_value(key, cvalue);
}

int
pal_parse_sel(uint8_t fru, uint8_t *sel, char *error_log)
{
  uint8_t snr_num = sel[11];
  uint8_t *event_data = &sel[10];
  uint8_t *ed = &event_data[3];
  char temp_log[512] = {0};
  uint8_t sen_type = event_data[0];
  uint8_t chn_num, dimm_num;
  bool parsed = false;

  switch(snr_num) {
    case BIC_SENSOR_SYSTEM_STATUS:
      strcpy(error_log, "");
      switch (ed[0] & 0x0F) {
        case 0x00:
          strcat(error_log, "SOC_Thermal_Trip");
          break;
        case 0x01:
          strcat(error_log, "SOC_FIVR_Fault");
          break;
        case 0x02:
          strcat(error_log, "SOC_Throttle");
          break;
        case 0x03:
          strcat(error_log, "PCH_HOT");
          break;
      }
      parsed = true;
      break;

    case BIC_SENSOR_CPU_DIMM_HOT:
      strcpy(error_log, "");
      switch (ed[0] & 0x0F) {
        case 0x01:
          strcat(error_log, "SOC_MEMHOT");
          break;
      }
      parsed = true;
      break;

    case MEMORY_ECC_ERR:
    case MEMORY_ERR_LOG_DIS:
      strcpy(error_log, "");
      if (snr_num == MEMORY_ECC_ERR) {
        // SEL from MEMORY_ECC_ERR Sensor
        if ((ed[0] & 0x0F) == 0x0) {
          if (sen_type == 0x0C) {
            strcat(error_log, "Correctable");
            sprintf(temp_log, "DIMM%02X ECC err", ed[2]);
            pal_add_cri_sel(temp_log);
          } else if (sen_type == 0x10)
            strcat(error_log, "Correctable ECC error Logging Disabled");
        } else if ((ed[0] & 0x0F) == 0x1) {
          strcat(error_log, "Uncorrectable");
          sprintf(temp_log, "DIMM%02X UECC err", ed[2]);
          pal_add_cri_sel(temp_log);
        } else if ((ed[0] & 0x0F) == 0x5)
          strcat(error_log, "Correctable ECC error Logging Limit Reached");
        else
          strcat(error_log, "Unknown");
      } else {
        // SEL from MEMORY_ERR_LOG_DIS Sensor
        if ((ed[0] & 0x0F) == 0x0)
          strcat(error_log, "Correctable Memory Error Logging Disabled");
        else
          strcat(error_log, "Unknown");
      }

      // DIMM number (ed[2]):
      // Bit[7:5]: Socket number  (Range: 0-7)
      // Bit[4:3]: Channel number (Range: 0-3)
      // Bit[2:0]: DIMM number    (Range: 0-7)
      if (((ed[1] & 0xC) >> 2) == 0x0) {
        /* All Info Valid */
        chn_num = (ed[2] & 0x18) >> 3;
        dimm_num = ed[2] & 0x7;

        /* If critical SEL logging is available, do it */
        if (sen_type == 0x0C) {
          if ((ed[0] & 0x0F) == 0x0) {
            sprintf(temp_log, "DIMM%c%d ECC err,FRU:%u", 'A'+chn_num,
                    dimm_num, fru);
            pal_add_cri_sel(temp_log);
          } else if ((ed[0] & 0x0F) == 0x1) {
            sprintf(temp_log, "DIMM%c%d UECC err,FRU:%u", 'A'+chn_num,
                    dimm_num, fru);
            pal_add_cri_sel(temp_log);
          }
        }
        /* Then continue parse the error into a string. */
        /* All Info Valid                               */
        sprintf(temp_log, " DIMM %c%d Logical Rank %d (CPU# %d, CHN# %d, DIMM# %d)",
            'A'+chn_num, dimm_num, ed[1] & 0x03, (ed[2] & 0xE0) >> 5, chn_num, dimm_num);
      } else if (((ed[1] & 0xC) >> 2) == 0x1) {
        /* DIMM info not valid */
        sprintf(temp_log, " (CPU# %d, CHN# %d)",
            (ed[2] & 0xE0) >> 5, (ed[2] & 0x18) >> 3);
      } else if (((ed[1] & 0xC) >> 2) == 0x2) {
        /* CHN info not valid */
        sprintf(temp_log, " (CPU# %d, DIMM# %d)",
            (ed[2] & 0xE0) >> 5, ed[2] & 0x7);
      } else if (((ed[1] & 0xC) >> 2) == 0x3) {
        /* CPU info not valid */
        sprintf(temp_log, " (CHN# %d, DIMM# %d)",
            (ed[2] & 0x18) >> 3, ed[2] & 0x7);
      }
      strcat(error_log, temp_log);
      parsed = true;
      break;
  }

  if (parsed == true) {
    if ((event_data[2] & 0x80) == 0) {
      strcat(error_log, " Assertion");
    } else {
      strcat(error_log, " Deassertion");
    }
    return 0;
  }

  pal_parse_sel_helper(fru, sel, error_log);

  return 0;
}

int
wedge400_sensor_name(uint8_t fru, uint8_t sensor_num, char *name) {

  switch(fru) {
    case FRU_SCM:
      switch(sensor_num) {
        case BIC_SENSOR_SYSTEM_STATUS:
          sprintf(name, "SYSTEM_STATUS");
          break;
        case BIC_SENSOR_SYS_BOOT_STAT:
          sprintf(name, "SYS_BOOT_STAT");
          break;
        case BIC_SENSOR_CPU_DIMM_HOT:
          sprintf(name, "CPU_DIMM_HOT");
          break;
        case BIC_SENSOR_PROC_FAIL:
          sprintf(name, "PROC_FAIL");
          break;
        case BIC_SENSOR_VR_HOT:
          sprintf(name, "VR_HOT");
          break;
        default:
          return -1;
      }
      break;
  }
  return 0;
}

int
pal_get_event_sensor_name(uint8_t fru, uint8_t *sel, char *name) {
  uint8_t snr_type = sel[10];
  uint8_t snr_num = sel[11];

  // If SNR_TYPE is OS_BOOT, sensor name is OS
  switch (snr_type) {
    case OS_BOOT:
      // OS_BOOT used by OS
      sprintf(name, "OS");
      return 0;
    default:
      if (wedge400_sensor_name(fru, snr_num, name) != 0) {
        break;
      }
      return 0;
  }
  // Otherwise, translate it based on snr_num
  return pal_get_x86_event_sensor_name(fru, snr_num, name);
}

// Read GUID from EEPROM
static int
pal_get_guid(uint16_t offset, char *guid) {
  int fd = 0;
  ssize_t bytes_rd;
  char eeprom_path[FBW_EEPROM_PATH_SIZE];
  errno = 0;

  wedge_eeprom_path(eeprom_path);

  // Check if file is present
  if (access(eeprom_path, F_OK) == -1) {
      OBMC_ERROR(-1, "pal_get_guid: unable to access the %s file: %s",
          eeprom_path, strerror(errno));
      return errno;
  }

  // Open the file
  fd = open(eeprom_path, O_RDONLY);
  if (fd == -1) {
    OBMC_ERROR(-1, "pal_get_guid: unable to open the %s file: %s",
        eeprom_path, strerror(errno));
    return errno;
  }

  // seek to the offset
  lseek(fd, offset, SEEK_SET);

  // Read bytes from location
  bytes_rd = read(fd, guid, GUID_SIZE);
  if (bytes_rd != GUID_SIZE) {
    OBMC_ERROR(-1, "pal_get_guid: read to %s file failed: %s",
        eeprom_path, strerror(errno));
    goto err_exit;
  }

err_exit:
  close(fd);
  return errno;
}

// GUID based on RFC4122 format @ https://tools.ietf.org/html/rfc4122
static void
pal_populate_guid(uint8_t *guid, char *str) {
  unsigned int secs;
  unsigned int usecs;
  struct timeval tv;
  uint8_t count;
  uint8_t lsb, msb;
  int i, r;

  // Populate time
  gettimeofday(&tv, NULL);

  secs = tv.tv_sec;
  usecs = tv.tv_usec;
  guid[0] = usecs & 0xFF;
  guid[1] = (usecs >> 8) & 0xFF;
  guid[2] = (usecs >> 16) & 0xFF;
  guid[3] = (usecs >> 24) & 0xFF;
  guid[4] = secs & 0xFF;
  guid[5] = (secs >> 8) & 0xFF;
  guid[6] = (secs >> 16) & 0xFF;
  guid[7] = (secs >> 24) & 0x0F;

  // Populate version
  guid[7] |= 0x10;

  // Populate clock seq with randmom number
  //getrandom(&guid[8], 2, 0);
  srand(time(NULL));
  //memcpy(&guid[8], rand(), 2);
  r = rand();
  guid[8] = r & 0xFF;
  guid[9] = (r>>8) & 0xFF;

  // Use string to populate 6 bytes unique
  // e.g. LSP62100035 => 'S' 'P' 0x62 0x10 0x00 0x35
  count = 0;
  for (i = strlen(str)-1; i >= 0; i--) {
    if (count == 6) {
      break;
    }

    // If alphabet use the character as is
    if (isalpha(str[i])) {
      guid[15-count] = str[i];
      count++;
      continue;
    }

    // If it is 0-9, use two numbers as BCD
    lsb = str[i] - '0';
    if (i > 0) {
      i--;
      if (isalpha(str[i])) {
        i++;
        msb = 0;
      } else {
        msb = str[i] - '0';
      }
    } else {
      msb = 0;
    }
    guid[15-count] = (msb << 4) | lsb;
    count++;
  }

  // zero the remaining bytes, if any
  if (count != 6) {
    memset(&guid[10], 0, 6-count);
  }

}

int
pal_get_dev_guid(uint8_t fru, char *guid) {

  pal_get_guid(OFFSET_DEV_GUID, (char *)g_dev_guid);
  memcpy(guid, g_dev_guid, GUID_SIZE);

  return 0;
}

int
pal_get_sys_guid(uint8_t slot, char *guid) {

  return bic_get_sys_guid(slot, (uint8_t *)guid);
}

int
pal_set_sys_guid(uint8_t slot, char *str) {
  uint8_t guid[GUID_SIZE] = {0x00};

  pal_populate_guid(guid, str);

  return bic_set_sys_guid(slot, guid);
}

int
pal_get_boot_order(uint8_t slot, uint8_t *req_data, uint8_t *boot, uint8_t *res_len) {
  int i;
  int j = 0;
  int ret;
  int msb, lsb;
  char key[MAX_KEY_LEN] = {0};
  char str[MAX_VALUE_LEN] = {0};
  char tstr[4] = {0};

  sprintf(key, "slot%d_boot_order", slot);
  ret = pal_get_key_value(key, str);
  if (ret) {
    *res_len = 0;
    return ret;
  }

  for (i = 0; i < 2*SIZE_BOOT_ORDER; i += 2) {
    sprintf(tstr, "%c\n", str[i]);
    msb = strtol(tstr, NULL, 16);

    sprintf(tstr, "%c\n", str[i+1]);
    lsb = strtol(tstr, NULL, 16);
    boot[j++] = (msb << 4) | lsb;
  }

  *res_len = SIZE_BOOT_ORDER;
  return 0;
}

int
pal_set_boot_order(uint8_t slot, uint8_t *boot, uint8_t *res_data, uint8_t *res_len) {
  int i, j, network_dev = 0;
  char key[MAX_KEY_LEN] = {0};
  char str[MAX_VALUE_LEN] = {0};
  char tstr[10] = {0};
  enum {
    BOOT_DEVICE_IPV4 = 0x1,
    BOOT_DEVICE_IPV6 = 0x9,
  };

  *res_len = 0;

  for (i = 0; i < SIZE_BOOT_ORDER; i++) {
    if (i > 0) {  // byte[0] is boot mode, byte[1:5] are boot order
      for (j = i+1; j < SIZE_BOOT_ORDER; j++) {
        if (boot[i] == boot[j])
          return CC_INVALID_PARAM;
      }

      // If bit[2:0] is 001b (Network), bit[3] is IPv4/IPv6 order
      // bit[3]=0b: IPv4 first
      // bit[3]=1b: IPv6 first
      if ((boot[i] == BOOT_DEVICE_IPV4) || (boot[i] == BOOT_DEVICE_IPV6))
        network_dev++;
    }

    snprintf(tstr, 3, "%02x", boot[i]);
    strncat(str, tstr, 3);
  }

  // not allow having more than 1 network boot device in the boot order
  if (network_dev > 1){
    OBMC_ERROR(-1, "Network device are %d",network_dev);
    return CC_INVALID_PARAM;
  }
  OBMC_WARN("pal_set_boot_order: %s",str);

  sprintf(key, "slot%d_boot_order", slot);
  return pal_set_key_value(key, str);
}
