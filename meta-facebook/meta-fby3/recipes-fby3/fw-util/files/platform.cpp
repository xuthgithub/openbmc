#include "bic_fw.h"
#include "bic_me.h"
#include "bic_cpld.h"
#include "bmc_cpld.h"
#include "bios.h"
#include "fscd.h"
#ifdef BIC_SUPPORT
#include <facebook/bic.h>

// Register BIC FW and BL components
/****************
 *param1 - string fru
 *param2 - string board
 *param3 - string comp
 *param4 - uint8_t slot_id
 *param5 - uint8_t intf
*****************/

/*The BIC of the server board*/
BicFwComponent bicfw1("slot1", "sb", "bic", FRU_SLOT1, NONE_INTF);
BicFwComponent bicfw2("slot2", "sb", "bic", FRU_SLOT2, NONE_INTF);
BicFwComponent bicfw3("slot3", "sb", "bic", FRU_SLOT3, NONE_INTF);
BicFwComponent bicfw4("slot4", "sb", "bic", FRU_SLOT4, NONE_INTF);

BicFwBlComponent bicfwbl1("slot1", "sb", "bicbl", FRU_SLOT1, NONE_INTF);
BicFwBlComponent bicfwbl2("slot2", "sb", "bicbl", FRU_SLOT2, NONE_INTF);
BicFwBlComponent bicfwbl3("slot3", "sb", "bicbl", FRU_SLOT3, NONE_INTF);
BicFwBlComponent bicfwbl4("slot4", "sb", "bicbl", FRU_SLOT4, NONE_INTF);

/*The BIC of the 1OU board*/
BicFwExtComponent bic_1ou_fw1("slot1", "1ou", "bic", FRU_SLOT1, FEXP_BIC_INTF);
BicFwExtComponent bic_1ou_fw2("slot2", "1ou", "bic", FRU_SLOT2, FEXP_BIC_INTF);
BicFwExtComponent bic_1ou_fw3("slot3", "1ou", "bic", FRU_SLOT3, FEXP_BIC_INTF);
BicFwExtComponent bic_1ou_fw4("slot4", "1ou", "bic", FRU_SLOT4, FEXP_BIC_INTF);

BicFwBlExtComponent bic_1ou_fwbl1("slot1", "1ou", "bicbl", FRU_SLOT1, FEXP_BIC_INTF);
BicFwBlExtComponent bic_1ou_fwbl2("slot2", "1ou", "bicbl", FRU_SLOT2, FEXP_BIC_INTF);
BicFwBlExtComponent bic_1ou_fwbl3("slot3", "1ou", "bicbl", FRU_SLOT3, FEXP_BIC_INTF);
BicFwBlExtComponent bic_1ou_fwbl4("slot4", "1ou", "bicbl", FRU_SLOT4, FEXP_BIC_INTF);

/*The BIC of the 2OU board*/
BicFwExtComponent bic_2ou_fw1("slot1", "2ou", "bic", FRU_SLOT1, REXP_BIC_INTF);
BicFwExtComponent bic_2ou_fw2("slot2", "2ou", "bic", FRU_SLOT2, REXP_BIC_INTF);
BicFwExtComponent bic_2ou_fw3("slot3", "2ou", "bic", FRU_SLOT3, REXP_BIC_INTF);
BicFwExtComponent bic_2ou_fw4("slot4", "2ou", "bic", FRU_SLOT4, REXP_BIC_INTF);

BicFwBlExtComponent bic_2ou_fwbl1("slot1", "2ou", "bicbl", FRU_SLOT1, REXP_BIC_INTF);
BicFwBlExtComponent bic_2ou_fwbl2("slot2", "2ou", "bicbl", FRU_SLOT2, REXP_BIC_INTF);
BicFwBlExtComponent bic_2ou_fwbl3("slot3", "2ou", "bicbl", FRU_SLOT3, REXP_BIC_INTF);
BicFwBlExtComponent bic_2ou_fwbl4("slot4", "2ou", "bicbl", FRU_SLOT4, REXP_BIC_INTF);

/*The BIC of the baseboard*/
BicFwExtComponent bic_bb_fw1("slot1", "bb", "bic", FRU_SLOT1, BB_BIC_INTF);
BicFwBlExtComponent bic_bb_fwbl1("slot1", "bb", "bicbl", FRU_SLOT1, BB_BIC_INTF);

/*The ME of the serverboard*/
MeFwComponent me_fw1("slot1", "sb", "me", FRU_SLOT1, NONE_INTF);
MeFwComponent me_fw2("slot2", "sb", "me", FRU_SLOT2, NONE_INTF);
MeFwComponent me_fw3("slot3", "sb", "me", FRU_SLOT3, NONE_INTF);
MeFwComponent me_fw4("slot4", "sb", "me", FRU_SLOT4, NONE_INTF);

/*The CPLD of the server board*/
CpldComponent cpld_fw1("slot1", "sb", "cpld", FRU_SLOT1, 2, 0x80, NONE_INTF);
CpldComponent cpld_fw2("slot2", "sb", "cpld", FRU_SLOT2, 2, 0x80, NONE_INTF);
CpldComponent cpld_fw3("slot3", "sb", "cpld", FRU_SLOT3, 2, 0x80, NONE_INTF);
CpldComponent cpld_fw4("slot4", "sb", "cpld", FRU_SLOT4, 2, 0x80, NONE_INTF);

/*The CPLD of BMC*/
BmcCpldComponent cpld_bmc("bmc", "bmc", "cpld", 12, 0x80);

/*The BIOS of the server board*/
BiosComponent bios_fw1("slot1", "sb", "bios", FRU_SLOT1, NONE_INTF);
BiosComponent bios_fw2("slot2", "sb", "bios", FRU_SLOT2, NONE_INTF);
BiosComponent bios_fw3("slot3", "sb", "bios", FRU_SLOT3, NONE_INTF);
BiosComponent bios_fw4("slot4", "sb", "bios", FRU_SLOT4, NONE_INTF);

/*The fscd of BMC*/
FscdComponent fscd("bmc", "bmc", "fscd");

#endif
