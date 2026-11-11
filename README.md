# mysht30

Sensirion SHT30 溫濕度感測器的 Linux I2C driver（練習用），在 STM32MP215F-DK、Linux 6.6 上驗證過。

讀取時透過 sysfs，每次 `cat` 都會重新量測一次：

```sh
cat /sys/bus/i2c/devices/0-0044/temperature   # 27782 → 27.782 °C
cat /sys/bus/i2c/devices/0-0044/humidity      # 72335 → 72.335 %RH
```

單位是千分之一（m°C、m%RH），除以 1000 就是 °C 和 %RH。

## 檔案

| 檔案 | 內容 |
|---|---|
| `mysht30.c` | driver 本體：probe、量測流程、sysfs |
| `mysht30_calc.h` | 純計算：CRC-8、6 bytes 解析、溫濕度換算（driver 和測試共用） |
| `test/test_calc.c` | 在主機上測 `mysht30_calc.h` |
| `test/run.sh` | 編譯並執行測試 |
| `Makefile` | out-of-tree module，也給 Yocto `module.bbclass` 用 |

## Device tree

裝置要寫在 dts 裡，`compatible` 必須是 `mylab,sht30`：

```dts
&i2c1 {
	pinctrl-names = "default";
	pinctrl-0 = <&i2c1_pins_mylab>;
	status = "okay";

	sht30@44 {
		compatible = "mylab,sht30";
		reg = <0x44>;
	};
};
```

## 編譯

用 `KERNEL_SRC` 指向已編好的 kernel build 目錄，並指定交叉編譯器：

```sh
make KERNEL_SRC=<kernel build dir> ARCH=arm64 CROSS_COMPILE=<toolchain prefix>
```

產生 `mysht30.ko`。上板後用 `insmod mysht30.ko` 載入，`dmesg` 會看到 `probed at 0x44`。

## 測試

不需要板子，在主機上執行：

```sh
test/run.sh
```

測試項目：datasheet 的 CRC 測試向量 `0xBEEF → 0x92`、實際讀到的封包解析、CRC 錯誤時回傳 `-EIO`、溫濕度換算的邊界值。

## 量測設定

- 指令 `0x2400`：高重複性、不使用 clock stretching
- 送出指令後等待 15–20 ms 再讀 6 bytes
- 兩組 CRC 任一不符時回傳 `-EIO`，不輸出數值

## Yocto

meta-wayne 的 `recipes-kernel/mysht30/mysht30_1.0.bb` 從這個 repo 的 `main` 編譯成 `kernel-module-mysht30`，開機時依 device tree 的 `compatible` 自動載入。
