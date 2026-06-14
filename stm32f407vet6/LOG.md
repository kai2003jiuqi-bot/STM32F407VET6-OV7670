# 需求
每采集OV7670一行，就显示在LCD上

重新写ov7670.c.h，参考这种风格："D:\tkx20\Project\Embedded\MCU\_example\stm32f407vet6\QR Code Recognization\BSP\OV7670.c"
要求：
仅设置为QQVAG模式，不提供其他模式接口


# 屏幕不显示任何颜色
1、检查引脚连接 ❌
2、问AI：LCD设置为了外部中断 ❌
3、换代码版本 ✔️

# 上电后%70概率显示屏白屏
初始化的时候CS没有置高电平，导致LCD被选中，可能初始化的时候SPI发了数据干扰了之后的通信

# 显示一直为竖条
OV7670采集频率太高了。

# QQVGA无法使用
要开启VGA模式，然后才能使用QQVGA