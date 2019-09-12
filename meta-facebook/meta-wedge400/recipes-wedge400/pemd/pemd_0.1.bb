# Copyright 2019-present Facebook. All Rights Reserved.
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

SUMMARY = "PEM Sensor Monitoring Daemon"
DESCRIPTION = "Daemon for monitoring the PEM sensors"
SECTION = "base"
PR = "r1"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://pemd.c;beginline=4;endline=16;md5=219631f7c3c1e390cdedab9df4b744fd"

SRC_URI = "file://Makefile \
           file://pemd.c \
           file://setup-pemd.sh \
           file://run-pemd.sh \
          "

S = "${WORKDIR}"

binfiles = "pemd \
           "

CFLAGS += " -llog -lpal -lwedge400-pem "

DEPENDS += " liblog libpal libwedge400-pem update-rc.d-native "
RDEPENDS_${PN} += " liblog libpal libwedge400-pem "

pkgdir = "pemd"

do_install() {
  dst="${D}/usr/local/fbpackages/${pkgdir}"
  bin="${D}/usr/local/bin"
  install -d $dst
  install -d $bin
  for f in ${binfiles}; do
    install -m 755 $f ${dst}/$f
    ln -snf ../fbpackages/${pkgdir}/$f ${bin}/$f
  done
  install -d ${D}${sysconfdir}/init.d
  install -d ${D}${sysconfdir}/rcS.d
  install -d ${D}${sysconfdir}/sv
  install -d ${D}${sysconfdir}/sv/pemd
  install -d ${D}${sysconfdir}/pemd
  install -m 755 setup-pemd.sh ${D}${sysconfdir}/init.d/setup-pemd.sh
  install -m 755 run-pemd.sh ${D}${sysconfdir}/sv/pemd/run
  update-rc.d -r ${D} setup-pemd.sh start 95 5 .
}

FBPACKAGEDIR = "${prefix}/local/fbpackages"

FILES_${PN} = "${FBPACKAGEDIR}/pemd ${prefix}/local/bin ${sysconfdir} "


INHIBIT_PACKAGE_DEBUG_SPLIT = "1"
INHIBIT_PACKAGE_STRIP = "1"
