# 过时的文档,待更新
# 需要配合服务端使用https://github.com/sakura-he/monitor.git (nodejs编写)



# 如何使用

## 材料 

| ESP8266/nodeMCU d1-mini(作者用的)/常见的ESP8266开发板(淘宝十几块钱包邮) | 1个  |
| :----------------------------------------------------------- | ---- |
| OLED12864显示屏7pin SPI引脚/OLED12864显示屏4pin IIC引脚(作者用的) | 1个  |
| 导线,杜邦线                                                  | 若干 |
| MicroUSB2.0数据线                                            | 1根  |
|                                                              |      |



## 接线

###OLED12864显示屏7pin SPI引脚

| ESP8266 | ->   | OLED |
| :-----: | ---- | ---- |
|   3V    |      | VCC  |
|    G    |      | GND  |
|   D7    |      | D1   |
|   D5    |      | D0   |
| D2orD8  |      | CS   |
|   D1    |      | DC   |
|   RST   |      | RES  |
|         |      |      |

### OLED12864显示屏4pin 2IC引脚

| ESP8266   | ->   | OLED |
| --------- | ---- | ---- |
| 3.3V      |      | VCC  |
| G (GND)   |      | GND  |
| D1(GPIO5) |      | SCL  |
| D2(GPIO4) |      | SDA  |
|           |      |      |

## 编译软件ArduinoIDE准备

###安装Arduino IDE

https://www.arduino.cc/en/Main/Software

### 配置Arduino ESP8266开发环境

1. 打开Arduino IDE->菜单项文件->首选项，然后会看到附加开发版管理器网址，填入http://arduino.esp8266.com/stable/package_esp8266com_index.json，重启IDE；

    ![image](https://user-gold-cdn.xitu.io/2019/7/5/16bc05635876b018?imageView2/0/w/1280/h/960/format/webp/ignore-error/1)

2. 重启IDE之后->菜单项工具->开发板->点击开发板管理器->滚动找到ESP8266平台；![image](https://user-gold-cdn.xitu.io/2019/7/5/16bc0563a061af8a?imageView2/0/w/1280/h/960/format/webp/ignore-error/1)

3. 从下拉选项中选择你想下载的版本，点击安装，需要等待一段时间安装完毕。![image](https://user-gold-cdn.xitu.io/2019/7/5/16bc0563d681a2ad?imageView2/0/w/1280/h/960/format/webp/ignore-error/1)

4. 在ArduinoIDE的工具选项卡找到开发板列表,选择合适你的开发板,作者用的淘宝十元包邮的nodeMCU d1-mini,所以选的是WeMos-R1![image-20200614104552825](C:%5CUsers%5Csakura%5CAppData%5CRoaming%5CTypora%5Ctypora-user-images%5Cimage-20200614104552825.png)

### 按注释修改相应代码

- 从github上克隆客户端(monitorClint)代码到本地,打开文件夹,找到MonitorClint.ino文件,用ArduinoIDE打开

- 修改代码

    ```
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);//默认4引脚 2IC
    //U8G2_SSD1306_128X64_NONAME_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 4, /* dc=*/ 5, /* reset=*/ 3); //使用7个引脚SPI屏幕的取消注释这行并注释掉上一行
    const char *SSID = "OpenWrt";    // WiFi名 SSID
    const char *SSIDPW = "";    // WiFi密码,没有留空 SSISPASSWORD
    const String ServerIP = "192.168.10.59";    // 服务器地址
    const int ServerPort = 553;    // 服务器端口
    const long infoSwitchTime = 4000; // 信息轮播时间间隔4秒
    ```

### 烧录到ESP8266

- 给ESP8266开发板插入USB数据线并连接到电脑USB接口上
- 选择ESP8266开发板的端口(作者这里是COM3,你的我不知道)![image-20200614105732800](https://github.com/sakura-he/OLEDMonitorClient/blob/master/img/image-20200614105732800.png)
- 点击编译按钮![image-20200614105553105](https://github.com/sakura-he/OLEDMonitorClient/blob/master/img/image-20200614105553105.png)

### 开启服务端

- 服务端github地址https://github.com/sakura-he/monitor.git
# 由于作者是第一次使用C++编写代码,代码中可能有大量的冗余代码和BUG,欢迎指出和修改

