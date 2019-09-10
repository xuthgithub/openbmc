#!/usr/bin/env python
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
import unittest

from common.base_process_running_test import BaseProcessRunningTest


class ProcessRunningTest(BaseProcessRunningTest, unittest.TestCase):
    def set_processes(self):
        self.expected_process = [
            "dhclient -6 -d -D LL -pf /var/run/dhclient6.eth0.pid eth0",
            "dhclient -pf /var/run/dhclient.eth0.pid eth0",
            "front-paneld",
            "gpiod",
            "ipmbd",
            "ipmid",
            "mTerm_server",
            "pemd",
            "rest.py",
            "restapi",
            "rsyslogd",
            "sensord",
            "spatula_wrapper.py",
            "usbmon.sh",
        ]
