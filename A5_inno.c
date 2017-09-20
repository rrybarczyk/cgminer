#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <stdbool.h>

#include "logging.h"
#include "miner.h"
#include "util.h"

#include "A5_spi-context.h"
#include "A5_inno.h"
#include "A5_inno_cmd.h"
#include "A5_inno_clock.h"
#include "A5_inno_gpio.h"


int opt_diff=15;

static const uint32_t difficult_Tbl[21] = {
	0x1d00ffff,// 1
	0x1d007fff,// 2
	0x1d005fff,// 3
	0x1d003fff,// 4
	0x1d001fff,// 8
	0x1d000fff,// 16
	0x1d0007ff,// 32
	0x1d0006ff,// 37
	0x1d0005ff,// 43
	0x1d0004ff,// 52
	0x1d0003ff,// 65
	0x1d0002ff,// 86
	0x1d00027f,// 103
	0x1d0001ff,// 129
	0x1c00ffff,// 256
	0x1c007fff,// 512
	0x1b3fffff,// 1024
	0x1b1fffff,// 2048
	0x1b0fffff,// 4096
	0x1b07ffff,// 8192
	0x1b03ffff,// 16834
};

static const uint8_t default_reg[142][12] = 
{
    {0x02, 0x50, 0x40, 0xc2, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //120 MHz
    {0x02, 0x53, 0x40, 0xc2, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //125 MHz
    {0x02, 0x56, 0x40, 0xc2, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //129 MHz
    {0x02, 0x5d, 0x40, 0xc2, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //140 MHz
    {0x02, 0x35, 0x40, 0x82, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //159 MHz
    {0x02, 0x39, 0x40, 0x82, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //171 MHz
    {0x02, 0x3c, 0x40, 0x82, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //180 MHz
    {0x02, 0x3f, 0x40, 0x82, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //189 MHz
    {0x02, 0x43, 0x40, 0x82, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //201 MHz
    {0x02, 0x46, 0x40, 0x82, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //210 MHz
    {0x02, 0x49, 0x40, 0x82, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //219 MHz
    {0x02, 0x4d, 0x40, 0x82, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //231 MHz
    {0x02, 0x50, 0x40, 0x82, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //240 MHz
    {0x02, 0x53, 0x40, 0x82, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //249 MHz
    {0x02, 0x57, 0x40, 0x82, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //261 MHz
    {0x02, 0x5a, 0x40, 0x82, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //270 MHz
    {0x02, 0x5d, 0x40, 0x82, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //279 MHz
    {0x02, 0x61, 0x40, 0x82, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //291 MHz
    {0x02, 0x32, 0x40, 0x42, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //300 MHz
    {0x02, 0x34, 0x40, 0x42, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //312 MHz
    {0x02, 0x35, 0x40, 0x42, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //318 MHz
    {0x02, 0x37, 0x40, 0x42, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //330 MHz
    {0x02, 0x39, 0x40, 0x42, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //342 MHz
    {0x02, 0x3a, 0x40, 0x42, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //348 MHz
    {0x02, 0x3c, 0x40, 0x42, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //360 MHz
    {0x02, 0x3e, 0x40, 0x42, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //372 MHz
    {0x02, 0x3f, 0x40, 0x42, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //378 MHz
    {0x02, 0x41, 0x40, 0x42, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //390 MHz
    {0x02, 0x43, 0x40, 0x42, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //402 MHz
    {0x02, 0x44, 0x40, 0x42, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //408 MHz
    {0x02, 0x46, 0x40, 0x42, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //420 MHz
    {0x02, 0x48, 0x40, 0x42, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //432 MHz
    {0x02, 0x49, 0x40, 0x42, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //438 MHz
    {0x02, 0x4b, 0x40, 0x42, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //450 MHz
    {0x02, 0x4d, 0x40, 0x42, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //462 MHz
    {0x02, 0x4e, 0x40, 0x42, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //468 MHz
    {0x02, 0x50, 0x40, 0x42, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //480 MHz
    {0x02, 0x52, 0x40, 0x42, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //492 MHz
    {0x02, 0x53, 0x40, 0x42, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //498 MHz
    {0x02, 0x55, 0x40, 0x42, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //510 MHz
    {0x02, 0x57, 0x40, 0x42, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //522 MHz
    {0x02, 0x58, 0x40, 0x42, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //528 MHz
    {0x02, 0x5a, 0x40, 0x42, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //540 MHz
    {0x02, 0x5c, 0x40, 0x42, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //552 MHz
    {0x02, 0x5d, 0x40, 0x42, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //558 MHz
    {0x02, 0x5f, 0x40, 0x42, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //570 MHz
    {0x02, 0x61, 0x40, 0x42, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //582 MHz
    {0x02, 0x62, 0x40, 0x42, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //588 MHz
    {0x02, 0x32, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x24, 0xff, 0xff},  //600 MHz
    {0x02, 0x33, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //612 MHz
    {0x02, 0x34, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //624 MHz
    {0x04, 0x69, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //630 MHz
    {0x02, 0x35, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //636 MHz
    {0x02, 0x36, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //648 MHz
    {0x02, 0x37, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //660 MHz
    {0x02, 0x38, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //672 MHz
    {0x02, 0x39, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //684 MHz
    {0x04, 0x73, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //690 MHz
    {0x02, 0x3a, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //696 MHz
    {0x02, 0x3b, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //708 MHz
    {0x02, 0x3c, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //720 MHz
    {0x02, 0x3d, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //732 MHz
    {0x02, 0x3e, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //744 MHz
    {0x04, 0x7d, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //750 MHz
    {0x02, 0x3f, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //756 MHz
    {0x02, 0x40, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //768 MHz
    {0x02, 0x41, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //780 MHz
    {0x02, 0x42, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //792 MHz
    {0x02, 0x43, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //804 MHz
    {0x04, 0x87, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //810 MHz
    {0x02, 0x44, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //816 MHz
    {0x02, 0x45, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //828 MHz
    {0x02, 0x46, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //840 MHz
    {0x02, 0x47, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //852 MHz
    {0x02, 0x48, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //864 MHz
    {0x04, 0x91, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //870 MHz
    {0x02, 0x49, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //876 MHz
    {0x02, 0x4a, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //888 MHz
    {0x02, 0x4b, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //900 MHz
    {0x02, 0x4c, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //912 MHz
    {0x02, 0x4d, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //924 MHz
    {0x04, 0x9b, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //930 MHz
    {0x02, 0x4e, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //936 MHz
    {0x02, 0x4f, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //948 MHz
    {0x02, 0x50, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //960 MHz
    {0x02, 0x51, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //972 MHz
    {0x02, 0x52, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //984 MHz
    {0x04, 0xa5, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //990 MHz
    {0x02, 0x53, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //996 MHz
    {0x02, 0x54, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1008 MHz
    {0x02, 0x55, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1020 MHz
    {0x02, 0x56, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1032 MHz
    {0x02, 0x57, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1044 MHz
    {0x04, 0xaf, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1050 MHz
    {0x02, 0x58, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1056 MHz
    {0x02, 0x59, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1068 MHz
    {0x02, 0x5a, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1080 MHz
    {0x02, 0x5b, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1092 MHz
    {0x02, 0x5c, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1104 MHz
    {0x04, 0xb9, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1110 MHz
    {0x02, 0x5d, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1116 MHz
    {0x02, 0x5e, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1128 MHz
    {0x02, 0x5f, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1140 MHz
    {0x02, 0x60, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1152 MHz
    {0x02, 0x61, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1164 MHz
    {0x04, 0xc3, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1170 MHz
    {0x02, 0x62, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1176 MHz
    {0x02, 0x63, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1188 MHz
    {0x02, 0x64, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1200 MHz
    {0x02, 0x65, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1212 MHz
    {0x02, 0x66, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1224 MHz
    {0x02, 0x67, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1236 MHz
    {0x02, 0x68, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1248 MHz
    {0x02, 0x69, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1260 MHz
    {0x02, 0x6a, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1272 MHz
    {0x02, 0x6b, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1284 MHz
    {0x02, 0x6c, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1296 MHz
    {0x02, 0x6d, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1308 MHz
    {0x02, 0x6e, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1320 MHz
    {0x02, 0x6f, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1332 MHz
    {0x02, 0x70, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1344 MHz
    {0x02, 0x71, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1356 MHz
    {0x02, 0x72, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1368 MHz
    {0x02, 0x73, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1380 MHz
    {0x02, 0x74, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1392 MHz
    {0x02, 0x75, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1404 MHz
    {0x02, 0x76, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1416 MHz
    {0x02, 0x77, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1428 MHz
    {0x02, 0x78, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1440 MHz
    {0x02, 0x79, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1452 MHz
    {0x02, 0x7a, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1464 MHz
    {0x02, 0x7b, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1476 MHz
    {0x02, 0x7c, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1488 MHz
    {0x02, 0x7d, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1500 MHz
    {0x02, 0x7e, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1512 MHz
    {0x02, 0x7f, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1524 MHz
    {0x02, 0x80, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1536 MHz
    {0x02, 0x81, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1548 MHz
    {0x02, 0x82, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1560 MHz
    {0x02, 0x83, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1572 MHz
    {0x02, 0x84, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff},  //1584 MHz
    {0x02, 0x85, 0x40, 0x02, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x20, 0xff, 0xff}   //1596 MHz
};


static void rev(unsigned char *s, size_t l)
{
	size_t i, j;
	unsigned char t;

	for (i = 0, j = l - 1; i < j; i++, j--) {
		t = s[i];
		s[i] = s[j];
		s[j] = t;
	}
}

static uint8_t *create_job(uint8_t chip_id, uint8_t job_id, struct work *work)
{
	double sdiff = work->sdiff;
	uint8_t tmp_buf[JOB_LENGTH];
	uint16_t crc;
	uint8_t i;
			
	static uint8_t job[JOB_LENGTH] = {
		/* command */
		0x00, 0x00,
		/* midstate3 */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		/* midstate2 */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		/* midstate1 */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		/* midstate0 */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		/* wdata */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		/* start nonce */
		0x00, 0x00, 0x00, 0x00,
		/* difficulty 1 */
		0xff, 0xff, 0x00, 0x1d,
		/* end nonce */
		0xff, 0xff, 0xff, 0xff,
		0x00, 0x00, 0x00, 0x00,
		/* crc data */
		0x00, 0x00, 0x00, 0x00
	};

	uint32_t *p1 = (uint32_t *) &job[130];
	uint32_t *p2 = (uint32_t *) (work->data + 64);
	unsigned char mid[32], data[12];
	uint32_t diffIdx;
	//uint32_t diffIdx,*diff=(uint32_t*)&job[50]; //difficulty pointer

	job[0] = (job_id << 4) | CMD_WRITE_JOB;
	job[1] = chip_id;
	
	swab256(job + 2, work->midstate3);
	swab256(job + 34, work->midstate2);
	swab256(job + 66, work->midstate1);
	swab256(job + 98, work->midstate);
	p1 = (uint32_t *) &job[130];
	p2 = (uint32_t *) (work->data + 64);
	p1[0] = bswap_32(p2[0]);
	p1[1] = bswap_32(p2[1]);
	p1[2] = bswap_32(p2[2]);

	uint8_t diff[4] = {0x1e, 0x03, 0xff, 0xff};

	if(sdiff>16833)
		memcpy(diff, &(difficult_Tbl[20]), 4);
	else if(sdiff>8191)
		memcpy(diff, &(difficult_Tbl[19]), 4);
	else if(sdiff>4095)
		memcpy(diff, &(difficult_Tbl[18]), 4);
	else if(sdiff>2047)
		memcpy(diff, &(difficult_Tbl[17]), 4);
	else if(sdiff>1023)
		memcpy(diff, &(difficult_Tbl[16]), 4);
	else if(sdiff > 511)
		memcpy(diff, &(difficult_Tbl[15]), 4);
	else if(sdiff > 255)
		memcpy(diff, &(difficult_Tbl[14]), 4);
	else {
		if(opt_diff>=1&&opt_diff<=17)
		{
			diffIdx=opt_diff-1;
			memcpy(diff, &(difficult_Tbl[diffIdx]), 4);
		}
		else
			memcpy(diff, &(difficult_Tbl[13]), 4);
	}
	
	memcpy(job+146, diff, 4);
	
    memset(tmp_buf, 0, sizeof(tmp_buf));
    for(i = 0; i < 79; i++)
    {
        tmp_buf[(2 * i) + 1] = job[(2 * i) + 0];
        tmp_buf[(2 * i) + 0] = job[(2 * i) + 1];
    }
    crc = CRC16_2(tmp_buf, 158);
    job[158] = (uint8_t)((crc >> 8) & 0xff);
    job[159] = (uint8_t)((crc >> 0) & 0xff);

	//printf("[create job] \r\n");
	//hexdump("job:", job, JOB_LENGTH);

	return job;
}




#define COOLDOWN_MS			(30 * 1000)
#define DISABLE_CHIP_FAIL_THRESHOLD	3
#define LEAST_CORE_ONE_CHAIN	603
#define RESET_CHAIN_CNT	2



/********** disable / re-enable related section (temporary for testing) */
int get_current_ms(void)
{
	cgtimer_t ct;
	cgtimer_time(&ct);
	return cgtimer_to_ms(&ct);
}

bool is_chip_disabled(struct A1_chain *a1, uint8_t chip_id)
{
	struct A1_chip *chip = &a1->chips[chip_id - 1];
	return chip->disabled || chip->cooldown_begin != 0;
}

/* check and disable chip, remember time */
void disable_chip(struct A1_chain *a1, uint8_t chip_id)
{
	flush_spi(a1);
	struct A1_chip *chip = &a1->chips[chip_id - 1];
	int cid = a1->chain_id;
	if (is_chip_disabled(a1, chip_id)) {
		applog(LOG_WARNING, "%d: chip %d already disabled",
		       cid, chip_id);
		return;
	}
	applog(LOG_WARNING, "%d: temporary disabling chip %d", cid, chip_id);
	chip->cooldown_begin = get_current_ms();
}

/* check if disabled chips can be re-enabled */
void check_disabled_chips(struct A1_chain *a1, int pllnum)
{
	int i;
	int cid = a1->chain_id;
	uint8_t reg[REG_LENGTH];
	struct spi_ctx *ctx = a1->spi_ctx;

	for (i = 0; i < a1->num_active_chips; i++) 
	{
		int chip_id = i + 1;
		struct A1_chip *chip = &a1->chips[i];
		if (!is_chip_disabled(a1, chip_id))
			continue;
		/* do not re-enable fully disabled chips */
		if (chip->disabled)
			continue;
		if (chip->cooldown_begin + COOLDOWN_MS > get_current_ms())
			continue;

		//if the core in chain least than 630, reinit this chain 
		if(a1->num_cores <= LEAST_CORE_ONE_CHAIN && chip->fail_reset < RESET_CHAIN_CNT)
		{
			chip->fail_reset++;
			asic_gpio_write(ctx->reset, 0);
			usleep(500000);
			asic_gpio_write(ctx->reset, 1);	
		
			a1->num_chips = chain_detect(a1, pllnum);
			
			inno_cmd_bist_fix(a1, ADDR_BROADCAST);
		
			for (i = 0; i < a1->num_active_chips; i++)
			{
				check_chip(a1, i);
			}
		}
		
		if (!inno_cmd_read_reg(a1, chip_id, reg)) 
		{
			chip->fail_count++;
			applog(LOG_WARNING, "%d: chip %d not yet working - %d",
			       cid, chip_id, chip->fail_count);
			if (chip->fail_count > DISABLE_CHIP_FAIL_THRESHOLD) 
			{
				applog(LOG_WARNING, "%d: completely disabling chip %d at %d",
				       cid, chip_id, chip->fail_count);
				chip->disabled = true;
				a1->num_cores -= chip->num_cores;
				continue;
			}
			/* restart cooldown period */
			chip->cooldown_begin = get_current_ms();
			continue;
		}
		applog(LOG_WARNING, "%d: chip %d is working again", cid, chip_id);
		chip->cooldown_begin = 0;
		chip->fail_count = 0;
		chip->fail_reset = 0;
	}
}

//struct timeval tvLast;
//struct timeval tvCurr;
//struct timeval tvDiff;


bool set_work(struct A1_chain *a1, uint8_t chip_id, struct work *work, uint8_t queue_states)
{
	int cid = a1->chain_id;
	struct A1_chip *chip = &a1->chips[chip_id - 1];
	bool retval = false;

	int job_id = chip->last_queued_id + 1;

	//applog(LOG_INFO, "%d: queuing chip %d with job_id %d, state=0x%02x", cid, chip_id, job_id, queue_states);
	if (job_id == (queue_states & 0x0f) || job_id == (queue_states >> 4))
	{
		applog(LOG_WARNING, "%d: job overlap: %d, 0x%02x", cid, job_id, queue_states);
	}

	if (chip->work[chip->last_queued_id] != NULL) 
	{
		work_completed(a1->cgpu, chip->work[chip->last_queued_id]);
		chip->work[chip->last_queued_id] = NULL;	
		retval = true;
	}
	
	uint8_t *jobdata = create_job(chip_id, job_id, work);
	if (!inno_cmd_write_job(a1, chip_id, jobdata)) 
	{
		/* give back work */
		work_completed(a1->cgpu, work);
		applog(LOG_ERR, "%d: failed to set work for chip %d.%d", cid, chip_id, job_id);
		disable_chip(a1, chip_id);
	} 
	else 
	{
		chip->work[chip->last_queued_id] = work;
		chip->last_queued_id++;
		chip->last_queued_id &= 3;
	}
/*
	if(chip_id == 10)
	{
		cgtime(&tvCurr);
		timersub(&tvCurr, &tvLast, &tvDiff);
		printf("CCCCCCCTime.tv_sec:%d,tv_usec:%d.\n", tvDiff.tv_sec,tvDiff.tv_usec);
		cgtime(&tvLast);	
	}
*/	
	return retval;
}

bool get_nonce(struct A1_chain *a1, uint8_t *nonce, uint8_t *chip_id, uint8_t *job_id, uint8_t *micro_job_id)
{
	uint8_t buffer[64];

	memset(buffer, 0, sizeof(buffer));
	if(inno_cmd_read_result(a1, ADDR_BROADCAST, buffer))
	{
		*job_id = buffer[0] >> 4;
		*chip_id = buffer[1];
		*(uint16_t *)micro_job_id = buffer[3];		
		memcpy(nonce, buffer + 4, 4);

		//applog(LOG_INFO, "Got nonce for chip %d / job_id %d / micro_job_id:%d / nonce 0x%08x",
		//	   *chip_id, *job_id, buffer[3], *(uint32_t *)nonce);
		
		return true;
	}
	
	return false;
}

bool abort_work(struct A1_chain *a1)
{

	applog(LOG_INFO,"Start to reset ");

	return true;
}

bool check_chip(struct A1_chain *a1, int i)
{
	uint8_t buffer[64];
	int chip_id = i + 1;
	int cid = a1->chain_id;

	memset(buffer, 0, sizeof(buffer));
	if (!inno_cmd_read_reg(a1, chip_id, buffer)) 
	{
		applog(LOG_WARNING, "%d: Failed to read register for "
			"chip %d -> disabling", cid, chip_id);
		a1->chips[i].num_cores = 0;
		a1->chips[i].disabled = 1;
		return false;;
	}
	else
	{
		hexdump("check chip:", buffer, REG_LENGTH);
	}

	a1->chips[i].num_cores = buffer[11];
	a1->num_cores += a1->chips[i].num_cores;
	applog(LOG_WARNING, "%d: Found chip %d with %d active cores",
	       cid, chip_id, a1->chips[i].num_cores);

	//keep ASIC register value
	memcpy(a1->chips[i].reg, buffer, 12);
	a1->chips[i].temp= (buffer[8]<<8)|buffer[9];

	if (a1->chips[i].num_cores < BROKEN_CHIP_THRESHOLD) 
	{
		applog(LOG_WARNING, "%d: broken chip %d with %d active "
		       "cores (threshold = %d)", cid, chip_id,
		       a1->chips[i].num_cores, BROKEN_CHIP_THRESHOLD);

		//set low pll
		//A1_SetA1PLLClock(a1, BROKEN_CHIP_SYS_CLK, chip_id);
		//cmd_READ_REG(a1, chip_id);
		hexdump_error("new.PLL", a1->spi_rx, 8);
		a1->chips[i].disabled = true;
		a1->num_cores -= a1->chips[i].num_cores;
		
		return false;
	}

	if (a1->chips[i].num_cores < WEAK_CHIP_THRESHOLD) 
	{
		applog(LOG_WARNING, "%d: weak chip %d with %d active "
		       "cores (threshold = %d)", cid,
		       chip_id, a1->chips[i].num_cores, WEAK_CHIP_THRESHOLD);

		//A1_SetA1PLLClock(a1, WEAK_CHIP_SYS_CLK, chip_id);
		//cmd_READ_REG(a1, chip_id);
		hexdump_error("new.PLL", a1->spi_rx, 8);
		
		return false;
	}

	return true;
}

/*
 * BIST_START works only once after HW reset, on subsequent calls it
 * returns 0 as number of chips.
 */
int chain_detect(struct A1_chain *a1, int idxpll)
{
	uint8_t buffer[64];
	int cid = a1->chain_id;
	uint8_t temp_reg[REG_LENGTH];
	int i;

	//add for A6
	asic_spi_init();

	set_spi_speed(1500000);

	inno_cmd_reset(a1, ADDR_BROADCAST);

	usleep(1000);
/*
	memcpy(temp_reg, default_reg[idxpll], REG_LENGTH-2);
	if(!inno_cmd_write_reg(a1, ADDR_BROADCAST, temp_reg))
	{
		applog(LOG_WARNING, "set default PLL fail");
		return -1;
	}
	applog(LOG_WARNING, "set default %d PLL success", i);

	usleep(100000);
*/

	for(i=0; i<idxpll+1; i++)
	{
		memcpy(temp_reg, default_reg[i], REG_LENGTH-2);
		if(!inno_cmd_write_reg(a1, ADDR_BROADCAST, temp_reg))
		{
			applog(LOG_WARNING, "set default PLL fail");
			return -1;
		}
		applog(LOG_WARNING, "set default %d PLL success", i);

		usleep(100000);
	}
	
	set_spi_speed(6500000);
	usleep(1000);

	memset(buffer, 0, sizeof(buffer));
	if(!inno_cmd_bist_start(a1, 0, buffer))
	{
		applog(LOG_WARNING, "bist start fail");
		return -1;
	}
	a1->num_chips = buffer[3]; 
	applog(LOG_WARNING, "%d: detected %d chips", cid, a1->num_chips);

	usleep(10000);
/*	
	if(!inno_cmd_bist_collect(a1, ADDR_BROADCAST))
	{
		applog(LOG_WARNING, "bist collect fail");
		return -1;
	}
*/

	applog(LOG_WARNING, "collect core success");
	applog(LOG_WARNING, "%d: no A1 chip-chain detected", cid);
	return a1->num_chips;

}



