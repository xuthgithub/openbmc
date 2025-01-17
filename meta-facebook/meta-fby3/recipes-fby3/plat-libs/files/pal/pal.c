/*
 *
 * Copyright 2015-present Facebook. All Rights Reserved.
 *
 * This file contains code to support IPMI2.0 Specification available @
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
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <sys/time.h>
#include <openbmc/kv.h>
#include <openbmc/libgpio.h>
#include "pal.h"

#define PLATFORM_NAME "yosemitev3"
#define LAST_KEY "last_key"

#define GUID_SIZE 16
#define OFFSET_SYS_GUID 0x17F0
#define OFFSET_DEV_GUID 0x1800

const char pal_fru_list_print[] = "all, slot1, slot2, slot3, slot4, bmc, bb";
const char pal_fru_list_rw[] = "slot1, slot2, slot3, slot4, bmc, bb";
const char pal_fru_list_sensor_history[] = "all, slot1, slot2, slot3, slot4, bmc";

const char pal_fru_list[] = "all, slot1, slot2, slot3, slot4, bmc";
const char pal_server_list[] = "slot1, slot2, slot3, slot4";

#define SYSFW_VER "sysfw_ver_slot"
#define SYSFW_VER_STR SYSFW_VER"%d"

enum key_event {
  KEY_BEFORE_SET,
  KEY_AFTER_INI,
};

struct pal_key_cfg {
  char *name;
  char *def_val;
  int (*function)(int, void*);
} key_cfg[] = {
  /* name, default value, function */
  {SYSFW_VER "1", "0", NULL},
  {SYSFW_VER "2", "0", NULL},
  {SYSFW_VER "3", "0", NULL},
  {SYSFW_VER "4", "0", NULL},
  {"pwr_server1_last_state", "on", NULL},
  {"pwr_server2_last_state", "on", NULL},
  {"pwr_server3_last_state", "on", NULL},
  {"pwr_server4_last_state", "on", NULL},
  {"timestamp_sled", "0", NULL},
  {"slot1_boot_order", "0000000", NULL},
  {"slot2_boot_order", "0000000", NULL},
  {"slot3_boot_order", "0000000", NULL},
  {"slot4_boot_order", "0000000", NULL},
  {"fru1_restart_cause", "3", NULL},
  {"fru2_restart_cause", "3", NULL},
  {"fru3_restart_cause", "3", NULL},
  {"fru4_restart_cause", "3", NULL},
  {"ntp_server", "", NULL},  
  /* Add more Keys here */
  {LAST_KEY, LAST_KEY, NULL} /* This is the last key of the list */
};

static int
pal_key_index(char *key) {

  int i;

  i = 0;
  while(strcmp(key_cfg[i].name, LAST_KEY)) {

    // If Key is valid, return success
    if (!strcmp(key, key_cfg[i].name))
      return i;

    i++;
  }

#ifdef DEBUG
  syslog(LOG_WARNING, "pal_key_index: invalid key - %s", key);
#endif
  return -1;
}

int
pal_get_key_value(char *key, char *value) {
  int index;

  // Check is key is defined and valid
  if ((index = pal_key_index(key)) < 0)
    return -1;

  return kv_get(key, value, NULL, KV_FPERSIST);
}

int
pal_set_key_value(char *key, char *value) {
  int index, ret;
  // Check is key is defined and valid
  if ((index = pal_key_index(key)) < 0)
    return -1;

  if (key_cfg[index].function) {
    ret = key_cfg[index].function(KEY_BEFORE_SET, value);
    if (ret < 0)
      return ret;
  }

  return kv_set(key, value, 0, KV_FPERSIST);
}

void
pal_dump_key_value(void) {
  int ret;
  int i = 0;
  char value[MAX_VALUE_LEN] = {0x0};

  while (strcmp(key_cfg[i].name, LAST_KEY)) {
    printf("%s:", key_cfg[i].name);
    if ((ret = kv_get(key_cfg[i].name, value, NULL, KV_FPERSIST)) < 0) {
    printf("\n");
  } else {
    printf("%s\n",  value);
  }
    i++;
    memset(value, 0, MAX_VALUE_LEN);
  }
}

int
pal_set_def_key_value() {
  int i;
  //char key[MAX_KEY_LEN] = {0};

  for(i = 0; strcmp(key_cfg[i].name, LAST_KEY) != 0; i++) {
    if (kv_set(key_cfg[i].name, key_cfg[i].def_val, 0, KV_FCREATE | KV_FPERSIST)) {
#ifdef DEBUG
      syslog(LOG_WARNING, "pal_set_def_key_value: kv_set failed.");
#endif
    }
    if (key_cfg[i].function) {
      key_cfg[i].function(KEY_AFTER_INI, key_cfg[i].name);
    }
  }

  return 0;
}

bool
pal_is_fw_update_ongoing(uint8_t fruid) {
  char key[MAX_KEY_LEN];
  char value[MAX_VALUE_LEN] = {0};
  int ret;
  struct timespec ts;

  sprintf(key, "fru%d_fwupd", fruid);
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
pal_set_sysfw_ver(uint8_t slot, uint8_t *ver) {
  int i;
  char key[MAX_KEY_LEN] = {0};
  char str[MAX_VALUE_LEN] = {0};
  char tstr[10] = {0};

  sprintf(key, SYSFW_VER_STR, (int) slot);
  for (i = 0; i < SIZE_SYSFW_VER; i++) {
    sprintf(tstr, "%02x", ver[i]);
    strcat(str, tstr);
  }

  return pal_set_key_value(key, str);
}

void
pal_update_ts_sled()
{
  char key[MAX_KEY_LEN] = {0};
  char tstr[MAX_VALUE_LEN] = {0};
  struct timespec ts;

  clock_gettime(CLOCK_REALTIME, &ts);
  sprintf(tstr, "%ld", ts.tv_sec);

  sprintf(key, "timestamp_sled");

  pal_set_key_value(key, tstr);
}

#if 0

static int
key_func_por_policy (int event, void *arg) {
  char value[MAX_VALUE_LEN] = {0};
  int ret = -1;

  switch (event) {
    case KEY_BEFORE_SET:
      if (pal_is_fw_update_ongoing(FRU_MB))
        return -1;
      // sync to env
      if ( !strcmp(arg,"lps") || !strcmp(arg,"on") || !strcmp(arg,"off")) {
        ret = fw_setenv("por_policy", (char *)arg);
      }
      else
        return -1;
      break;
    case KEY_AFTER_INI:
      // sync to env
      kv_get("server_por_cfg", value, NULL, KV_FPERSIST);
      ret = fw_setenv("por_policy", value);
      break;
  }

  return ret;
}

static int
key_func_lps (int event, void *arg)
{
  char value[MAX_VALUE_LEN] = {0};

  switch (event) {
    case KEY_BEFORE_SET:
      if (pal_is_fw_update_ongoing(FRU_MB))
        return -1;
      fw_setenv("por_ls", (char *)arg);
      break;
    case KEY_AFTER_INI:
      kv_get("pwr_server_last_state", value, NULL, KV_FPERSIST);
      fw_setenv("por_ls", value);
      break;
  }

  return 0;
}
#endif

int
pal_get_platform_name(char *name) {
  strcpy(name, PLATFORM_NAME);
  return PAL_EOK;
}

int
pal_get_fru_list(char *list) {
  strcpy(list, pal_fru_list);
  return PAL_EOK;
}

int
pal_is_fru_prsnt(uint8_t fru, uint8_t *status) {

  int ret = PAL_EOK;

  switch (fru) {
    case FRU_SLOT1:
    case FRU_SLOT2:
    case FRU_SLOT3:
    case FRU_SLOT4:
      ret = fby3_common_is_fru_prsnt(fru, status);
      break;
    case FRU_BB:
      *status = 1;
      break;
    case FRU_NIC:
      *status = 1;
      break;
    case FRU_BMC:
      *status = 1;
      break;
    default:
      *status = 0;
      syslog(LOG_WARNING, "%s() wrong fru id 0x%02x", __func__, fru);
      return -1;
  }

  return ret;
}

int
pal_get_fru_id(char *str, uint8_t *fru) {
  int ret = 0;

  ret = fby3_common_get_fru_id(str, fru);
  if ( ret < 0 ) {
    syslog(LOG_WARNING, "%s() wrong fru %s", __func__, str);
    return -1;
  }

  return ret;
}

int
pal_get_fruid_name(uint8_t fru, char *name) {

  switch(fru) {
  case FRU_SLOT1:
    sprintf(name, "Server board 1");
    break;
  case FRU_SLOT2:
    sprintf(name, "Server board 2");
    break;
  case FRU_SLOT3:
    sprintf(name, "Server board 3");
    break;
  case FRU_SLOT4:
    sprintf(name, "Server board 4");
    break;
  case FRU_BMC:
    sprintf(name, "BMC");
    break;
  case FRU_BB:
    sprintf(name, "Baseboard");
    break;
  case FRU_NIC:
    sprintf(name, "NIC");
    break;
  default:
    syslog(LOG_WARNING, "%s() wrong fru %d", __func__, fru);
    return -1;
  }

  return PAL_EOK;
}

int
pal_get_fru_name(uint8_t fru, char *name) {

  switch(fru) {
    case FRU_SLOT1:
      sprintf(name, "slot1");
      break;
    case FRU_SLOT2:
      sprintf(name, "slot2");
      break;
    case FRU_SLOT3:
      sprintf(name, "slot3");
      break;
    case FRU_SLOT4:
      sprintf(name, "slot4");
      break;
    case FRU_BMC:
      sprintf(name, "bmc");
      break;
    case FRU_NIC:
      sprintf(name, "nic");
      break;
    default:
      syslog(LOG_WARNING, "%s() unknown fruid %d", __func__, fru);
      return -1;
  }

  return PAL_EOK;
}

int
pal_get_fruid_eeprom_path(uint8_t fru, char *path) {
  int ret = 0;
  uint8_t bmc_location = 0;

  ret = fby3_common_get_bmc_location(&bmc_location);
  if ( ret < 0 ) {
    syslog(LOG_WARNING, "%s() Cannot get the location of BMC", __func__);
    return ret;;
  }

  switch(fru) {
  case FRU_BMC:
    sprintf(path, EEPROM_PATH, (bmc_location == BB_BMC)?CLASS1_FRU_BUS:CLASS2_FRU_BUS, BMC_FRU_ADDR);
    break;
  case FRU_BB:
    sprintf(path, EEPROM_PATH, (bmc_location == BB_BMC)?CLASS1_FRU_BUS:CLASS2_FRU_BUS, BB_FRU_ADDR);
    break;
  default:
    return -1;
  }

  return PAL_EOK;
}

int
pal_get_fruid_path(uint8_t fru, char *path) {
  char fname[16] = {0};

  switch(fru) {
  case FRU_SLOT1:
    sprintf(fname, "slot1");
    break;
  case FRU_SLOT2:
    sprintf(fname, "slot2");
    break;
  case FRU_SLOT3:
    sprintf(fname, "slot3");
    break;
  case FRU_SLOT4:
    sprintf(fname, "slot4");
    break;
  case FRU_BMC:
    sprintf(fname, "bmc");
    break;
  case FRU_NIC:
    sprintf(fname, "nic");
    break;
  case FRU_BB:
    sprintf(fname, "bb");
    break;
  default:
    syslog(LOG_WARNING, "%s() unknown fruid %d", __func__, fru);
    return -1;
  }

  sprintf(path, "/tmp/fruid_%s.bin", fname);
  return PAL_EOK;
}

int
pal_fruid_write(uint8_t fru, char *path)
{
  if (fru == FRU_NIC) {
    syslog(LOG_WARNING, "%s() nic is not supported", __func__);
    return -1;
    //return _write_nic_fruid(path);
  }
  return bic_write_fruid(fru, 0, path, NONE_INTF);
}

int
pal_is_bmc_por(void) {
  FILE *fp;
  int por = 0;

  fp = fopen("/tmp/ast_por", "r");
  if (fp != NULL) {
    if (fscanf(fp, "%d", &por) != 1) {
      por = 0;
    }
    fclose(fp);
  }

  return (por)?1:0;
}

int
pal_get_sysfw_ver(uint8_t slot, uint8_t *ver) {
  int i;
  int j = 0;
  int ret;
  int msb, lsb;
  char key[MAX_KEY_LEN] = {0};
  char str[MAX_VALUE_LEN] = {0};
  char tstr[4] = {0};

  sprintf(key, SYSFW_VER_STR, (int) slot);
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
pal_is_fru_ready(uint8_t fru, uint8_t *status) {
  int ret = PAL_EOK;

  switch (fru) {
    case FRU_SLOT1:
    case FRU_SLOT2:
    case FRU_SLOT3:
    case FRU_SLOT4:
      *status = 1;
      //ret = fby3_common_is_bic_ready(fru, status);
      break;
    case FRU_BB:
      *status = 1;
      break;
    case FRU_NIC:
      *status = 1;
      break;
    case FRU_BMC:
      *status = 1;
      break;
    default:
      ret = -1;
      *status = 0;
      syslog(LOG_WARNING, "%s() wrong fru id 0x%02x", __func__, fru);
      break;
  }

  return ret;
}

// GUID based on RFC4122 format @ https://tools.ietf.org/html/rfc4122
static void
pal_populate_guid(char *guid, char *str) {
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
  srand(time(NULL));
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

  return;
}

// GUID for System and Device
static int
pal_get_guid(uint16_t offset, char *guid) {
  char path[128] = {0};
  int fd;
  uint8_t bmc_location = 0;
  ssize_t bytes_rd;

  errno = 0;

  if ( fby3_common_get_bmc_location(&bmc_location) < 0 ) {
    syslog(LOG_ERR, "%s() Cannot get the location of BMC", __func__);
    return -1;
  }

  snprintf(path, sizeof(path), EEPROM_PATH, (bmc_location == BB_BMC)?CLASS1_FRU_BUS:CLASS2_FRU_BUS, BB_FRU_ADDR);

  // check for file presence
  if (access(path, F_OK)) {
    syslog(LOG_ERR, "%s() unable to access %s: %s", __func__, path, strerror(errno));
    return errno;
  }

  fd = open(path, O_RDONLY);
  if (fd < 0) {
    syslog(LOG_ERR, "%s() unable to open %s: %s", __func__, path, strerror(errno));
    return errno;
  }

  lseek(fd, offset, SEEK_SET);

  bytes_rd = read(fd, guid, GUID_SIZE);
  if (bytes_rd != GUID_SIZE) {
    syslog(LOG_ERR, "%s() read from %s failed: %s", __func__, path, strerror(errno));
  }

  if (fd > 0 ) {
    close(fd);
  }

  return errno;
}

static int
pal_set_guid(uint16_t offset, char *guid) {
  char path[128] = {0};
  int fd;
  uint8_t bmc_location = 0;
  ssize_t bytes_wr;

  errno = 0;

  if ( fby3_common_get_bmc_location(&bmc_location) < 0 ) {
    syslog(LOG_ERR, "%s() Cannot get the location of BMC", __func__);
    return -1;
  }

  snprintf(path, sizeof(path), EEPROM_PATH, (bmc_location == BB_BMC)?CLASS1_FRU_BUS:CLASS2_FRU_BUS, BB_FRU_ADDR);

  // check for file presence
  if (access(path, F_OK)) {
    syslog(LOG_ERR, "%s() unable to open %s: %s", __func__, path, strerror(errno));
    return errno;
  }

  fd = open(path, O_WRONLY);
  if (fd < 0) {
    syslog(LOG_ERR, "%s() read from %s failed: %s", __func__, path, strerror(errno));
    return errno;
  }

  lseek(fd, offset, SEEK_SET);

  bytes_wr = write(fd, guid, GUID_SIZE);
  if (bytes_wr != GUID_SIZE) {
    syslog(LOG_ERR, "%s() write to %s failed: %s", __func__, path, strerror(errno));
  }

  close(fd);
  return errno;
}

int
pal_get_sys_guid(uint8_t fru, char *guid) {
  return pal_get_guid(OFFSET_SYS_GUID, guid);
}

int
pal_set_sys_guid(uint8_t fru, char *str) {
  char guid[GUID_SIZE] = {0};

  pal_populate_guid(guid, str);
  return pal_set_guid(OFFSET_SYS_GUID, guid);
}

int
pal_get_dev_guid(uint8_t fru, char *guid) {
  return pal_get_guid(OFFSET_DEV_GUID, guid);
}

int
pal_set_dev_guid(uint8_t fru, char *str) {
  char guid[GUID_SIZE] = {0};

  pal_populate_guid(guid, str);
  return pal_set_guid(OFFSET_DEV_GUID, guid);
}
