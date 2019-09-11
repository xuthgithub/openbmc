#!/usr/bin/env python3
#
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
#

from board_endpoint import boardApp_Handler
from boardroutes import *


def setup_board_routes(app):
    bhandler = boardApp_Handler()
    app.router.add_get(board_routes[0], bhandler.rest_seutil_hdl)
    app.router.add_get(board_routes[1], bhandler.rest_firmware_info_hdl)
    app.router.add_get(board_routes[2], bhandler.rest_firmware_info_cpld_hdl)
    app.router.add_get(board_routes[3], bhandler.rest_firmware_info_fpga_hdl)
    app.router.add_get(board_routes[4], bhandler.rest_firmware_info_scm_hdl)
    app.router.add_get(board_routes[5], bhandler.rest_presence_hdl)
    app.router.add_get(board_routes[6], bhandler.rest_presence_scm_hdl)
    app.router.add_get(board_routes[7], bhandler.rest_presence_fan_hdl)
    app.router.add_get(board_routes[8], bhandler.rest_presence_psu_hdl)
    app.router.add_get(board_routes[9], bhandler.rest_feutil_hdl)
    app.router.add_get(board_routes[10], bhandler.rest_feutil_all_hdl)
    app.router.add_get(board_routes[11], bhandler.rest_feutil_fcm_hdl)
    app.router.add_get(board_routes[12], bhandler.rest_feutil_fan1_hdl)
    app.router.add_get(board_routes[13], bhandler.rest_feutil_fan2_hdl)
    app.router.add_get(board_routes[14], bhandler.rest_feutil_fan3_hdl)
    app.router.add_get(board_routes[15], bhandler.rest_feutil_fan4_hdl)