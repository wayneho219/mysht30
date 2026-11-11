// SPDX-License-Identifier: GPL-2.0
/*
 * 在 DGX（user space）上測 mysht30_calc.h 的純計算函式，不需要板子。
 * 執行：test/run.sh
 */
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* kernel 的型別名稱，在 user space 自己補上 */
typedef uint8_t u8;
typedef uint16_t u16;
typedef int64_t s64;

#include "../mysht30_calc.h"

static int failures;

#define CHECK_EQ(name, got, want)                                              \
	do {                                                                   \
		long long g_ = (got), w_ = (want);                             \
		if (g_ != w_) {                                                \
			printf("FAIL %-32s got %lld, want %lld\n", name, g_, w_); \
			failures++;                                            \
		} else {                                                       \
			printf("ok   %s\n", name);                             \
		}                                                              \
	} while (0)

/* 板子上用 i2ctransfer 實際讀到的封包 */
static const u8 real_pkt[6] = { 0x6a, 0x42, 0x65, 0xba, 0xc7, 0x2e };

static void test_crc(void)
{
	const u8 vec[2] = { 0xBE, 0xEF };

	CHECK_EQ("crc: datasheet 0xBEEF", mysht30_crc8(vec, 2), 0x92);
	CHECK_EQ("crc: real temp 0x6a42", mysht30_crc8(real_pkt, 2), 0x65);
	CHECK_EQ("crc: real humid 0xbac7", mysht30_crc8(real_pkt + 3, 2), 0x2e);
}

static void test_parse(void)
{
	u8 pkt[6];
	u16 t = 0, rh = 0;
	size_t i;

	CHECK_EQ("parse: real packet ret", mysht30_parse(real_pkt, &t, &rh), 0);
	CHECK_EQ("parse: real packet raw_t", t, 0x6a42);
	CHECK_EQ("parse: real packet raw_rh", rh, 0xbac7);

	/* 溫度 CRC 錯一個 bit：要回 -EIO，而且不能改到輸出 */
	for (i = 0; i < 6; i++)
		pkt[i] = real_pkt[i];
	pkt[2] ^= 0x01;
	t = rh = 0x1234;
	CHECK_EQ("parse: bad temp crc ret", mysht30_parse(pkt, &t, &rh), -EIO);
	CHECK_EQ("parse: bad temp crc keeps raw_t", t, 0x1234);

	/* 濕度 CRC 錯 */
	for (i = 0; i < 6; i++)
		pkt[i] = real_pkt[i];
	pkt[5] ^= 0x01;
	CHECK_EQ("parse: bad humid crc ret", mysht30_parse(pkt, &t, &rh), -EIO);

	/* 資料錯、CRC 沒變：也要被抓到 */
	for (i = 0; i < 6; i++)
		pkt[i] = real_pkt[i];
	pkt[1] ^= 0x01;
	CHECK_EQ("parse: bad temp data ret", mysht30_parse(pkt, &t, &rh), -EIO);
}

static void test_temp(void)
{
	CHECK_EQ("temp: raw 0 -> -45000", mysht30_temp_mc(0), -45000);
	CHECK_EQ("temp: raw 32768 -> 42501", mysht30_temp_mc(32768), 42501);
	CHECK_EQ("temp: raw 65535 -> 130000", mysht30_temp_mc(65535), 130000);
	CHECK_EQ("temp: raw 27202 -> 27638", mysht30_temp_mc(27202), 27638);
}

static void test_humidity(void)
{
	CHECK_EQ("humid: raw 0 -> 0", mysht30_humidity_pcm(0), 0);
	CHECK_EQ("humid: raw 32768 -> 50000", mysht30_humidity_pcm(32768), 50000);
	CHECK_EQ("humid: raw 65535 -> 100000", mysht30_humidity_pcm(65535), 100000);
	CHECK_EQ("humid: raw 47815 -> 72961", mysht30_humidity_pcm(47815), 72961);
}

int main(void)
{
	test_crc();
	test_parse();
	test_temp();
	test_humidity();

	if (failures) {
		printf("\n%d test(s) FAILED\n", failures);
		return 1;
	}
	printf("\nall tests passed\n");
	return 0;
}
