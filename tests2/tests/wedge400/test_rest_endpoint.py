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
import json
import unittest

from common.base_rest_endpoint_test import FbossRestEndpointTest
from tests.wedge400.test_data.sensors.sensors import SENSORS


class RestEndpointTest(FbossRestEndpointTest, unittest.TestCase):
    """
    Input data to the test needs to be a list like below.
    User can choose to sends these lists from jsons too.
    """

    FRUID_SCM_ENDPOINT = "/api/sys/seutil"
    FRUID_SMB_ENDPOINT = "/api/sys/mb/fruid"
    FRUID_FCM_ENDPOINT = "/api/sys/feutil/fcm"
    FRUID_FAN1_ENDPOINT = "/api/sys/feutil/fan1"
    FRUID_FAN2_ENDPOINT = "/api/sys/feutil/fan2"
    FRUID_FAN3_ENDPOINT = "/api/sys/feutil/fan3"
    FRUID_FAN4_ENDPOINT = "/api/sys/feutil/fan4"
    SCM_PRESENT_ENDPOINT = "/api/sys/presence/scm"
    PSU_PRESENT_ENDPOINT = "/api/sys/presence/psu"
    FAN_PRESENT_ENDPOINT = "/api/sys/presence/fan"
    CPLD_FIRMWARE_INFO_ENDPOINT = "/api/sys/firmware_info/cpld"
    FPGA_FIRMWARE_INFO_ENDPOINT = "/api/sys/firmware_info/fpga"
    SCM_FIRMWARE_INFO_ENDPOINT = "/api/sys/firmware_info/scm"

    # "/api/sys"
    def set_endpoint_sys_attributes(self):
        self.endpoint_sys_attrb = [
            "server",
            "bmc",
            "mb",
            "slotid",
            "firmware_info",
            "presence",
            "feutil",
            "seutil",
            "psu_update",
            "gpios",
            "sensors",
            "mTerm_status",
        ]

    # "/api/sys/sensors"
    def set_endpoint_sensors_attributes(self):
        self.endpoint_sensors_attrb = SENSORS

    def test_endpoint_api_sys_sensors(self):
        self.set_endpoint_sensors_attributes()
        self.verify_endpoint_attributes(
            RestEndpointTest.SENSORS_ENDPOINT, self.endpoint_sensors_attrb
        )

    # For sensors endpoint, custom verify attributes function
    def verify_endpoint_attributes(self, endpointname, attributes):
        """
        Verify if attributes are present in endpoint response
        """
        self.assertNotEqual(
            attributes, None, "{} endpoint attributes not set".format(endpointname)
        )
        info = self.get_from_endpoint(endpointname)
        fail_attr = []
        for attrib in attributes:
            if not attrib in info:
                fail_attr.append(attrib)
                continue
            with self.subTest(attrib=attrib):
                self.assertIn(attrib, info)
        if "Resources" in info:
            dict_info = json.loads(info)
            self.verify_endpoint_resource(
                endpointname=endpointname, resources=dict_info["Resources"]
            )
        if len(fail_attr) > 0:
            self.fail("attributes {} not in {}".format(fail_attr, info))

    # "/api/sys/mb/fruid"
    def set_endpoint_fruid_attributes(self):
        self.endpoint_fruid_attrb = self.FRUID_ATTRIBUTES

    # "/api/sys/server"
    def set_endpoint_server_attributes(self):
        self.endpoint_server_attrb = ["BIC_ok", "status"]

    # "/api/sys/slotid"
    def set_endpoint_slotid_attributes(self):
        self.endpoint_slotid_attrb = ["1"]

    # "/api/sys/seutil"
    def set_endpoint_fruid_scm_attributes(self):
        self.endpoint_fruid_scm_attrb = self.FRUID_ATTRIBUTES

    def test_endpoint_api_sys_fruid_scm(self):
        self.set_endpoint_fruid_scm_attributes()
        self.verify_endpoint_attributes(
            RestEndpointTest.FRUID_SCM_ENDPOINT, self.endpoint_fruid_scm_attrb
        )

    # "/api/sys/mb/fruid"
    def set_endpoint_mb_fruid_attributes(self):
        self.endpoint_mb_fruid_attrb = self.FRUID_ATTRIBUTES

    def test_endpoint_api_sys_seutil_fruid(self):
        self.set_endpoint_mb_fruid_attributes()
        self.verify_endpoint_attributes(
            RestEndpointTest.FRUID_SMB_ENDPOINT, self.endpoint_mb_fruid_attrb
        )

    # "/api/sys/feutil/fcm"
    def set_endpoint_fruid_fcm_attributes(self):
        self.endpoint_fruid_fcm_attrb = self.FRUID_ATTRIBUTES

    def test_endpoint_api_sys_fruid_fcm(self):
        self.set_endpoint_fruid_fcm_attributes()
        self.verify_endpoint_attributes(
            RestEndpointTest.FRUID_FCM_ENDPOINT, self.endpoint_fruid_fcm_attrb
        )

    # "/api/sys/feutil/fan1"
    def set_endpoint_fruid_fan1_attributes(self):
        self.endpoint_fruid_fan1_attrb = self.FRUID_ATTRIBUTES

    def test_endpoint_api_sys_fruid_fan1(self):
        self.set_endpoint_fruid_fan1_attributes()
        self.verify_endpoint_attributes(
            RestEndpointTest.FRUID_FAN1_ENDPOINT, self.endpoint_fruid_fan1_attrb
        )

    # "/api/sys/feutil/fan2"
    def set_endpoint_fruid_fan2_attributes(self):
        self.endpoint_fruid_fan2_attrb = self.FRUID_ATTRIBUTES

    def test_endpoint_api_sys_fruid_fan2(self):
        self.set_endpoint_fruid_fan2_attributes()
        self.verify_endpoint_attributes(
            RestEndpointTest.FRUID_FAN2_ENDPOINT, self.endpoint_fruid_fan2_attrb
        )

    # "/api/sys/feutil/fan3"
    def set_endpoint_fruid_fan3_attributes(self):
        self.endpoint_fruid_fan3_attrb = self.FRUID_ATTRIBUTES

    def test_endpoint_api_sys_fruid_fan3(self):
        self.set_endpoint_fruid_fan3_attributes()
        self.verify_endpoint_attributes(
            RestEndpointTest.FRUID_FAN3_ENDPOINT, self.endpoint_fruid_fan3_attrb
        )

    # "/api/sys/feutil/fan4"
    def set_endpoint_fruid_fan4_attributes(self):
        self.endpoint_fruid_fan4_attrb = self.FRUID_ATTRIBUTES

    def test_endpoint_api_sys_fruid_fan4(self):
        self.set_endpoint_fruid_fan4_attributes()
        self.verify_endpoint_attributes(
            RestEndpointTest.FRUID_FAN4_ENDPOINT, self.endpoint_fruid_fan4_attrb
        )

    # "/api/sys/presence/scm"
    def set_endpoint_scm_presence_attributes(self):
        self.endpoint_scm_presence = ["scm"]

    def test_endpoint_api_scm_present(self):
        self.set_endpoint_scm_presence_attributes()
        self.verify_endpoint_attributes(
            RestEndpointTest.SCM_PRESENT_ENDPOINT, self.endpoint_scm_presence
        )

    # "/api/sys/presence/psu"
    def set_endpoint_psu_presence_attributes(self):
        self.endpoint_psu_presence = [
            "psu1",
            "psu2"
        ]

    def test_endpoint_api_psu_present(self):
        self.set_endpoint_psu_presence_attributes()
        self.verify_endpoint_attributes(
            RestEndpointTest.PSU_PRESENT_ENDPOINT, self.endpoint_psu_presence
        )

    # "/api/sys/presence/fan"
    def set_endpoint_fan_presence_attributes(self):
        self.endpoint_fan_presence = [
            "fan1",
            "fan2",
            "fan3",
            "fan4"
        ]

    def test_endpoint_api_fan_present(self):
        self.set_endpoint_fan_presence_attributes()
        self.verify_endpoint_attributes(
            RestEndpointTest.FAN_PRESENT_ENDPOINT, self.endpoint_fan_presence
        )

    # "/api/sys/firmware_info/cpld"
    def set_endpoint_firmware_info_cpld_attributes(self):
        self.endpoint_firmware_info_cpld_attributes = [
            "SCMCPLD",
            "SMB_SYSCPLD",
            "SMB_PWRCPLD",
            "FCMCPLD",
        ]

    def test_endpoint_api_sys_firmware_info_cpld(self):
        self.set_endpoint_firmware_info_cpld_attributes()
        self.verify_endpoint_attributes(
            RestEndpointTest.CPLD_FIRMWARE_INFO_ENDPOINT,
            self.endpoint_firmware_info_cpld_attributes,
        )

    # "/api/sys/firmware_info/fpga"
    def set_endpoint_firmware_info_fpga_attributes(self):
        self.endpoint_firmware_info_fpga_attributes = [
            "DOMFPGA1",
            "DOMFPGA2",
        ]

    def test_endpoint_api_sys_firmware_info_fpga(self):
        self.set_endpoint_firmware_info_fpga_attributes()
        self.verify_endpoint_attributes(
            RestEndpointTest.FPGA_FIRMWARE_INFO_ENDPOINT,
            self.endpoint_firmware_info_fpga_attributes,
        )

    # "/api/sys/firmware_info/scm"
    def set_endpoint_firmware_info_scm_attributes(self):
        self.endpoint_firmware_info_scm_attributes = [
            "CPLD Version",
            "Bridge-IC Version",
            "ME Version",
            "Bridge-IC Bootloader Version",
            "PVCCIN VR Version",
            "DDRAB VR Version",
            "P1V05 VR Version",
            "BIOS Version"
        ]

    def test_endpoint_api_sys_firmware_info_scm(self):
        self.set_endpoint_firmware_info_scm_attributes()
        self.verify_endpoint_attributes(
            RestEndpointTest.SCM_FIRMWARE_INFO_ENDPOINT,
            self.endpoint_firmware_info_scm_attributes,
        )

    # "/api/sys/firmware_info_all"
    def test_endpoint_api_sys_firmware_info_all(self):
        self.skipTest("not support")
