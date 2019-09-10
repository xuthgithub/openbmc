#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <openbmc/pal.h>
#include "bios.h"

#define BIOS_VER_REGION_SIZE (4*1024*1024)
#define BIOS_ERASE_PKT_SIZE  (64*1024)

using namespace std;

int BiosComponent::_check_image(const char *path) {
  int fd = 0, ret = -1;
  int offs, rcnt, end;
  uint8_t *buf;
  uint8_t ver_sig[] = { 0x46, 0x49, 0x44, 0x04, 0x78, 0x00 };
  struct stat st;

  if (stat(path, &st)) {
    return -1;
  }

  if (st.st_size < BIOS_VER_REGION_SIZE) {
    return -1;
  }

  buf = (uint8_t *)malloc(BIOS_VER_REGION_SIZE);
  if (!buf) {
    return -1;
  }

  do {
    fd = open(path, O_RDONLY);
    if (fd < 0) {
      break;
    }

    offs = st.st_size - BIOS_VER_REGION_SIZE;
    if (lseek(fd, offs, SEEK_SET) != (off_t)offs) {
      printf("%s: lseek to %d failed\n", __func__, offs);
      break;
    }

    offs = 0;
    while (offs < BIOS_VER_REGION_SIZE) {
      rcnt = ((offs + BIOS_ERASE_PKT_SIZE) <= BIOS_VER_REGION_SIZE) ? BIOS_ERASE_PKT_SIZE : (BIOS_VER_REGION_SIZE - offs);
      rcnt = read(fd, (buf + offs), rcnt);
      if (rcnt <= 0) {
        if (errno == EINTR) {
          continue;
        }
        printf("%s: unexpected rcnt: %d", __func__, rcnt);
        break;
      }
      offs += rcnt;
    }
    if (offs < BIOS_VER_REGION_SIZE) {
      break;
    }

    end = BIOS_VER_REGION_SIZE - (sizeof(ver_sig) + _ver_prefix.length());
    for (offs = 0; offs < end; offs++) {
      if (!memcmp(buf+offs, ver_sig, sizeof(ver_sig))) {
        if (!memcmp(buf+offs+sizeof(ver_sig), _ver_prefix.data(), _ver_prefix.length())) {
          ret = 0;
        }
        break;
      }
    }
  } while (0);

  free(buf);
  if (fd > 0) {
    close(fd);
  }

  return ret;
}

int BiosComponent::_update(const char *path, uint8_t force) {
  int ret = -1;
  int mtdno, found = 0;
  char line[128], name[32];
  uint8_t fruid = 1;
  FILE *fp;

  pal_get_fru_id((char *)_fru.c_str(), &fruid);

  if (!force) {
    ret = pal_check_fw_image(fruid, _component.c_str(), path);
    if (ret == PAL_ENOTSUP) {
      ret = _check_image(path);
    }

    if (ret) {
      cerr << "Invalid image. Stopping the update!" << endl;
      return -1;
    }
  }

  ret = pal_fw_update_prepare(fruid, _component.c_str());
  if (ret) {
    return -1;
  }

  fp = fopen("/proc/mtd", "r");
  if (fp) {
    while (fgets(line, sizeof(line), fp)) {
      if (sscanf(line, "mtd%d: %*x %*x %s", &mtdno, name) == 2) {
        if (!strcmp(_mtd_name.c_str(), name)) {
          found = 1;
          break;
        }
      }
    }
    fclose(fp);
  }

  if (found) {
    snprintf(line, sizeof(line), "flashcp -v %s /dev/mtd%d", path, mtdno);
    ret = system(line);
    ret = (ret < 0) ? FW_STATUS_FAILURE : WEXITSTATUS(ret);
  } else {
    ret = FW_STATUS_FAILURE;
  }

  ret = pal_fw_update_finished(fruid, _component.c_str(), ret);

  return ret;
}

int BiosComponent::update(string image) {
  return _update(image.c_str(), 0);
}

int BiosComponent::fupdate(string image) {
  return _update(image.c_str(), 1);
}

int BiosComponent::print_version() {
  uint8_t ver[32] = {0};
  uint8_t fruid = 1;
  int i, end;

  pal_get_fru_id((char *)_fru.c_str(), &fruid);
  if (!pal_get_sysfw_ver(fruid, ver)) {
    // BIOS version response contains the length at offset 2 followed by ascii string
    if ((end = 3+ver[2]) > (int)sizeof(ver)) {
      end = sizeof(ver);
    }
    printf("BIOS Version: ");
    for (i = 3; i < end; i++) {
      printf("%c", ver[i]);
    }
    printf("\n");
  } else {
    cout << "BIOS Version: NA" << endl;
  }

  return 0;
}
