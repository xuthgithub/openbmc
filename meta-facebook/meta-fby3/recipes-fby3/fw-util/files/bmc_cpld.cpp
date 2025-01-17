#include <iostream>
#include <sstream>
#include <string>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>
#include <sys/mman.h>
#include <syslog.h>
#include <openbmc/obmc-i2c.h>
#include "bmc_cpld.h"

using namespace std;

int BmcCpldComponent::get_cpld_version(uint8_t *ver) {
  int ret = 0;
  uint8_t tbuf[4] = {0x00, 0x20, 0x00, 0x28};
  uint8_t tlen = 4;
  uint8_t rlen = 4;
  int i2cfd = 0;
  string i2cdev("/dev/i2c-12");

  if ((i2cfd = open(i2cdev.c_str(), O_RDWR)) < 0) {
    printf("Failed to open %s\n", i2cdev.c_str());
    return -1;
  }

  if (ioctl(i2cfd, I2C_SLAVE, addr>>1) < 0) {
    printf("Failed to talk to slave@0x%02X\n", addr);
  } else {
    ret = i2c_rdwr_msg_transfer(i2cfd, addr, tbuf, tlen, ver, rlen);
  }

  if ( i2cfd > 0 ) close(i2cfd);

  return ret;
}

int BmcCpldComponent::print_version()
{
  uint8_t ver[4] = {0};
  string fru_name = fru();
  try {
    //TODO The function is not ready, we skip it now.
    //server.ready();

    // Print CPLD Version
    transform(fru_name.begin(), fru_name.end(), fru_name.begin(), ::toupper);
    if ( get_cpld_version(ver) < 0 ) {
      printf("%s CPLD Version: NA\n", fru_name.c_str());
    }
    else {
      printf("%s CPLD Version: %02X%02X%02X%02X\n", fru_name.c_str(), ver[3], ver[2], ver[1], ver[0]);
    }
  } catch(string err) {
    printf("%s CPLD Version: NA (%s)\n", fru_name.c_str(), err.c_str());
  }
  return 0;
}
