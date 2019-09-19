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

import json
import re
import subprocess

from rest_utils import DEFAULT_TIMEOUT_SEC


#
# "SENSORS" REST API handler for Wedge400
#
# Wedge400 sensor REST API handler is unique in that,
# it uses rest-api-1, but still uses sensor-util, instead of sensor.
# We are forced to use sensor-util, as Wedge400 has too many resources
# for BMC to control. If we use conventional way of fetching sensor data
# (sensors command), it will take too long. So we use sensor-util, which
# will use the cached value. But the output format of sensor-util is quite
# different from sensors command. So we need a separate REST api handler
# for this.
#
def get_fru_sensor(fru):
    cmd = "/usr/local/bin/sensor-util"
    proc = subprocess.Popen([cmd, fru], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        data, err = proc.communicate(timeout=DEFAULT_TIMEOUT_SEC)
        data = data.decode()
    except proc.TimeoutError as ex:
        data = ex.output
        data = data.decode()
        err = ex.error

    # We flatten all key value pair into one level
    # We keep the JSON format compatible with other
    # FBOSS chassis, to make bmc_proxy happy
    result = {}
    result["name"] = "util"
    result["Adapter"] = "util"
    if "not present" in data:
        result["present"] = False
        return result

    result["present"] = True
    for edata in data.split("\n"):
        # Per each line
        adata = edata.split()
        # For each key value pair
        if len(adata) < 4:
            continue
        key = adata[0].strip()
        value = adata[3].strip()
        try:
            value = float(value)
            result[key] = str(value)
        except Exception:
            result[key] = "NA"
    return result


def get_sensors():
    result = {}
    # FRUs of /usr/local/bin/sensor-util commands
    frus = ["scm", "smb", "psu1", "psu2", "pem1", "pem2"]

    for fru in frus:
        sresult = get_fru_sensor(fru)
        result[fru] = sresult

    fresult = {"Information": result, "Actions": [], "Resources": []}
    return fresult


# This function doesn't seem to be actively used, but I will leave it
# here for any future compatibility issue if any
def get_sensors_full():
    return get_sensors()
