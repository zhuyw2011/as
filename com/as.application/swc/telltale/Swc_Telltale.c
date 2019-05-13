/**
 * AS - the open source Automotive Software on https://github.com/parai
 *
 * Copyright (C) 2017 AS <parai@foxmail.com>
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation; See <http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
/* ============================ [ INCLUDES  ] ====================================================== */
#include "Rte_Telltale.h"
/* ============================ [ MACROS    ] ====================================================== */
/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */



void Telltale_run(void)
{	/* period is 20ms */
	Rte_Write_Telltale_AirbagState(OnOff_On);
	Rte_Write_Telltale_AutoCruiseState(OnOff_1Hz);
	Rte_Write_Telltale_HighBeamState(OnOff_3Hz);
	Rte_Write_Telltale_LowOilState(OnOff_1Hz);
	Rte_Write_Telltale_PosLampState(OnOff_2Hz);
	Rte_Write_Telltale_SeatbeltDriverState(OnOff_3Hz);
	Rte_Write_Telltale_SeatbeltPassengerState(OnOff_1Hz);
	Rte_Write_Telltale_TPMSState(OnOff_2Hz);
	Rte_Write_Telltale_TurnLeftState(OnOff_3Hz);
	Rte_Write_Telltale_TurnRightState(OnOff_3Hz);
}
