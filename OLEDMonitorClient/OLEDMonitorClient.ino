#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <U8g2lib.h>
#include <sstream>
#include <string.h>
#include <math.h> /* round, floor, ceil, trunc */
#include <stdio.h>
#include "loadingGif.h"
#include "iconImg.h"
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);
// 图标数组
const unsigned char *const iconList[] U8X8_PROGMEM = {active, cpu, box, database, drive, global, nodejs, server, terminal, thermomete, wifi, link, wifi2};
// 加载动画数组 loading array
const unsigned char *const gifList[] U8X8_PROGMEM = {load1, load2, load3, load4, load5, load6, load7, load8, load9, load10, load11, load12, load13, load14, load15, load16, load17, load18, load19, load20, load21, load22, load23, load24, load25, load26, load27, load28};
Ticker isCON;                            // 检测连接定时器
Ticker gifTicker;                        // 更新loading gif 数组索引定时器
Ticker getServerInfoTicker;              // 连接成功后定时向服务器获取请求
Ticker swInfoTimer;                      // 切换信息显示帧
WiFiClient client;                       // Tcp客户端
String infoArr[3][6];                    // 存放从服务器获取到的数据
const char *SSID = "OpenWrt";            // WiFi名 SSID
const char *SSIDPW = "";                 // WiFi密码,没有留空 SSISPASSWORD
const String ServerIP = "192.168.10.90"; // 服务器地址
const int ServerPort = 553;              // 服务器端口
byte CONStatusCode = 0;
byte CONTimer = 24; // *连接计时器
byte gifIndex = 0;  // *loading gif帧数组索引
bool loadGIndxFstTimer = 1;
byte ServerLoading = 1;           // *服务器是否加载中flag
byte infoIndex = 0;               // *信息显示帧 (轮播)
const long infoSwitchTime = 4000; // 信息轮播时间间隔3秒
void setup()
{
    Serial.begin(9600);
    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, SSIDPW);
    u8g2.begin();
    isCON.once_ms(200, isCONFun);                     // 定时检测连接状态
    getServerInfoTicker.attach(2, getServaerInfo);    // 定时获取服务器信息
    swInfoTimer.once_ms(infoSwitchTime, infoIndexSw); // 开启切换信息显示帧
}
void loop()
{
    if (CONStatusCode < 2) // 未连接服务器,根据连接检测更新的状态码来重连各级连接
    {
        switch (CONStatusCode)
        {
        case 0:
            if (CONTimer >= 50) // 10 秒超时重连
            {
                WiFi.disconnect();
                WiFi.begin(SSID, SSIDPW); // 尝试重新连接WiFi
                CONTimer = 0;
            };
            break;
        case 1:
            if (CONTimer >= 50) // 5 秒超时重连服务器
            {
                client.stop();
                client.connect(ServerIP, ServerPort);
                CONTimer = 0;
            };
        }
    }
    OLEDDraw();
}
// 定时检测客户端和服务器的连接状态并更新连接状态码,以及重连后的重置重连中的操作
void isCONFun()
{
    isCON.once_ms(200, isCONFun);      // 保持下次定时器
    if (WiFi.status() != WL_CONNECTED) // WiFi未连接
    {
        CONStatusCode = 0; // 降级连接状态检测码
        CONTimer++;        // 开启重连WiFi定时器
        return;
    }
    else
    {
        if (CONStatusCode < 1) // WiFi已连接,但状态码为未连接WiFi值修复状态码(也可看做重连成功部分)
        {
            CONStatusCode = 1;     // 更新状态码
            CONTimer = 49;         // 重置loop中的重连超时计数器
            gifTicker.detach();    // 关闭索引定时器
            loadGIndxFstTimer = 1; // 重置索引定时器开启开关为开,以便下次连接加载动画使用
        }
    }
    // 连接服务器部分
    if (!client.connected()) // 未连接到服务器
    {
        CONStatusCode = 1; // 降级连接状态检测码为连接WiFi未连接服务器1
        CONTimer++;        // 开启重连WiFi定时器
        return;            // 不在执行下面的服务器连接检测,无意义
    }
    else
    {
        if (CONStatusCode < 2) // 服务器已连接,但状态码为未连接WiFi值修复状态码
        {
            CONStatusCode = 2;     // 更新状态码
            CONTimer = 24;         // 重置loop中的重连超时计数器
            gifTicker.detach();    // 关闭索引定时器
            loadGIndxFstTimer = 1; // 重置索引定时器开启开关为开,以便下次连接加载动画使用}
        }
    }
}

// 根据状态码来更绘制屏幕显示内容
void OLEDDraw()
{
    switch (CONStatusCode)
    {
    case 0: // WiFi未连接显式内容
        //开启一个定时器更新索引,没有更新状态码只能执行一次"开启帧定时器",除非状态码更新,但状态码更新要确保上次状态码的帧处理定时器关闭
        OLEDLoadDraw(gifList, 66 + 12, 0, wifi2, 0, 0, "WiFi", 0, 64 - 16, 0, 64);
        break;
    case 1:
        OLEDLoadDraw(gifList, -15, 0, server, 128 - 32, 0, "Server", 128 - getFontWidth(u8g2_font_helvB12_te, "Server"), 64 - 16, 128 - getFontWidth(u8g2_font_t0_16b_tf, "Server") - 32, 64);
        break;
    case 2:
        if (ServerLoading) // 服务端获取系统数据没准备好
        {
            OLEDLoadDraw(gifList, -15, 0, link, 128 - 32, 0, "link", 128 - getFontWidth(u8g2_font_helvB12_te, "Link"), 64 - 16, 128 - getFontWidth(u8g2_font_t0_16b_tf, "Server") - 32, 64);
        }
        else
        {
            gifTicker.detach();    // 关闭索引定时器
            loadGIndxFstTimer = 1; // 重置索引定时器开启开关为开,以便下次连接加载动画使用
            OLEDInfoDraw();
        }
        break;
    }
}

// Loading动画帧切换
void gifIndexSw()
{
    if (++gifIndex >= 28)
        gifIndex = 0;
    gifTicker.once_ms(30, gifIndexSw);
}

// 未连接服务的加载界面
void OLEDLoadDraw(const unsigned char *const *gifList, unsigned int gifX, int gifY, const unsigned char *icon, int iconX, int iconY, const char *Heading, int HeadingX, int HeadingY, int tipsX, int tipsY)
{
    u8g2.clearBuffer();
    if (loadGIndxFstTimer) // 开启gif索引定时器，防止在loop中多次开启
    {
        gifTicker.once_ms(15, gifIndexSw);
        loadGIndxFstTimer = 0; // 关闭下次不再重复开启gifTicker定时器
    }
    u8g2.drawXBMP(gifX, gifY, 64, 64, gifList[gifIndex]);
    u8g2.drawXBMP(iconX, iconY, 32, 32, icon);
    u8g2.setFont(u8g2_font_helvB12_te);
    u8g2.drawStr(HeadingX, HeadingY, Heading);
    u8g2.setFont(u8g2_font_t0_16b_tf);
    u8g2.drawStr(tipsX, tipsY, "Connection");
    u8g2.sendBuffer();
}

// 发送获取服务端系统信息请求
void getServaerInfo()
{
    if (CONStatusCode < 2) // 没连接到服务端作甚?
        return;
    client.write("get");    // 请求数据
    Tcp_Handler(readTcp()); // 数据处理
}

// 读取服务端发送的数据
String readTcp()
{
    String data = "";
    while (client.available() > 0)
        data += char(client.read()); // 读取数据
    return data;
}

// 处理服务端发送的数据
void Tcp_Handler(String data)
{
    String tempStr = "";
    if (!(data.charAt(0) == '?' && data.charAt(data.length() - 1) == '!')) // 字符不是?-!形式
        ServerLoading = 1;                                                 // 服务器初始化中,切换为服务器加载界面
    else
    {
        ServerLoading = 0;                                                             // 关闭加载动画
        data = (data.substring(1, data.length() - 1)).substring(0, data.length() - 2); // 去掉首尾字符
        // 处理获取到的字符到信息数组中infoArr
        for (int i = 0; i < 3; i++)
        {
            tempStr = (getValue(data, '\\', i));
            for (int j = 0; j < 6; j++)
            {
                infoArr[i][j] = getValue(tempStr, '@', j);
            }
        }
    }
}

// 字符串截取
String getValue(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = {0, -1};
    int maxIndex = data.length() - 1;

    for (int i = 0; i <= maxIndex && found <= index; i++)
    {
        if (data.charAt(i) == separator || i == maxIndex)
        {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i + 1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

// 系统信息绘制
void OLEDInfoDraw()
{
    // value all
    u8g2.clearBuffer();
    const String view = infoArr[infoIndex][0],
                 option = infoArr[infoIndex][1],
                 key = infoArr[infoIndex][2],
                 value = infoArr[infoIndex][3],
                 unit = infoArr[infoIndex][4],
                 all = infoArr[infoIndex][5];

    float pl = value.toFloat() / all.toFloat(); // 已用占比
    String ListInfoArr[4];
    char pc[3];
    itoa(int(pl * 100), pc, 10);
    String pcstr(pc);
    pcstr.concat("%");
    // 视图模式
    switch (view.toInt())
    {
    case 1:                  // 大字样式
        u8g2.setFontMode(1); // 文字透明
        u8g2.setFont(u8g2_font_lastapprenticebold_tr);
        u8g2.drawStr((128 - u8g2.getUTF8Width(key.c_str())) / 2, 17, key.c_str()); // key
        u8g2.setFont(u8g2_font_freedoomr25_tn);
        u8g2.drawStr(128 - u8g2.getUTF8Width(value.c_str()) - 34, 50, value.c_str()); // value
        if (option.toInt())
        {
            // 开启百分比框
            u8g2.drawRBox(96, 23, 32, 12, 3); // 百分比框
            u8g2.setDrawColor(0);             // 文字反色
            u8g2.setFont(u8g2_font_t0_14b_mr);
            u8g2.drawStr(96 + (32 - u8g2.getUTF8Width(pcStr(value, all).c_str())) / 2, 50 - 16, pcstr.c_str()); // 百分比文字
            u8g2.setDrawColor(1);
        }
        u8g2.drawRBox(96, 36, 32, 12, 3); // 单位框
        u8g2.setDrawColor(0);             // 文字反色
        u8g2.setFont(u8g2_font_t0_14b_mr);
        u8g2.drawStr(96 + (32 - u8g2.getUTF8Width(unit.c_str())) / 2, 50 - 3, unit.c_str()); // 单位文字
        u8g2.setDrawColor(1);
        // 进度条
        u8g2.setDrawColor(1);
        u8g2.drawFrame(0, 52, 128, 12); //
        u8g2.drawBox(0 + 2, 52 + 2, (int(floor(value.toFloat() / all.toFloat() * 124))), 8);
        u8g2.setFontMode(0);
        break;
    case 2: // 视图2样式 进度条样式
        u8g2.setFontMode(1);
        // 进度条1
        // 提取服务端列表模式下的第一组数据
        for (int i = 0; i < 4; i++)
        {
            ListInfoArr[i] = getValue(infoArr[infoIndex][1], '#', i);
        }

        u8g2.setFont(u8g2_font_mercutio_basic_nbp_tf);
        u8g2.drawStr(0, 14, ListInfoArr[0].c_str()); //key
        u8g2.setFont(u8g2_font_t0_14b_mr);
        u8g2.drawStr(128 - u8g2.getUTF8Width((ListInfoArr[1] + "/" + ListInfoArr[3] + ListInfoArr[2]).c_str()), 14, (ListInfoArr[1] + "/" + ListInfoArr[3] + ListInfoArr[2]).c_str()); //value/all unit
        u8g2.drawRFrame(0, 16, 128, 16, 0);                                                                                                                                            // 百分比进度条外框
        u8g2.drawRBox(2, 18, int(floor(ListInfoArr[1].toFloat() / ListInfoArr[3].toFloat() * 124)), 12, 0);                                                                            //百分比进度条 两侧各空白2像素
        u8g2.setDrawColor(2);                                                                                                                                                          // 文字反色
        u8g2.drawStr(2 + (124 - u8g2.getUTF8Width(pcStr(ListInfoArr[1], ListInfoArr[3]).c_str())) / 2, 28, pcStr(ListInfoArr[1], ListInfoArr[3]).c_str());                             // 百分比文字
        u8g2.setDrawColor(1);
        // 进度条2
        u8g2.setFont(u8g2_font_mercutio_basic_nbp_tf);
        u8g2.drawStr(0, 46, key.c_str()); // key
        u8g2.setFont(u8g2_font_t0_14b_mr);
        u8g2.drawStr(128 - u8g2.getUTF8Width((value + "/" + all + unit).c_str()), 46, (value + "/" + all + unit).c_str()); //value/all unit
        u8g2.drawRFrame(0, 48, 128, 16, 0);                                                                                // 百分比框
        u8g2.drawRBox(2, 50, int(floor(value.toFloat() / all.toFloat() * 124)), 12, 0);                                    // 百分比
        u8g2.drawRBox(2, 18, int(floor(ListInfoArr[1].toFloat() / ListInfoArr[3].toFloat() * 124)), 12, 0);                //百分比进度条 两侧各空白2像素
        u8g2.setDrawColor(2);                                                                                              // 文字反色
        u8g2.drawStr(2 + (124 - u8g2.getUTF8Width(pcStr(value, all).c_str())) / 2, 60, pcStr(value, all).c_str());         // 百分比文字
        u8g2.setDrawColor(1);
        u8g2.setFontMode(0);
        break;
    }
    u8g2.sendBuffer();
}

// 切换信息显示帧
void infoIndexSw()
{
    if (++infoIndex >= 3)
        infoIndex = 0;
    swInfoTimer.once_ms(infoSwitchTime, infoIndexSw);
}

// 获取字符宽度
int getFontWidth(const uint8_t *font, const char *str)
{
    u8g2.setFont(font);
    return u8g2.getUTF8Width(str);
}
String pcStr(String fst, String sec)
{
    String tempStr(int(floor(fst.toFloat() / sec.toFloat() * 100))); // 计算百分比
    tempStr += "%";                                                  // 添加百分号
    return tempStr;
}