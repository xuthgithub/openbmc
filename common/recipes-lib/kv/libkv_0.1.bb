# Copyright 2016-present Facebook. All Rights Reserved.
#
# This program file is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program in a file named COPYING; if not, write to the
# Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor,
# Boston, MA 02110-1301 USA

SUMMARY = "KV Store Library"
DESCRIPTION = "library for get set of kv pairs"
SECTION = "base"
PR = "r1"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://kv.c;beginline=4;endline=16;md5=da35978751a9d71b73679307c4d296ec"

BBCLASSEXTEND = "native"

SRC_URI = "file://Makefile \
           file://kv.c \
           file://kv.h \
           file://kv \
           file://kv.py \
          "

S = "${WORKDIR}"

RDEPENDS_${PN} += "python3-core bash"
inherit distutils3
python() {
  if d.getVar('DISTRO_CODENAME', True) == 'rocko':
    d.setVar('INHERIT', 'python3-dir')
  else:
    d.setVar('INHERIT', 'python-dir')
}

distutils3_do_configure(){
    :
}

do_compile() {
  make
}

do_install() {
    install -d ${D}${bindir}
    install -m 755 kv ${D}${bindir}/kv

    install -d ${D}${PYTHON_SITEPACKAGES_DIR}
    install -m 644 kv.py ${D}${PYTHON_SITEPACKAGES_DIR}/

	  install -d ${D}${libdir}
    install -m 0644 libkv.so ${D}${libdir}/libkv.so

    install -d ${D}${includedir}/openbmc
    install -m 0644 kv.h ${D}${includedir}/openbmc/kv.h
}

FILES_${PN} = "${libdir}/libkv.so ${bindir}/kv ${PYTHON_SITEPACKAGES_DIR}/kv.py"
FILES_${PN}-dev = "${includedir}/openbmc/kv.h"
