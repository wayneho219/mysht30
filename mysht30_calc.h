/* SPDX-License-Identifier: GPL-2.0 */
/*
 * SHT30 的純計算函式：不碰 I2C、不碰鎖。
 * driver 和 test/test_calc.c 共用這一份，所以測試測到的就是 driver 實際用的程式。
 * kernel 編譯時型別來自 linux/types.h；user space 的測試要在 include 前自己 typedef u8、u16、s64。
 */
#ifndef MYSHT30_CALC_H
#define MYSHT30_CALC_H

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/errno.h>
#endif

static inline u8 mysht30_crc8(const u8 *buf, size_t len)
{
	u8 crc = 0xFF;
	for (size_t i = 0; i < len; i++) {
		crc ^= buf[i];
		for (size_t j = 0; j < 8; j++){
			if ((crc & 0x80) == 0x80) {
				crc <<= 1;
				crc ^= 0x31;
			} else {
				crc <<= 1;
			}
		}
	}
	return crc;
}

static inline int mysht30_temp_mc(u16 raw)
{
	return -45000 + ((s64)175000 * raw) / 65535;
}

static inline int mysht30_humidity_pcm(u16 raw)
{
	return (s64)100000 * raw / 65535;
}

/*
 * 解析讀回的 6 bytes：[T_MSB T_LSB T_CRC RH_MSB RH_LSB RH_CRC]
 * 兩組 CRC 都對才寫入 raw_t、raw_rh 並回傳 0；任一組錯回傳 -EIO，不寫入。
 */
static inline int mysht30_parse(const u8 *buf, u16 *raw_t, u16 *raw_rh)
{
	if (mysht30_crc8(buf, 2) != buf[2]) // 檢查溫度 crc
		return -EIO;
	if (mysht30_crc8(buf + 3, 2) != buf[5]) // 檢查濕度 crc
		return -EIO;

	*raw_t = (buf[0] << 8) | buf[1]; // 溫度
	*raw_rh = (buf[3] << 8) | buf[4]; // 溼度
	return 0;
}

#endif
