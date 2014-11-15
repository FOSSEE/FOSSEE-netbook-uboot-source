/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <common.h>
#include <command.h>

#include "include/wmt_pmc.h"
#include "include/common_def.h"

#define PMHC_HIBERNATE 0x205

void do_wmt_poweroff(void)
{
	/*
	 * Set scratchpad to zero, just in case it is used as a restart
	 * address by the bootloader. Since PB_RESUME button has been
	 * set to be one of the wakeup sources, clean the resume address
	 * will cause zacboot to issue a SW_RESET, for design a behavior
	 * to let PB_RESUME button be a power on button.
	 *
	 * Also force to disable watchdog timer, if it has been enabled.
	 */
	HSP0_VAL = 0;
	OSTW_VAL &= ~OSTW_WE;

	/*
	 * Well, I cannot power-off myself,
	 * so try to enter power-off suspend mode.
	 */
	PMHC_VAL = PMHC_HIBERNATE;

	/* Force ARM to idle mode*/
	asm volatile("ldr r0, =0xD813004C\n\t"
		     "adr r1, .Cpu1_wfi\n\t"
		     "str r1, [r0]\n\t"
		     "sev\n\t"
		     "ldr r0, =0xD8018008  @SCU\n\t"
		     "ldr r1, =0x0303\n\t"
		     "str  r1, [r0]\n\t"
		     "ldr  r0, =0xD8130012\n\t"
		     "ldr  r1, =0x205\n\t"
		     "strh r1, [r0]\n\t"
		     "wfi\n\t"
		     ".Cpu1_wfi:\n\t"
		     "wfi\n\t"
		     "b .Cpu1_wfi\n\t"
		    );
}

U_BOOT_CMD(
	poweroff, 1,	0,	do_wmt_poweroff,
	"poweroff - wmt device power off. \n",
	"- wmt device power off.\n"
);

