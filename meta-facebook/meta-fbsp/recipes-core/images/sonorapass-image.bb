# Copyright 2018-present Facebook. All Rights Reserved.

inherit kernel_fitimage

require recipes-core/images/fb-openbmc-image.bb

# Include modules in rootfs
IMAGE_INSTALL += " \
  packagegroup-openbmc-base \
  packagegroup-openbmc-net \
  packagegroup-openbmc-python3 \
  packagegroup-openbmc-rest3 \
  packagegroup-openbmc-emmc \
  openbmc-utils \
  plat-utils \
  fscd \
  fan-util \
  power-util \
  mterm \
  front-paneld \
  ipmid \
  fruid \
  sensor-util \
  sensor-mon \
  log-util-v2 \
  kcsd \
  healthd \
  fpc-util \
  fw-util \
  cfg-util \
  ipmi-util \
  peci-util-v2 \
  asd \
  asd-test \
  ipmitool \
  bios-util \
  vboot-utils \
  libncsi \
  ncsi-util \
  ncsid \
  libpldm \
  gpiod \
  guid-util \
  ipmbd\
  ipmb-util\
  me-util \
  i2craw \
  "
