#
# See file CREDITS for list of people who contributed to this
# project.
#
# Some descriptions of such software. Copyright (c) 2008 WonderMedia Technologies, Inc.
#
# This program is free software: you can redistribute it and/or modify it under the
# terms of the GNU General Public License as published by the Free Software Foundation,
# either version 2 of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.  See the GNU General Public License for more details.
# You should have received a copy of the GNU General Public License along with
# this program.  If not, see <http://www.gnu.org/licenses/>.
#
# WonderMedia Technologies, Inc.
# 10F, 529, Chung-Cheng Road, Hsin-Tien, Taipei 231, R.O.C.
#

include $(TOPDIR)/config.mk

LIB	= lib$(BOARD).a

OBJS	:= wmt.o flash.o main.o \
           flash/spi_flash_lock.o flash/spi_flash.o flash/nand_flash.o \
           wmt_clk.o env.o ehci-hcd.o usb_uhci.o wmt_i2c.o wmt_i2c_1.o \
           wmt_i2c_2.o wmt_i2c_3.o wmt_spi_0.o wmt_ost.o wmt_gpio.o poweroff.o \
           wmt_battery/wmt_battery.o \
           wmt_battery/charger/g2214/g2214_charger.o \
           wmt_battery/charger/mp2625/mp2625_charger.o \
           wmt_battery/gauge/upi/uG31xx_API_Measurement.o \
           wmt_battery/gauge/upi/uG31xx_API_Otp.o \
           wmt_battery/gauge/upi/uG31xx_API_System.o \
           wmt_battery/gauge/upi/ug31xx_boot.o \
           wmt_battery/gauge/upi/ug31xx_boot_i2c.o \
           wmt_battery/gauge/vt1603/vt1603_battery.o \
           wmt_battery/gauge/sp2541/sp2541_battery.o \
           wmt_battery/gauge/bq27541/bq_battery_i2c.o \
	   vt1603/snd-vt1603.o

SOBJS	:= lowlevel_init.o

$(LIB):	$(OBJS) $(SOBJS)
	$(AR) crv $@ $^

clean:
	rm -f $(SOBJS) $(OBJS)

distclean:	clean
	rm -f $(LIB) core *.bak .depend

#########################################################################

.depend:	Makefile $(SOBJS:.o=.S) $(OBJS:.o=.c)
		$(CC) -M $(CPPFLAGS) $(SOBJS:.o=.S) $(OBJS:.o=.c) > $@

-include .depend

#########################################################################
