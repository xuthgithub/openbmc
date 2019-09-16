FILESEXTRAPATHS_prepend := "${THISDIR}/files/pal:"

SRC_URI += "file://pal.c \
            file://pal.h \
            "

DEPENDS += "libbic libsensor-correction libobmc-i2c libmisc-utils"
RDEPENDS_${PN} += " libbic libsensor-correction libobmc-i2c libmisc-utils"
LDFLAGS += " -lbic -lsensor-correction -lm -lobmc-i2c -lmisc-utils"
