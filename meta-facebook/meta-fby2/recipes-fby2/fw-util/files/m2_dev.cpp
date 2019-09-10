#include "fw-util.h"
#include <cstdio>
#include <cstring>
#include "server.h"
#include <openbmc/pal.h>
#include <syslog.h>
#ifdef BIC_SUPPORT
#include <facebook/bic.h>

using namespace std;

#if defined(CONFIG_FBY2_GPV2)

#define MAX_READ_RETRY 10
/* NVMe-MI Form Factor Information 0 Register */
#define FFI_0_STORAGE 0x00
#define FFI_0_ACCELERATOR 0x01

class M2_DevComponent : public Component {
  uint8_t slot_id = 0;
  uint8_t dev_id = 0;
  Server server;
  public:
  M2_DevComponent(string fru, string comp, uint8_t _slot_id, uint8_t dev_id)
    : Component(fru, comp), slot_id(_slot_id), dev_id(dev_id), server(_slot_id, fru) {
    if (fby2_get_slot_type(_slot_id) != SLOT_TYPE_GPV2) {
      (*fru_list)[fru].erase(comp);
      if ((*fru_list)[fru].size() == 0) {
        fru_list->erase(fru);
      }
    }
  }

  int print_version() {
    int ret = 0;
    uint8_t retry = MAX_READ_RETRY;
    uint16_t vendor_id = 0;
    uint8_t nvme_ready = 0 ,status = 0 ,ffi = 0 ,meff = 0 ,major_ver = 0, minor_ver = 0;

    if (fby2_get_slot_type(slot_id) != SLOT_TYPE_GPV2)
      return -1;

    while (retry) {
      ret = bic_get_dev_power_status(slot_id, dev_id + 1, &nvme_ready, &status, &ffi, &meff, &vendor_id, &major_ver,&minor_ver);
      if (!ret)
        break;
      msleep(50);
      retry--;
    }

    if (ret || !status || !nvme_ready || (ffi != FFI_0_ACCELERATOR) ) {
      printf("device%d Version: NA",dev_id);
      if (ret) { // Add Reason for dsiplay NA
        printf("\n");
      } else if (!status) {
        printf("(Not Presnet)\n");
      } else if (!nvme_ready) {
        printf("(NVMe Not Ready)\n");
      } else if (ffi != FFI_0_ACCELERATOR) {
        printf("(Not Accelerator)\n");
      }
    } else {
      printf("device%d Version: v%d.%d\n",dev_id,major_ver,minor_ver);
    }
    return 0;
  }

  int update(string image) {
    return FW_STATUS_NOT_SUPPORTED;
  }
};

// Register M.2 device components
// slot1 device from 0 to 11
M2_DevComponent m2_slot1_dev0("slot1", "device0", 1, 0);
M2_DevComponent m2_slot1_dev1("slot1", "device1", 1, 1);
M2_DevComponent m2_slot1_dev2("slot1", "device2", 1, 2);
M2_DevComponent m2_slot1_dev3("slot1", "device3", 1, 3);
M2_DevComponent m2_slot1_dev4("slot1", "device4", 1, 4);
M2_DevComponent m2_slot1_dev5("slot1", "device5", 1, 5);
M2_DevComponent m2_slot1_dev6("slot1", "device6", 1, 6);
M2_DevComponent m2_slot1_dev7("slot1", "device7", 1, 7);
M2_DevComponent m2_slot1_dev8("slot1", "device8", 1, 8);
M2_DevComponent m2_slot1_dev9("slot1", "device9", 1, 9);
M2_DevComponent m2_slot1_dev10("slot1", "device10", 1, 10);
M2_DevComponent m2_slot1_dev11("slot1", "device11", 1, 11);
// slot3 device from 0 to 11
M2_DevComponent m2_slot3_dev0("slot3", "device0", 3, 0);
M2_DevComponent m2_slot3_dev1("slot3", "device1", 3, 1);
M2_DevComponent m2_slot3_dev2("slot3", "device2", 3, 2);
M2_DevComponent m2_slot3_dev3("slot3", "device3", 3, 3);
M2_DevComponent m2_slot3_dev4("slot3", "device4", 3, 4);
M2_DevComponent m2_slot3_dev5("slot3", "device5", 3, 5);
M2_DevComponent m2_slot3_dev6("slot3", "device6", 3, 6);
M2_DevComponent m2_slot3_dev7("slot3", "device7", 3, 7);
M2_DevComponent m2_slot3_dev8("slot3", "device8", 3, 8);
M2_DevComponent m2_slot3_dev9("slot3", "device9", 3, 9);
M2_DevComponent m2_slot3_dev10("slot3", "device10", 3, 10);
M2_DevComponent m2_slot3_dev11("slot3", "device11", 3, 11);
#endif

#endif
