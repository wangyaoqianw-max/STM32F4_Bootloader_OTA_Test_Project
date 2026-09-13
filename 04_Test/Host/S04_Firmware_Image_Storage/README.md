# S04 Firmware Image Storage Host Tests

本目录保存 S04 可在 Host 上独立验证的数据格式、CRC 与存储编排测试。

`s04_crc_host_test.c` 覆盖 CRC-8/SMBUS、CRC-16/XMODEM、CRC-32/ISO-HDLC 的标准向量，以及一次性和分块计算的一致性。
