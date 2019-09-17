# Copyright 2015-present Facebook. All Rights Reserved.
SUMMARY = "Bridge IC Library"
DESCRIPTION = "library for communicating with Bridge IC"
SECTION = "base"
PR = "r1"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://bic.c;beginline=8;endline=20;md5=da35978751a9d71b73679307c4d296ec"


SRC_URI = "file://bic \
          "
CFLAGS += "-Werror"
LDFLAGS += "-lobmc-i2c -llog -lipmb -lkv"
DEPENDS += " libipmi libipmb libkv libobmc-i2c liblog"
RDEPENDS_${PN} += " libipmi libipmb libkv libobmc-i2c liblog"

S = "${WORKDIR}/bic"

do_install() {
	  install -d ${D}${libdir}
    install -m 0644 libbic.so ${D}${libdir}/libbic.so

    install -d ${D}${includedir}/facebook
    install -m 0644 bic.h ${D}${includedir}/facebook/bic.h
}

FILES_${PN} = "${libdir}/libbic.so"
FILES_${PN}-dev = "${includedir}/facebook/bic.h"
