// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/mod_devicetable.h>

struct mysht30_data {
	struct i2c_client *client;
	struct mutex lock;
};

static u8 mysht30_crc8(const u8 *buf, size_t len)
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

static int mysht30_temp_mc(u16 raw)
{
	return -45000 + ((s64)175000 * raw) / 65535;
}

static int mysht30_humidity_pcm(u16 raw)
{
	return (s64)100000 * raw / 65535;
}

static int mysht30_measure(struct mysht30_data *data, u16 *raw_t, u16 *raw_rh)
{
		const u8 cmd[2] = { 0x24, 0x00 }; // sht30 規定的量測指令 (高精度: 0x2400 -> 0x24 0x00)
		u8 buf[6];
		int ret;

		mutex_lock(&data->lock); // sensor 上鎖

		ret = i2c_master_send(data->client, cmd, sizeof(cmd)); // 傳送量測指令給 sensor
		if (ret != sizeof(cmd)) {
			ret = ret < 0 ? ret : -EIO; // ret < 0 代表是錯誤碼
			goto unlock;
		}

		usleep_range(15000, 20000);

		ret = i2c_master_recv(data->client, buf, sizeof(buf)); // 讀回數據放在 buf -> buf[溫, 度, crc, 濕, 度, crc]
		if (ret != sizeof(buf)){
			ret = ret < 0 ? ret : -EIO;
			goto unlock;
		}

		// 檢查溫度 crc
		if (mysht30_crc8(buf, 2) != buf[2]) {
			ret = -EIO;
			goto unlock;
		}

		// 檢查濕度 crc
		if (mysht30_crc8(buf + 3, 2) != buf[5]) {
			ret = -EIO;
			goto unlock;
		}

		*raw_t = (buf[0] << 8) | buf[1]; // 溫度
		*raw_rh = (buf[3] << 8) | buf[4]; // 溼度

		ret = 0;

unlock:
		mutex_unlock(&data->lock);
		return ret;
}

static ssize_t temperature_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mysht30_data *data = dev_get_drvdata(dev); // 取回 probe 的資料
	u16 raw_t, raw_rh;
	int ret;

	ret = mysht30_measure(data, &raw_t, &raw_rh); // 取回溫度、溼度數據
	if (ret) return ret; // 失敗回傳錯誤碼

	return sysfs_emit(buf, "%d\n", mysht30_temp_mc(raw_t));
}
static DEVICE_ATTR_RO(temperature);

static ssize_t humidity_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mysht30_data *data = dev_get_drvdata(dev); // 取回 probe 的資料
	u16 raw_t, raw_rh;
	int ret;

	ret = mysht30_measure(data, &raw_t, &raw_rh); // 取回溫度、溼度數據
	if (ret) 
		return ret; // 失敗回傳錯誤碼

	return sysfs_emit(buf, "%d\n", mysht30_humidity_pcm(raw_rh));
}
static DEVICE_ATTR_RO(humidity);

static struct attribute *mysht30_attrs[] = {
	&dev_attr_temperature.attr,
	&dev_attr_humidity.attr,
	NULL,
};
ATTRIBUTE_GROUPS(mysht30);

static int mysht30_probe(struct i2c_client *client)
{
	struct mysht30_data *data; // 通常都要在第一個 if 之前就要宣告好

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -EOPNOTSUPP; // 檢查 i2c controller 是否支援這個 target 的溝通

	data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL); // 跟 kernel 要一個給 data 的記憶體; z 代表會清 0; devm 代表不用自己回收
	if (!data) 
		return -ENOMEM; // 檢查是不是有要到這個記憶體
	data->client = client; // 把 probe 記錄到的參數登記到 data 裡面

	mutex_init(&data->lock); // 初始化鎖

	i2c_set_clientdata(client, data); // 把 data 掛回 client (data 在 probe 結束之後就會被 devm 清掉)

	dev_info(&client->dev, "probed at 0x%02x\n", client->addr);
	return 0;
}

static const struct of_device_id mysht30_of_match[] = {
	{ .compatible = "mylab,sht30" },
	{ }
};
MODULE_DEVICE_TABLE(of, mysht30_of_match);

static const struct i2c_device_id mysht30_id[] = {
	{ "mysht30", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mysht30_id);

static struct i2c_driver mysht30_driver = {
	.driver = {
		.name		= "mysht30",
		.of_match_table	= mysht30_of_match,
		.dev_groups     = mysht30_groups,
	},
	.probe		= mysht30_probe,
	.id_table	= mysht30_id,
};
module_i2c_driver(mysht30_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SHT30 practice driver");
