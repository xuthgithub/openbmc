SUMMARY = "Library for IPv6 Neighbor Discovery Protocol"
HOMEPAGE = "http://libndp.org/"
LICENSE = "LGPLv2.1"
LIC_FILES_CHKSUM = "file://COPYING;md5=4fbd65380cdd255951079008b364516c"

SRC_URI = "git://github.com/jpirko/libndp \
           "
SRCREV = "81df977dc0c2a04d5fa0c18dee130490d84a92f5"
S = "${WORKDIR}/git"

inherit autotools
