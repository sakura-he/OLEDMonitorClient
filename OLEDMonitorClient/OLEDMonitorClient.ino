#include <math.h>
#include <Ticker.h>
#include <EEPROM.h>
#include <U8g2lib.h>
#include <ESP8266WiFi.h>
#include "LogansGreatButton.h" // 请使用https://github.com/sakura-he/LogansGreatButton
#include <ESP8266WebServer.h>
#include <ESP8266_Seniverse.h> // 太极创客的心知天气第三方API库 https://github.com/taichi-maker/ESP8266-Seniverse 由衷感谢太极创客的ESP8266教程
#include "iconImg.h"
#include "loadingGif.h"
#include "weatherIcon.h"
#include <WEMOS_SHT3X.h>
#define SCL 14
#define SDA 2
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R2, /* reset=*/U8X8_PIN_NONE, SCL, SDA);
// 图标数组
const unsigned char *const iconList[] U8X8_PROGMEM = {active, cpu, box, database, drive, global, nodejs, server, terminal, thermomete, wifi, link, wifi2};
// 加载动画数组 loading array
const unsigned char *const gifList[] U8X8_PROGMEM = {load1, load2, load3, load4, load5, load6, load7, load8, load9, load10, load11, load12, load13, load14, load15, load16, load17, load18, load19, load20, load21, load22, load23, load24, load25, load26, load27, load28};
// 天气图标数组
const unsigned char *const weatherIconList[42] U8X8_PROGMEM = {sunny00, sunny01, sunny02, sunny03, cloudy04, partlyCloudy05, partlyCloudy06, mostlyCloudy07, mostlyCloudy08, overcast09, shower10, thundershower11, thundershowerWithHail12, lightRain13, moderateRain14, heavyRain15, storm16, heavyStorm17, severeStorm18, iceRain19, sleet20, snowFlurry21, lightSnow22, moderateSnow23, heavySnow24, snowstorm25, dust26, sand27, duststorm28, sandstorm29, foggy30, haze31, windy32, blustery33, hurricane34, tropicalStorm35, tornado36, clod37, hot38, light39, night40};
Ticker checkWiFiConnectedTicker;   // 检测连接定时器
Ticker checkServerConnectedTicker; // 检测与检测服务器连接定时器
Ticker gifTicker;                  // 更新loading gif 数组索引定时器
Ticker getServerInfoTicker;        // 连接成功后定时向服务器获取请求
Ticker getWeatherInfoTicker;       // 获取服务器信息定时器
Ticker swInfoTimer;                // 切换服务器监控信息显示帧
WiFiClient client;                 // 连接监控服务器Tcp客户端
SHT3X sht30(0x44);
ESP8266WebServer webserver(80);
WeatherNow weatherNow; // 建立WeatherNow对象用于获取心知天气信息
Forecast forecast;
String infoArr[50][6];            // 存放从服务器获取到的数据 !will be BUG!
byte infoItemsNum;                // 服务获取的项目数量
byte WiFiReconnectTimeout = 69;   // WiFi重连定时器
byte serverReconnectTimeout = 29; // 与检测服务器重连定时器
byte gifIndex = 0;                // *loading gif帧数组索引
byte weatherInfoIndex;            // *天气信息显示帧 (控制在oled轮播第天气信息项目)
byte serverInfoIndex = 0;         // *服务器信息显示帧 (控制在oled轮播第几个服务器监控信息项目)
byte mainIndex = 1;               // 显示面板项目索引
byte monitorItem = 1;
byte settingItem = 1;            // 当前设置项目值(在设置界面显示第几个项目值)
const byte PROGMEM btn1Pin = 10; // 按钮引脚
//const byte PROGMEM btn2Pin = ;   // 按钮引脚
// const byte PROGMEM ledPin = 2;   // led指示灯引脚
LogansGreatButton btn1(btn1Pin);
//LogansGreatButton btn2(btn2Pin);
bool isWiFiConnected = 0; // WiFi状态码
bool isServerConnected = 0;
bool loadGIndxFstTimer = 1;
bool getWeatherInfoFlag = 0; // 请求天气flag
bool getServerInfoFlag = 0;  // 请求检测服务器信息flag
bool ServerLoading = 1;      // *服务器是否加载中flag
//该结构体通过savaConfig和loadConfig函数读取或保存内容到EEPROM
struct config_type
{
    char SSID[32];           // station mode WiFi SSID
    char SSIDPW[64];         // WiFi password
    char serverIP[16];       // 被监控服务端端 ip地址
    unsigned int serverPort; // 被监控服务端端 端口
    char xinzhiKey[24];      // 心知天气秘钥
    char city[50];           // 城市
    byte infoSwitchTime;     //切换间隔时间
    bool fastUsd;            // 第一次使用?
    int contrast;            // OLED亮度(对比度)
    byte lastMonitorItem;    // 上次使用的检测项目
};
config_type config;
struct weather_config
{
    String statusCode;

    String day0WeatherText;    // day0实时天气描述
    byte day0WeatherCode;      // day0实时天气码
    byte day0WeatherDayCode;   // day0白天预报天气码
    byte day0WeatherNightCode; // day0夜晚预报天气码
    byte day0IndoorHumidity;   // day0室内实时湿度
    int day0IndoorDegree;      // day0室内实时温度
    int day0Degree;            // day0室外实时温度
    int day0High;              // day0最高气温
    int day0Low;               // day0最低气温

    byte day1WeatherNightCode;
    byte day1WeatherDayCode;
    int day1High;
    int day1Low;

    byte day2WeatherNightCode;
    byte day2WeatherDayCode;
    int day2High;
    int day2Low;

    String lastUpdate1; // 心知天气服务端更新日期
    String lastUpdate2; // 心知天气服务端天气预报更新日期
};
weather_config weatherInfo;

void setup()
{
    Serial.begin(115200);
    loadConfig();       // 从EEPROM中更新config
    if (config.fastUsd) // 首次使用初始化配置为默认值
    {
        // 初始化config
        strcpy(config.city, "请配置城市(使用拼音或输入'ip'自动定位)");
        config.fastUsd = false;
        strcpy(config.SSID, "请配置WiFi网络");
        strcpy(config.SSIDPW, "Not configured");
        strcpy(config.serverIP, "请配置服务器ip地址和端口");
        config.serverPort = 0;
        config.infoSwitchTime = 5;
        strcpy(config.xinzhiKey, "Not configured");
        config.contrast = 255;
        config.lastMonitorItem = 1;
        saveConfig();
    }
    //pinMode(ledPin, OUTPUT);
    //digitalWrite(LED_BUILTIN, HIGH); // 关闭LED指示灯
    monitorItem = config.lastMonitorItem;
    WiFi.mode(WIFI_STA);
    webserver.begin(); // 网页服务器
    webserverInit();
    u8g2.begin();
    u8g2.enableUTF8Print();
    weatherNow.config(config.xinzhiKey, config.city, "c"); // 初始化心知天气配置
    forecast.config(config.xinzhiKey, config.city, "c");   // 初始化心知天气配置
    swInfoTimer.once(config.infoSwitchTime, infoIndexSw);  // 开启切换信息显示帧
    btn1Init();
    //btn2Init();
    checkWiFiConnectedTicker.attach_ms(200, checkWiFiConnected);
    checkServerConnectedTicker.attach_ms(500, checkServerConnected);
    getServerInfoTicker.attach(30, [] { getWeatherInfoFlag = 1; }); // 每隔20s通知主循环获取天气信息
    getWeatherInfoTicker.attach(3, [] { getServerInfoFlag = 1; });  // 每隔2秒通知主循环获取服务器信息
}
// 网络连接检测,
void loop()
{
    switch (mainIndex)
    {
    case 1:               // 检测界面
        tryWiFiConnect(); //wifi监测
        switch (monitorItem)
        {
        case 1:
            // 监控面板  WiFi连接正常在执行下面的服务器检测和服务器信息获取
            tryServerConnect(); // 当Wifi连接正常,与服务器的连接不正常且到了重新连接服务器的时间会重新连接
            getServaerInfo();
            break;
        case 2:
            // 天气面板
            getWeatherInfo(); // 请求天气数据
            break;
        }
        break;
    case 2:
        /* 设置界面 */
        webserver.handleClient(); // ESP8266服务器检测新客户端连接
        break;
    }
    OLEDDraw();                  // 屏幕绘制
    btn1.LOOPButtonController(); // 按钮循环检测
    //btn2.LOOPButtonController(); // 按钮循环检测
}
void tryWiFiConnect()
{
    // 基础网路连接:根据状态码重连网络
    if (!isWiFiConnected && WiFiReconnectTimeout >= 70) // wifi未连接,并且距离上次重新连接超过了15s,再次尝试重新连接
    {
        //Serial.println("尝试重新连接WiFi");
        WiFi.disconnect();
        WiFi.begin(config.SSID, config.SSIDPW); // 尝试重新连接WiFi
        WiFiReconnectTimeout = 0;
    }
}
void tryServerConnect()
{
    if (!isServerConnected && isWiFiConnected && serverReconnectTimeout >= 30)
    {
        client.stop();
        client.connect(config.serverIP, config.serverPort);
        serverReconnectTimeout = 0; // 重新开始重连服务器的定时
    }
}
// 定时检测客户端和服务器的连接状态并更新连接状态码,以及重连后的重置重连中的操作
void checkWiFiConnected()
{
    if (mainIndex != 1)
        return;                        // 不在检主页面直接退出
    if (WiFi.status() != WL_CONNECTED) // WiFi未连接
    {
        isWiFiConnected = 0;    // 降级连接状态检测码
        WiFiReconnectTimeout++; // 开启重连WiFi定时器
    }
    else
    {
        isWiFiConnected = 1;       // 更新状态码
        WiFiReconnectTimeout = 50; // 重置loop中的重连超时计数器
        gifTicker.detach();        // 关闭gif索引定时器,停止改变loading动画的索引
        loadGIndxFstTimer = 1;     // 重置gif索引定时器开启开关为开,下次连接加载动画使用
    }
}
void checkServerConnected()
{
    // 连接服务器部分
    if (mainIndex != 1 || monitorItem != 1)
        return;
    if (!client.connected()) // 未连接到服务器
    {
        isServerConnected = false; // 降级连接状态检测码为连接WiFi未连接服务器1
        serverReconnectTimeout++;  // 开启重连WiFi定时器}
    }
    else // 服务器已连接,但状态码为未连接
    {
        isServerConnected = true;    // 更新状态码
        serverReconnectTimeout = 20; // 重置服务器下次重连间隔时间
        gifTicker.detach();          // 关闭索引定时器
        loadGIndxFstTimer = 1;       // 下次连接加载动画使用
    }
}
// 发送获取服务端系统信息请求
void getServaerInfo()
{
    if (isServerConnected && getServerInfoFlag)
    {
        client.write("get");    // 请求数据
        Tcp_Handler(readTcp()); // 数据处理
        getServerInfoFlag = 0;
    }
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
        byte num = 0;
        ServerLoading = 0;                                                             // 关闭加载动画
        data = (data.substring(1, data.length() - 1)).substring(0, data.length() - 2); // 去掉首尾字符
        // 计算服务器发送的检测项目个数 ,字符串有几个"\\",就有n+1个项目
        for (byte i = 0; i < data.length() - 1; i++)
        {
            if (data.charAt(i) == '\\')
                num++;
        }
        infoItemsNum = num + 1; // 更新服务器发送的项目数量
        // 处理获取到的字符到信息数组中infoArr
        for (byte i = 0; i < infoItemsNum; i++)
        {
            tempStr = (getValue(data, '\\', i));
            for (byte j = 0; j < 6; j++)
                infoArr[i][j] = getValue(tempStr, '@', j);
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

//获取天气信息
void getWeatherInfo()
{
    if (getWeatherInfoFlag && isWiFiConnected) //WiFi连接正常且到了获取天气数据的时间了
    {
        //Serial.println("请求心知天气");
        getXinzhiInfo();
        getIndoorInfo();
        getWeatherInfoFlag = 0;
    }
}

// 获取室内温度信息
void getIndoorInfo()
{
    static byte timerOut = 0;
    float tempH, tempT;

    if (sht30.get() == 0) // 温度获取正确
    {
        //Serial.println("温度获取成功");
        tempH = sht30.humidity;
        tempT = sht30.cTemp;
        Serial.println(tempH);
        Serial.println(tempT);
    }
    else
    {
        //Serial.println("温度获取失败");
        tempH = sht30.humidity;
        tempT = sht30.cTemp;
        //Serial.println(tempH);
        //Serial.println(tempT);
        if (++timerOut >= 10)
        {
            //Serial.println("温度失败展示错误码");
            timerOut = 0;
            weatherInfo.day0IndoorHumidity = 99;
            weatherInfo.day0IndoorDegree = 99; // 错误
        }
        return; // 停止下面的数据赋值
    }
    weatherInfo.day0IndoorHumidity = (int)round(tempH);
    weatherInfo.day0IndoorDegree = (int)round(tempT);
}

// 获取心知天气数据信息
void getXinzhiInfo()
{
    static byte timerOut = 0; // 当连续10次错误后才进行错误码更新,频繁出现错误界面太影响体验
    if (weatherNow.update())  // 获取心知的实时天气
    {
        timerOut = 0;
        weatherInfo.statusCode = ""; // 心知天气在获取数据成功的状态下不会返回错误码的,程序开始设置一个空字符串,主函数判断状态码的数据长度,以此来判断有没有错误
        weatherInfo.day0WeatherText = weatherNow.getWeatherText();
        weatherInfo.day0WeatherCode = weatherNow.getWeatherCode();
        weatherInfo.day0Degree = weatherNow.getDegree();
        weatherInfo.lastUpdate1 = weatherNow.getLastUpdate();
    }
    else
    {
        if (++timerOut >= 10)
        {
            timerOut = 0;
            weatherInfo.statusCode = weatherNow.getServerCode(); //获取错误码
        }
        return;
    }
    if (forecast.update()) // 获取心知的天气预报
    {
        timerOut = 0;
        weatherInfo.statusCode = "";
        weatherInfo.day0WeatherNightCode = forecast.getNightCode(0);
        weatherInfo.day0WeatherDayCode = forecast.getDayCode(0);
        weatherInfo.day0Low = forecast.getLow(0);   // day0最低气温
        weatherInfo.day0High = forecast.getHigh(0); // day0最高气温

        weatherInfo.day1WeatherNightCode = forecast.getNightCode(1); //day1天气
        weatherInfo.day1WeatherDayCode = forecast.getDayCode(1);
        weatherInfo.day1Low = forecast.getLow(1);   // day1最低气温
        weatherInfo.day1High = forecast.getHigh(1); // day1最高气温

        weatherInfo.day2WeatherNightCode = forecast.getNightCode(2);
        weatherInfo.day2WeatherDayCode = forecast.getDayCode(2);
        weatherInfo.day2Low = forecast.getLow(2);   // 最低气温
        weatherInfo.day2High = forecast.getHigh(2); // 最高气温

        weatherInfo.lastUpdate2 = weatherNow.getLastUpdate();
    }
    else
    {
        if (++timerOut >= 10)
        {
            timerOut = 0;
            weatherInfo.statusCode = weatherNow.getServerCode(); //获取错误码
        }
        return;
    }
}
// 根据状态码来更绘制屏幕显示内容
void OLEDDraw()
{
    // 应用亮度
    u8g2.setContrast(config.contrast);
    // 检测面板
    if (mainIndex == 1)
        monitorDraw();
    else if (mainIndex == 2)
        settingDraw();
}
void monitorDraw()
{
    if (isWiFiConnected) // WiFi连接状态正常
    {
        switch (monitorItem)
        {
        case 1:
            if (!isServerConnected) //服务器未连接显式内容
                OLEDLoadDraw(gifList, -15, 0, server, 128 - 32, 0, "Server", 128 - getFontWidth(u8g2_font_helvB12_te, "Server"), 64 - 16, 128 - getFontWidth(u8g2_font_t0_16b_tf, "Server") - 32, 64);
            else
            {
                if (ServerLoading) // 服务端获取系统数据没准备好
                    OLEDLoadDraw(gifList, -15, 0, link, 128 - 32, 0, "link", 128 - getFontWidth(u8g2_font_helvB12_te, "Link"), 64 - 16, 128 - getFontWidth(u8g2_font_t0_16b_tf, "Server") - 32, 64);
                else
                {
                    gifTicker.detach();    // 关闭索引定时器
                    loadGIndxFstTimer = 1; // 重置索引定时器开启开关为开,以便下次连接加载动画使用
                    serverInfoDraw();
                }
            }
            break;
        case 2:
            weatherDraw();
            break;
        }
    }
    else
        OLEDLoadDraw(gifList, 66 + 12, 0, wifi2, 0, 0, "WiFi", 0, 64 - 16, 0, 64);
}
// 未连接服务的加载界面
void OLEDLoadDraw(const unsigned char *const *gifList, byte gifX, byte gifY, const unsigned char *icon, byte iconX, byte iconY, const char *Heading, byte HeadingX, byte HeadingY, byte tipsX, byte tipsY)
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

// Loading动画帧切换
void gifIndexSw()
{
    if (++gifIndex >= 28)
        gifIndex = 0;
    gifTicker.once_ms(30, gifIndexSw);
}

// 系统信息绘制
void serverInfoDraw()
{
    u8g2.clearBuffer();
    // 提取数组的值
    const String view = infoArr[serverInfoIndex][0],
                 option = infoArr[serverInfoIndex][5],
                 key = infoArr[serverInfoIndex][1],
                 value = infoArr[serverInfoIndex][2],
                 unit = infoArr[serverInfoIndex][3],
                 all = infoArr[serverInfoIndex][4];
    float pl = value.toFloat() / all.toFloat(); // 已用占比
    //String splitMonitorItem[4];
    char pc[3];
    itoa(int(pl * 100), pc, 10);
    String pcstr(pc);
    pcstr.concat("%");
    // 视图模式
    if (view.toInt() == 1)
    {
        u8g2.setFontMode(1); // 文字透明
        u8g2.setFont(u8g2_font_lastapprenticebold_tr);
        u8g2.drawStr((128 - u8g2.getUTF8Width(key.c_str())) / 2, 17, key.c_str()); // key
        u8g2.setFont(u8g2_font_freedoomr25_tn);
        u8g2.drawStr(128 - u8g2.getUTF8Width(value.c_str()) - 34, 50, value.c_str()); // value
        if (option.toInt())
        {
            // 开启百分比框
            u8g2.drawRBox(96, 23, 32, 12, 0); // 百分比框
            u8g2.setDrawColor(0);             // 文字反色
            u8g2.setFont(u8g2_font_tenthinnerguys_tf);
            u8g2.drawStr(96 + (32 - u8g2.getUTF8Width(pcStr(value, all).c_str())) / 2, 50 - 16, pcstr.c_str()); // 百分比文字
            u8g2.setDrawColor(1);
        }
        u8g2.drawRBox(96, 36, 32, 12, 0); // 单位框
        u8g2.setDrawColor(0);             // 文字反色
        u8g2.setFont(u8g2_font_tenthinnerguys_tf);
        u8g2.drawStr(96 + (32 - u8g2.getUTF8Width(unit.c_str())) / 2, 50 - 3, unit.c_str()); // 单位文字
        u8g2.setDrawColor(1);
        // 进度条
        u8g2.setDrawColor(1);
        u8g2.drawFrame(0, 52, 128, 12); //
        u8g2.drawBox(0 + 2, 52 + 2, (int(floor(value.toFloat() / all.toFloat() * 124))), 8);
        u8g2.setFontMode(0);
    }
    else if (view.toInt() == 2)
    {
        String splitMonitorItem[4]; // 存放提取到的复合检测项目中的项目
        u8g2.setFontMode(1);
        // 进度条1
        // 提取服务端列表模式下的第一组数据
        for (byte i = 0; i < 4; i++)
            splitMonitorItem[i] = getValue(option, '#', i);
        u8g2.setFont(u8g2_font_mercutio_basic_nbp_tf);
        u8g2.drawStr(0, 14, splitMonitorItem[0].c_str()); //key
        u8g2.setFont(u8g2_font_t0_14b_mr);
        u8g2.drawStr(128 - u8g2.getUTF8Width((splitMonitorItem[1] + "|" + splitMonitorItem[3] + splitMonitorItem[2]).c_str()), 14, (splitMonitorItem[1] + "|" + splitMonitorItem[3] + splitMonitorItem[2]).c_str()); //value/all unit
        u8g2.drawRFrame(0, 16, 128, 16, 0);                                                                                                                                                                          // 百分比进度条外框
        u8g2.drawRBox(2, 18, int(floor(splitMonitorItem[1].toFloat() / splitMonitorItem[3].toFloat() * 124)), 12, 0);                                                                                                //百分比进度条 两侧各空白2像素
        u8g2.setDrawColor(2);                                                                                                                                                                                        // 文字反色
        u8g2.drawStr(2 + (124 - u8g2.getUTF8Width(pcStr(splitMonitorItem[1], splitMonitorItem[3]).c_str())) / 2, 28, pcStr(splitMonitorItem[1], splitMonitorItem[3]).c_str());                                       // 百分比文字
        u8g2.setDrawColor(1);
        // 进度条2
        u8g2.setFont(u8g2_font_mercutio_basic_nbp_tf);
        u8g2.drawStr(0, 46, key.c_str());                                                                                  // key
        u8g2.setFont(u8g2_font_t0_14b_mr);                                                                                 // u8g2_font_t0_14b_mr
        u8g2.drawStr(128 - u8g2.getUTF8Width((value + "|" + all + unit).c_str()), 46, (value + "|" + all + unit).c_str()); //value/all unit
        u8g2.drawRFrame(0, 48, 128, 16, 0);                                                                                // 百分比框
        u8g2.drawRBox(2, 50, int(floor(value.toFloat() / all.toFloat() * 124)), 12, 0);                                    // 百分比
        u8g2.drawRBox(2, 18, int(floor(splitMonitorItem[1].toFloat() / splitMonitorItem[3].toFloat() * 124)), 12, 0);      //百分比进度条 两侧各空白2像素
        u8g2.setDrawColor(2);                                                                                              // 文字反色
        u8g2.drawStr(2 + (124 - u8g2.getUTF8Width(pcStr(value, all).c_str())) / 2, 60, pcStr(value, all).c_str());         // 百分比文字
        u8g2.setDrawColor(1);
        u8g2.setFontMode(0);
        break;
    }
    else if (view.toInt() == 3)
    {
        // 格式 3@title@0@0@0@key1#value1#unit1#all1#2#2#2#2#
        String splitMonitorItems[3][4];
        for (int i = 0; i < 3; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                splitMonitorItems[i][j] = getValue(option, '#', i * 4 + j); // 从复合数据的option值中每四个提取
            }
        }
        u8g2.setFont(u8g2_font_wqy13_t_gb2312a);
        u8g2.drawUTF8((128-u8g2.getUTF8Width(key) / 2, 13, key.c_str()); // 3层信息的标题
        u8g2.drawUTF8(0, 29, splitMonitorItems[0][1].c_str());  // 第一层信息的key
        u8g2.drawUTF8(0, 29, splitMonitorItems[0][2].c_str());  // 第一层信息的key
        u8g2.drawUTF8((128-u8g2.getUTF8Width(uint), 29, splitMonitorItems[0][3].c_str());  // 第一层信息的unit
        u8g2.drawUTF8(0, 45, splitMonitorItems[1][1].c_str());  // 第2层信息的key
        u8g2.drawUTF8(0, 45, splitMonitorItems[1][2].c_str());  // 第2层信息的key
        u8g2.drawUTF8(0, 45, splitMonitorItems[1][3].c_str());  // 第2层信息的key
        u8g2.drawUTF8(0, 61, splitMonitorItems[2][1].c_str());  // 第3层信息的key
        u8g2.drawUTF8(0, 61, splitMonitorItems[2][2].c_str());  // 第3层信息的key
        u8g2.drawUTF8(0, 61, splitMonitorItems[2][3].c_str());  // 第3层信息的key

    } else if (view.toInt() == 4)
    {
        String splitMonitorItems[3][4];
        for (int i = 0; i < 3; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                splitMonitorItems[i][j] = getValue(option, '#', i * 4 + j); // 从复合数据的option值中每四个提取
            }
        }
    }
    u8g2.sendBuffer();
}

// 切换信息显示帧
void infoIndexSw()
{
    if (++serverInfoIndex >= infoItemsNum)
        serverInfoIndex = 0; // 切换服务器信息帧
    if (++weatherInfoIndex >= 3)
        weatherInfoIndex = 0; // 切换天气信息帧
    swInfoTimer.once(config.infoSwitchTime, infoIndexSw);
}

// 绘制切换显示帧调速界面
void settingDraw()
{
    u8g2.clearBuffer();
    switch (settingItem)
    {
    case 1: // 主界面各项信息切换
        settingItemDraw("Switch Speed", config.infoSwitchTime, 10, "s");
        break;
    case 2:
        settingItemDraw("Bright Lv", config.contrast, 255, "Lv");
        break;
    case 3:
        WiFi_APDraw();
        break;
    }
    u8g2.sendBuffer();
}
void settingItemDraw(const char *head, byte value, byte maxValue, const char *unit)
{
    char str[4];          // 创建一个3个字符的空字符串,来保存转换后的传递的value值
    itoa(value, str, 10); // 转换传递的值为char
    u8g2.drawRFrame(0, 0, 128, 64, 0);
    u8g2.setFontMode(1); // 文字透明
    u8g2.setFont(u8g2_font_lastapprenticebold_tr);
    u8g2.drawStr((128 - u8g2.getUTF8Width(head)) / 2, 17, head); // 标题
    u8g2.setFont(u8g2_font_freedoomr25_tn);
    u8g2.drawStr(128 - u8g2.getUTF8Width(str) - 34, 50, str); // value
    u8g2.drawRBox(96, 36, 32, 12, 0);                         // 单位框
    u8g2.setDrawColor(0);                                     // 文字反色
    u8g2.setFont(u8g2_font_Pixellari_tf);
    u8g2.drawStr(96 + (32 - u8g2.getUTF8Width(unit)) / 2, 50 - 3, unit); // 单位文字
    u8g2.setDrawColor(1);
    // 百分比进度条
    u8g2.setDrawColor(1);
    u8g2.drawFrame(0, 52, 128, 12);
    u8g2.drawBox(2, 54, (int(floor(value / float(maxValue) * 124))), 8);
    u8g2.setFontMode(0);
}
void WiFi_APDraw()
{
}
// 获取字符宽度
byte getFontWidth(const uint8_t *font, const char *str)
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
//按钮初始化 : 绑定事件函数
void btn1Init()
{
    btn1.onActionPressed(btn1Handler);     // 按下
    btn1.onPressShortRelease(btn1Handler); //短按弹起
    btn1.onPressLongRelease(btn1Handler);  // 长按弹起
    btn1.onHoldStart(btn1Handler);         // 开始进入长按保持状态
    btn1.onHoldContinuous(btn1Handler);    // 长按保持状态
    btn1.onHoldRelease(btn1Handler);       // 长按保持状态弹起
    btn1.onMultiClick(btn1Handler);        // 多击
}
// void btn2Init()
// {
//     btn2.onActionPressed(btn2Handler);     // 按下
//     btn2.onPressShortRelease(btn2Handler); //短按弹起
//     btn2.onPressLongRelease(btn2Handler);  // 长按弹起
//     btn2.onHoldStart(btn2Handler);         // 开始进入长按保持状态
//     btn2.onHoldContinuous(btn2Handler);    // 长按保持状态
//     btn2.onHoldRelease(btn2Handler);       // 长按保持状态弹起
//     btn2.onMultiClick(btn2Handler);        // 多击
// }
// 按钮使用GPIO10引脚的,GPIO9的会触发ESP8266重启,我没能力解决
// 下面操作按钮触发的指示灯代码暂时注释掉,esp12f内置指示灯和OLEDIIC引脚冲突
// 长按在主页和设置页切换
// 主页单击暂停继续,设置单击增加值
// 主页双击切换检测项目,设置双节切换设置项
void btn1Handler()
{
    static byte lastMonitorItem;
    static long lastTime = millis();
    long nowTime = millis();
    switch (btn1.getClickType()) //判断按钮事件类型,进行不同的处理
    {
    case _ActionPressed: // 按下
        //digitalWrite(LED_BUILTIN, LOW); // 开启LED指示灯
        break;
        //单击
    case _PressShortRelease:
        //digitalWrite(LED_BUILTIN, HIGH); // 关闭LED指示灯
        switch (mainIndex)
        {
        case 1:
            // 主界面单击暂停继续播放
            break;
        case 2:
            // 设置界面单击更改值
            switch (settingItem)
            {
            case 1: // 调速界面下调节数值
                if (++config.infoSwitchTime >= 11)
                    config.infoSwitchTime = 1;
                break;
            case 2: // 亮度调节界面下调节数值(3级亮度调节)
                config.contrast += 127;
                if (config.contrast >= 256)
                    config.contrast = 1;
                break;
            }
            saveConfig(); // 保存到EEPROM
            break;
        }
        break;
    case _PressLongRelease: // 长按(和上面单击按钮作用一样)
                            //digitalWrite(LED_BUILTIN, HIGH); // 关闭LED指示灯
        switch (mainIndex)
        {
        case 1:
            // 主界面单击暂停继续播放
            break;
        case 2:
            // 设置界面单击更改值
            switch (settingItem)
            {
            case 1: // 调速界面下调节数值
                if (++config.infoSwitchTime >= 11)
                    config.infoSwitchTime = 1;
                break;
            case 2: // 亮度调节界面下调节数值(3级亮度调节)
                config.contrast += 127;
                if (config.contrast >= 256)
                    config.contrast = 1;
                break;
            }
            saveConfig(); // 保存到EEPROM
            break;
        }
        break;
    case _HoldRelease: // 按住结束 切换主界面
        //digitalWrite(LED_BUILTIN, HIGH); // 关闭LED指示灯
        switch (mainIndex)
        {
        case 1:                                           // 检测界面进入设置
            WiFi.disconnect(true);                        // 关闭wifi连接
            WiFi.mode(WIFI_AP);                           // 开启ap模式
            mainIndex = 2;                                // 进入设置页面
            WiFi.softAP("OLEDMonitorClient", "12345678"); //esp8266ap配置
            lastMonitorItem = monitorItem;                // 保存当前屏幕显示模式
            break;
        case 2:                          // 在设置页面,退回到主页面
            WiFi.softAPdisconnect(true); // 关闭热点模式
            WiFi.mode(WIFI_STA);
            mainIndex = 1;
            monitorItem = lastMonitorItem;
            break;
        }
        break;
    case _HoldContinuous: // 按住只是闪灯提醒用户,按住弹起才会执行设置
        if (nowTime - lastTime >= 200)
        {
            //digitalWrite(LED_BUILTIN, !digitalRead(ledPin)); // 闪烁led灯
            lastTime = millis();
        }
        break;
    case _MultiClicks:
        // 先灭灯
        //digitalWrite(ledPin, HIGH); // 关闭LED指示灯
        switch (btn1.getNumberOfMultiClicks())
        {
        case 2: //双击
            //判断所在界面,是主界面还是设置界面
            switch (mainIndex)
            {
            case 1: // 在主界面切换轮播
                if (++monitorItem >= 3)
                    monitorItem = 1;
                config.lastMonitorItem = monitorItem; // 保存到EEPROME,下次启动继续这个检测项目
                saveConfig();
                serverInfoIndex = 0;    // 服务器监控状态返回第一帧
                weatherInfoIndex = 0;   // 切换信息显示界面需要把天气或者服务器信息显示帧重置为第0帧
                getWeatherInfoFlag = 1; // 立即请求天气数据
                getServerInfoFlag = 1;
                break;
            case 2: // 在设置界面双击切换设置项
                if (++settingItem >= 3)
                    settingItem = 1;
                break;
            }
            break;
        }
        break;
    }
}

// void btn1Handler()
// {
//     static byte lastMonitorItem;
//     static long lastTime = millis();
//     long nowTime = millis();
//     switch (btn1.getClickType()) //判断按钮事件类型,进行不同的处理
//     {
//     case _ActionPressed: // 按下
//         //digitalWrite(LED_BUILTIN, LOW); // 开启LED指示灯
//         break;
//         //单击
//     case _PressShortRelease:
//         //digitalWrite(LED_BUILTIN, HIGH); // 关闭LED指示灯
//         switch (mainIndex)
//         {
//         case 1:
//             // 暂停播放
//             break;
//         case 2:
//             // 切换设置项目
//             if (++settingItem >= 3)
//                 settingItem = 1;
//             break;
//         }
//         break;
//     case _PressLongRelease: // 长按(和上面单击按钮作用一样)
//         //digitalWrite(LED_BUILTIN, HIGH); // 关闭LED指示灯
//         switch (mainIndex)
//         {
//         case 1:
//             // 暂停播放
//             break;
//         case 2:
//             // 切换设置项目
//             if (++settingItem >= 3)
//                 settingItem = 1;
//             break;
//         }
//         break;
//     case _HoldRelease: // 按住结束 切换主界面
//         //digitalWrite(LED_BUILTIN, HIGH); // 关闭LED指示灯
//         switch (mainIndex)
//         {
//         case 1:                                           // 检测界面进入设置
//             WiFi.mode(WIFI_AP);                           // 开启ap模式
//             mainIndex = 2;                                // 进入设置页面
//             WiFi.softAP("OLEDMonitorClient", "12345678"); //esp8266ap配置
//             lastMonitorItem = monitorItem;                // 保存当前屏幕显示模式
//             break;
//         case 2: // 在设置页面,退回到主页面
//             WiFi.mode(WIFI_STA);
//             mainIndex = 1;
//             monitorItem = lastMonitorItem;
//             break;
//         }
//         break;
//     case _HoldContinuous: // 按住只是闪灯提醒用户,按住弹起才会执行设置
//         if (nowTime - lastTime >= 200)
//         {
//             //digitalWrite(LED_BUILTIN, !digitalRead(ledPin)); // 闪烁led灯
//             lastTime = millis();
//         }
//         break;
//     }
// }
// //按钮2处理
// void btn2Handler()
// {
//     static byte lastMonitorItem;
//     static long lastTime = millis();
//     long nowTime = millis();
//     switch (btn2.getClickType()) //判断按钮事件类型,进行不同的处理
//     {
//     case _ActionPressed: // 按下
//         //digitalWrite(LED_BUILTIN, LOW); // 开启LED指示灯
//         break;
//         //单击
//     case _PressShortRelease:
//         //digitalWrite(LED_BUILTIN, HIGH); // 关闭LED指示灯
//         switch (mainIndex)
//         {
//         case 1: // 在主界面切换轮播
//             if (monitorItem++ >= 3)
//                 monitorItem = 1;
//             serverInfoIndex = 0;    // 服务器监控状态返回第一帧
//             weatherInfoIndex = 0;   // 切换信息显示界面需要把天气或者服务器信息显示帧重置为第0帧
//             getWeatherInfoFlag = 1; // 立即请求天气数据
//             getServerInfoFlag = 1;
//             break;
//         case 2: // 在设置界面更改值
//             switch (settingItem)
//             {
//             case 1: // 调速界面下调节数值
//                 if (++config.infoSwitchTime >= 11)
//                     config.infoSwitchTime = 1;
//                 break;
//             case 2: // 亮度调节界面下调节数值(3级亮度调节)
//                 config.contrast += 127;
//                 if (config.contrast >= 256)
//                     config.contrast = 1;
//                 break;
//             }
//             saveConfig(); // 保存到EEPROM
//         }
//         break;
//     case _PressLongRelease: // 长按(和上面单击按钮作用一样)
//         //digitalWrite(LED_BUILTIN, HIGH); // 关闭LED指示灯
//         switch (mainIndex)
//         {
//         case 1: // 在主界面切换轮播
//             if (monitorItem++ >= 3)
//                 monitorItem = 1;
//             serverInfoIndex = 0;    // 服务器监控状态返回第一帧
//             weatherInfoIndex = 0;   // 切换信息显示界面需要把天气或者服务器信息显示帧重置为第0帧
//             getWeatherInfoFlag = 1; // 立即请求天气数据
//             getServerInfoFlag = 1;
//             break;
//         case 2: // 在设置界面更改值
//             switch (settingItem)
//             {
//             case 1: // 调速界面下调节数值
//                 if (++config.infoSwitchTime >= 11)
//                     config.infoSwitchTime = 1;
//                 break;
//             case 2: // 亮度调节界面下调节数值(3级亮度调节)
//                 config.contrast += 127;
//                 if (config.contrast >= 256)
//                     config.contrast = 1;
//                 break;
//             }
//             saveConfig(); // 保存到EEPROM
//         }
//         break;
//     case _HoldRelease: // 按住结束 切换主界面
//         //digitalWrite(LED_BUILTIN, HIGH); // 关闭LED指示灯
//         break;
//     case _HoldContinuous: // 按住只是闪灯提醒用户,按住弹起才会执行设置
//         if (nowTime - lastTime >= 200)
//         {
//             //digitalWrite(LED_BUILTIN, !digitalRead(ledPin)); // 闪烁led灯
//             lastTime = millis();
//         }
//         break;
//     }
// }
//}
void ErrorDraw(const unsigned char *icon, const char *Heading, const char *Content, String errorCode)
{
    u8g2.drawXBMP(0, 0, 50, 50, icon); // 错误icon
    u8g2.setFont(u8g2_font_tenthinnerguys_tf);
    u8g2.drawUTF8(50 + (128 - 50 - u8g2.getUTF8Width(Heading)) / 2, 23, Heading); // 错误标题
    u8g2.setFont(u8g2_font_wqy13_t_gb2312a);
    u8g2.drawUTF8(50 + (128 - 50 - u8g2.getUTF8Width(Content)) / 2, 40, Content); // 错误内容
    u8g2.drawLine(0, 50, 128, 50);                                                // 底部横线
    u8g2.drawUTF8(0, 63, "错误代码:");                                            // 底部错误码显示
    u8g2.setFont(u8g2_font_tenthinnerguys_tf);
    u8g2.drawStr(128 - u8g2.getUTF8Width(errorCode.c_str()), 63, errorCode.c_str()); // 错误码
}
// 绘制天气界面
void weatherDraw()
{
    String emptyStr = ""; // 空字符串
    u8g2.clearBuffer();
    if (weatherInfo.statusCode.length() != 0) // 反回了错误码
        ErrorDraw(nan99, "Error!", "数据错误", weatherInfo.statusCode);
    else
    {
        // 绘制天气界面
        switch (weatherInfoIndex)
        {
        case 0:                                                                        // 当日天气
            u8g2.drawXBMP(0, 0, 50, 50, weatherIconList[weatherInfo.day0WeatherCode]); // 天气icon
            u8g2.setFont(u8g2_font_wqy13_t_gb2312a);
            u8g2.drawUTF8(50 + (128 - 50 - u8g2.getUTF8Width(weatherInfo.day0WeatherText.c_str())) / 2, 13, weatherInfo.day0WeatherText.c_str()); // 天气描述
            u8g2.drawUTF8(0, 63, (emptyStr + "温:" + weatherInfo.day0IndoorDegree + "°C").c_str());                                               // 底部室内信息
            u8g2.drawUTF8(128 - u8g2.getUTF8Width((emptyStr + "湿:" + weatherInfo.day0IndoorHumidity + "%").c_str()), 63, (emptyStr + "湿:" + weatherInfo.day0IndoorHumidity + "%").c_str());
            u8g2.setFont(u8g2_font_freedoomr25_tn);
            u8g2.drawUTF8(50 + (128 - 50 - u8g2.getUTF8Width((emptyStr + weatherInfo.day0Degree).c_str())) / 2, 45, (emptyStr + weatherInfo.day0Degree).c_str());                                // 当日室外温度
            u8g2.drawFrame(50 + (128 - 50 - u8g2.getUTF8Width((emptyStr + weatherInfo.day0Degree).c_str())) / 2 + u8g2.getUTF8Width((emptyStr + weatherInfo.day0Degree).c_str()) + 3, 19, 6, 6); // 摄氏度
            u8g2.drawFrame(50 + (128 - 50 - u8g2.getUTF8Width((emptyStr + weatherInfo.day0Degree).c_str())) / 2 + u8g2.getUTF8Width((emptyStr + weatherInfo.day0Degree).c_str()) + 4, 19 + 1, 4, 4);
            u8g2.drawLine(0, 50, 128, 50);
            break;
        case 1:                                                                              // 当时概况
            u8g2.drawXBMP(0, 0, 50, 50, weatherIconList[weatherInfo.day0WeatherDayCode]);    // 白天
            u8g2.drawXBMP(61, 1, 50, 50, weatherIconList[weatherInfo.day0WeatherNightCode]); // 夜晚
            u8g2.drawXBMP(0, 52, 14, 14, weatherIconList[39]);                               // 白天
            u8g2.drawXBMP(55, 52, 14, 14, weatherIconList[40]);                              // 夜晚
            u8g2.drawTriangle(52, 20, 52, 30, 60, 25);                                       // 白天夜晚分隔三角
            u8g2.drawBox(128 - 17, 0, 17, 64);                                               // 侧栏盒子
            u8g2.drawLine(0, 50, 100, 50);                                                   // 上下分割
            u8g2.setFontDirection(1);
            u8g2.setFontMode(1); // 开启透明模式
            u8g2.setDrawColor(2);
            u8g2.setFont(u8g2_font_wqy13_t_gb2312a);
            u8g2.drawStr(128 - 14, 5, "Today");
            u8g2.setDrawColor(0);
            u8g2.drawTriangle(124, 44, 124, 56, 114, 50); // 侧栏箭头
            u8g2.setDrawColor(1);
            u8g2.setFontDirection(0);
            u8g2.drawUTF8(15, 63, ":");
            u8g2.drawUTF8(68, 63, ":");
            u8g2.drawUTF8(22, 63, (((String)weatherInfo.day0High) + "°").c_str());
            u8g2.drawUTF8(76, 63, (((String)weatherInfo.day0Low) + "°").c_str());
            break;
        case 2:                                                                            //天气预报
            u8g2.drawXBMP(0, 0, 50, 50, weatherIconList[weatherInfo.day1WeatherDayCode]);  // 明天
            u8g2.drawXBMP(61, 1, 50, 50, weatherIconList[weatherInfo.day2WeatherDayCode]); // 后天
            u8g2.drawBox(50 + 5, 15, 2, 15);                                               // 左右分隔
            u8g2.drawBox(128 - 17, 0, 17, 64);                                             // 侧栏盒子
            u8g2.drawLine(0, 50, 100, 50);                                                 // 上下分割
            u8g2.setFontDirection(1);
            u8g2.setFontMode(1); // 开启透明模式
            u8g2.setDrawColor(2);
            u8g2.setFont(u8g2_font_wqy13_t_gb2312a);
            u8g2.drawStr(128 - 14, 5, "Day 2&3");
            u8g2.setDrawColor(0);
            u8g2.setDrawColor(1);
            u8g2.setFontDirection(0);
            u8g2.drawUTF8(0, 63, (emptyStr + "D2: " + weatherInfo.day1High + "°").c_str());
            u8g2.drawUTF8(110 - u8g2.getUTF8Width((emptyStr + "D3: " + weatherInfo.day2High + "°").c_str()), 63, (emptyStr + "D3: " + weatherInfo.day2High + "°").c_str());
            break;
        }
    }
    u8g2.sendBuffer();
}

void webserverInit()
{
    webserver.on("/", HTTP_GET, webRootHandle);
    webserver.on("/WiFi", HTTP_POST, webWiFiHandle);
    webserver.on("/Xinzhi", HTTP_POST, webXinzhiHandle);
    webserver.on("/Server", HTTP_POST, webServerHandle);
}

void webRootHandle()
{
    loadConfig();
    String webStr = "";
    webStr += "<!DOCTYPE html><head><meta charset=\"UTF-8\"></head>";
    webStr += "<body>";
    webStr += "<h2 style=\"text-align:center\">-更新WiFi配置-</h2>";
    webStr += "<form style=\"text-align:center\" action=\"/WiFi\" method=\"POST\">";
    webStr += "<span>WiFi账号: </span><input type=\"text\" name=\"SSID\">";
    webStr += "<br><span>WiFi密码: </span><input type=\"text\" name=\"SSIDPW\"><br>";
    webStr += "<input type=\"submit\" value=\"提交\"></form>";
    ///////////////////////////////////////////////////////////
    webStr += "<h2 style=\"text-align:center\">-更新服务端配置-</h2>";
    webStr += "<form style=\"text-align:center\" action=\"/Server\" method=\"POST\">";
    webStr += "<span>Server地址: </span><input type=\"text\" name=\"serverIP\">";
    webStr += "<br><ssssspan>Server端口: </span><input type=\"text\" name=\"serverPort\"><br>";
    webStr += "<input type=\"submit\" value=\"提交\"></form>";
    ////////////////////////////////////////////////////////////
    webStr += "<h2 style=\"text-align:center\">-更新心知天气配置-</h2>";
    webStr += "<form style=\"text-align:center\" action=\"/Xinzhi\" method=\"POST\">";
    webStr += "<span>心知天气私钥: </span><input type=\"text\" name=\"xinzhiKey\">";
    webStr += "<br><span>城市(拼音): </span><input type=\"text\" name=\"city\"><br>";
    webStr += "<input type=\"submit\" value=\"提交\"></form>";
    /////////////////////////////////////////////////////////////
    webStr += "<hr><h2 style=\"text-align:center\">当前配置</h2>";
    webStr = webStr + "<ul style=\"text-align: center;list-style-type:none;padding-inline-start:0;\">";
    webStr = webStr + "<li>WiFi账号: " + config.SSID + " | " + "WiFi密码: " + config.SSIDPW + "</ li> ";
    webStr = webStr + "<li>Server地址: " + config.serverIP + " | " + "Server端口: " + config.serverPort + "</ li> ";
    webStr = webStr + "<li>心知天气私钥: " + config.xinzhiKey + " | " + "城市(拼音): " + config.city + "</ li> ";
    webStr += "</ul>";
    webStr += "</body>";
    webserver.send(200, "text/html", webStr);
}

void webWiFiHandle()
{
    // 浏览器按下WiFi表单出的按钮发送的/wifi请求
    // 获取浏览器一同发送的表单属性
    strcpy(config.SSID, webserver.arg("SSID").c_str());
    strcpy(config.SSIDPW, webserver.arg("SSIDPW").c_str()); // 更新config
    // 保存到EEPROM
    saveConfig();
    webRootHandle(); // 获取到浏览器发送的请求后返回首页更新
                     // WiFi重连
}

void webXinzhiHandle()
{
    strcpy(config.xinzhiKey, webserver.arg("xinzhiKey").c_str());
    strcpy(config.city, webserver.arg("city").c_str()); // 更新config
    weatherNow.config(config.xinzhiKey, config.city, "c");
    forecast.config(config.xinzhiKey, config.city, "c");
    saveConfig();
    webRootHandle(); // 获取到浏览器发送的请求后返回首页更新
                     // 心知天气重新获取
}

void webServerHandle()
{
    client.stop();
    strcpy(config.serverIP, webserver.arg("serverIP").c_str());
    config.serverPort = webserver.arg("serverPort").toInt(); // 更新config
    // 保存到EEPROM
    saveConfig();
    webRootHandle(); // 获取到浏览器发送的请求后返回首页更新
                     // 服务器重连
}

// config保存到EEPROME
void saveConfig()
{
    EEPROM.begin(1024);
    uint8_t *p = (uint8_t *)(&config);
    for (int i = 0; i < sizeof(config); i++)
        EEPROM.write(i, *(p + i));
    EEPROM.commit();
}

// 从EEPROM中更新config
void loadConfig()
{
    EEPROM.begin(1024);
    uint8_t *p = (uint8_t *)(&config);
    for (int i = 0; i < sizeof(config); i++)
        *(p + i) = EEPROM.read(i);
    EEPROM.commit();
}
