#include <DHT.h>
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
#define DHTTYPE DHT11 // 采用DHT11传感器 可以将DHT11字段替换为DHT22来使用DHT22型号传感器
#define DHTPIN 14     // gpio14引脚,也就是开发板标注d5引脚作为温湿度传感器的输入端
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);
// 图标数组
const unsigned char *const iconList[] U8X8_PROGMEM = {active, cpu, box, database, drive, global, nodejs, server, terminal, thermomete, wifi, link, wifi2};
// 加载动画数组 loading array
const unsigned char *const gifList[] U8X8_PROGMEM = {load1, load2, load3, load4, load5, load6, load7, load8, load9, load10, load11, load12, load13, load14, load15, load16, load17, load18, load19, load20, load21, load22, load23, load24, load25, load26, load27, load28};
// 天气图标数组
const unsigned char *const weatherIconList[42] U8X8_PROGMEM = {sunny00, sunny01, sunny02, sunny03, cloudy04, partlyCloudy05, partlyCloudy06, mostlyCloudy07, mostlyCloudy08, overcast09, shower10, thundershower11, thundershowerWithHail12, lightRain13, moderateRain14, heavyRain15, storm16, heavyStorm17, severeStorm18, iceRain19, sleet20, snowFlurry21, lightSnow22, moderateSnow23, heavySnow24, snowstorm25, dust26, sand27, duststorm28, sandstorm29, foggy30, haze31, windy32, blustery33, hurricane34, tropicalStorm35, tornado36, clod37, hot38, light39, night40};
Ticker isCON;               // 检测连接定时器
Ticker gifTicker;           // 更新loading gif 数组索引定时器
Ticker getServerInfoTicker; // 连接成功后定时向服务器获取请求
Ticker getWeatherInfoTicker;
Ticker swInfoTimer;    // 切换服务器监控信息显示帧
Ticker SPDAdjustTimer; // 长按超时定时器
WiFiClient client;     // 连接监控服务器Tcp客户端
ESP8266WebServer webserver(80);
WeatherNow weatherNow; // 建立WeatherNow对象用于获取心知天气信息
Forecast forecast;
DHT dht(DHTPIN, DHTTYPE);
String infoArr[50][6]; // 存放从服务器获取到的数据 !will be BUG!

byte infoItemsNum;      // 服务获取的项目数量
byte CONStatusCode = 0; // 状态码
byte CONTimer = 24;     // *连接计时器
byte gifIndex = 0;      // *loading gif帧数组索引
bool loadGIndxFstTimer = 1;
bool getWeatherInfoFlag = 0;    // 是否请求天气flag
bool ServerLoading = 1;         // *服务器是否加载中flag
byte weatherInfoIndex;          // *天气信息显示帧 (控制在oled轮播第天气信息项目)
byte serverInfoIndex = 0;       // *服务器信息显示帧 (控制在oled轮播第几个服务器监控信息项目)
const byte PROGMEM btn1Pin = 0; // 按钮引脚
const byte PROGMEM DTHPin = 14; // 温湿度传感器引脚
const byte PROGMEM ledPin = 2;  // led指示灯引脚
LogansGreatButton btn1(btn1Pin);
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
    bool notFastUsd;
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
    Serial.begin(9600);
    loadConfig();          // 从EEPROM中更新config
    if (config.notFastUsd) // 首次使用初始化配置为默认值
    {
        Serial.println("首次配置");
        // 初始化config
        strcpy(config.city, "Not configured");
        config.notFastUsd = false;
        strcpy(config.SSID, "Not configured");
        strcpy(config.SSIDPW, "Not configured");
        strcpy(config.serverIP, "000.000.000.000");
        config.serverPort = 0;
        config.infoSwitchTime = 5;
        strcpy(config.xinzhiKey, "Not configured");
        saveConfig();
    }
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, HIGH); // 关闭LED指示灯
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("OLEDMonitorClient", "12345678"); //esp8266ap配置
    webserver.begin();                            // 网页服务器
    webserverInit();
    u8g2.begin();
    u8g2.enableUTF8Print();
    isCON.once_ms(200, isCONFun);                                   // 定时检测连接状态
    getServerInfoTicker.attach(2, getServaerInfo);                  // 定时获取服务器信息
    getWeatherInfoTicker.attach(5, [] { getWeatherInfoFlag = 1; }); // 定时获取服务器信息
    if (config.infoSwitchTime == 0)                                 // 如果首次使用没有设定切换速度则默认是0
        config.infoSwitchTime = 5;                                  // 页面内切换默认速度
    weatherNow.config(config.xinzhiKey, config.city, "c");          // 初始化心知天气配置
    forecast.config(config.xinzhiKey, config.city, "c");            // 初始化心知天气配置
    swInfoTimer.once(config.infoSwitchTime, infoIndexSw);           // 开启切换信息显示帧
    btn1Init();                                                     // 按钮1初始化
}

void loop()
{
    // 基础网路连接:检测并连接网络和服务端
    if (CONStatusCode < 2) // 未连接服务器,根据连接检测更新的状态码来重连各级连接
    {
        switch (CONStatusCode)
        {
        case 0:
            if (CONTimer >= 50) // 10 秒超时重连
            {
                WiFi.disconnect();
                WiFi.begin(config.SSID, config.SSIDPW); // 尝试重新连接WiFi
                CONTimer = 0;
            };
            break;
        case 1:
            if (CONTimer >= 50) // 5 秒超时重连服务器
            {
                client.stop();
                client.connect(config.serverIP, config.serverPort);
                CONTimer = 0;
            };
        }
    }
    OLEDDraw();                  // 屏幕绘制
    btn1.LOOPButtonController(); // 按钮循环检测
    webserver.handleClient();    // ESP8266服务器检测新客户端连接
    if (getWeatherInfoFlag)
        getWeatherInfo();
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

// 按钮初始化:绑定事件函数
void btn1Init()
{
    btn1.onActionPressed(btn1Handler);     // 按下
    btn1.onPressShortRelease(btn1Handler); //短按弹起
    btn1.onPressLongRelease(btn1Handler);  // 长按弹起
    btn1.onHoldStart(btn1Handler);         // 开始进入长按保持状态
    btn1.onHoldContinuous(btn1Handler);    // 长按保持状态
    btn1.onHoldRelease(btn1Handler);       // 长按保持状态弹起
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
    case 1: //服务器未连接显式内容
        OLEDLoadDraw(gifList, -15, 0, server, 128 - 32, 0, "Server", 128 - getFontWidth(u8g2_font_helvB12_te, "Server"), 64 - 16, 128 - getFontWidth(u8g2_font_t0_16b_tf, "Server") - 32, 64);
        break;
    case 2:                // 屏幕绘制从服务器获取到的系统信息
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
    case 3:
        weatherDraw();
        break;
    case 4:
        SPDCtrlDraw();
    }
}

// 发送获取服务端系统信息请求
void getServaerInfo()
{
    if (CONStatusCode != 2) // 没进入服务器信息画面
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

//获取天气信息
void getWeatherInfo()
{
    getWeatherInfoFlag = 0; //等待5秒后继续请求
    if (CONStatusCode != 3)
        return; // 没进入天气页面(状态码4)不做处理
    getXinzhiInfo();
    getIndoorInfo();
}

// 获取室内温度信息
void getIndoorInfo()
{
    static byte timerOut = 0;
    float tempH, tempT;
    tempH = dht.readHumidity();
    tempT = dht.readTemperature();
    if (isnan(tempH) || isnan(tempH))
    {
        if (++timerOut >= 10)
        {
            timerOut = 0;
            weatherInfo.day0IndoorHumidity = 99; // 错误
            weatherInfo.day0IndoorDegree = 99;   // 错误
        }
        return;
    }
    weatherInfo.day0IndoorHumidity = (int)round(tempH);
    weatherInfo.day0IndoorDegree = (int)round(tempT);
}

// 获取心知天气数据信息
void getXinzhiInfo()
{
    static byte timerOut = 0; // 当连续10次错误后才进行错误码更新,频繁出现错误界面太影响体验
    Serial.println(timerOut);
    if (weatherNow.update()) // 获取心知的实时天气
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
void OLEDInfoDraw()
{
    u8g2.clearBuffer();
    // 提取数组的值
    const String view = infoArr[serverInfoIndex][0],
                 option = infoArr[serverInfoIndex][1],
                 key = infoArr[serverInfoIndex][2],
                 value = infoArr[serverInfoIndex][3],
                 unit = infoArr[serverInfoIndex][4],
                 all = infoArr[serverInfoIndex][5];
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
        break;
    case 2: // 视图2样式 进度条样式
        u8g2.setFontMode(1);
        // 进度条1
        // 提取服务端列表模式下的第一组数据
        for (byte i = 0; i < 4; i++)
        {
            ListInfoArr[i] = getValue(infoArr[serverInfoIndex][1], '#', i);
        }
        u8g2.setFont(u8g2_font_mercutio_basic_nbp_tf);
        u8g2.drawStr(0, 14, ListInfoArr[0].c_str()); //key
        u8g2.setFont(u8g2_font_t0_14b_mr);
        u8g2.drawStr(128 - u8g2.getUTF8Width((ListInfoArr[1] + "|" + ListInfoArr[3] + ListInfoArr[2]).c_str()), 14, (ListInfoArr[1] + "|" + ListInfoArr[3] + ListInfoArr[2]).c_str()); //value/all unit
        u8g2.drawRFrame(0, 16, 128, 16, 0);                                                                                                                                            // 百分比进度条外框
        u8g2.drawRBox(2, 18, int(floor(ListInfoArr[1].toFloat() / ListInfoArr[3].toFloat() * 124)), 12, 0);                                                                            //百分比进度条 两侧各空白2像素
        u8g2.setDrawColor(2);                                                                                                                                                          // 文字反色
        u8g2.drawStr(2 + (124 - u8g2.getUTF8Width(pcStr(ListInfoArr[1], ListInfoArr[3]).c_str())) / 2, 28, pcStr(ListInfoArr[1], ListInfoArr[3]).c_str());                             // 百分比文字
        u8g2.setDrawColor(1);
        // 进度条2
        u8g2.setFont(u8g2_font_mercutio_basic_nbp_tf);
        u8g2.drawStr(0, 46, key.c_str());                                                                                  // key
        u8g2.setFont(u8g2_font_t0_14b_mr);                                                                                 // u8g2_font_t0_14b_mr
        u8g2.drawStr(128 - u8g2.getUTF8Width((value + "|" + all + unit).c_str()), 46, (value + "|" + all + unit).c_str()); //value/all unit
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
    if (++serverInfoIndex >= infoItemsNum) // 切换服务器信息帧
        serverInfoIndex = 0;
    if (++weatherInfoIndex >= 3) // 切换天气信息帧
        weatherInfoIndex = 0;
    swInfoTimer.once(config.infoSwitchTime, infoIndexSw);
}

// 绘制切换显示帧调速界面
void SPDCtrlDraw()
{
    char str[4];
    itoa(config.infoSwitchTime, str, 10);
    u8g2.clearBuffer();
    u8g2.drawRFrame(0, 0, 128, 64, 0);
    u8g2.setFontMode(1); // 文字透明
    u8g2.setFont(u8g2_font_lastapprenticebold_tr);
    u8g2.drawStr((128 - u8g2.getUTF8Width("Switch Speed")) / 2, 17, "Switch Speed"); // 标题
    u8g2.setFont(u8g2_font_freedoomr25_tn);
    u8g2.drawStr(128 - u8g2.getUTF8Width(str) - 34, 50, str); // value
    u8g2.drawRBox(96, 36, 32, 12, 0);                         // 单位框
    u8g2.setDrawColor(0);                                     // 文字反色
    u8g2.setFont(u8g2_font_Pixellari_tf);
    u8g2.drawStr(96 + (32 - u8g2.getUTF8Width("s")) / 2, 50 - 3, "s"); // 单位文字
    u8g2.setDrawColor(1);
    // 百分比进度条
    u8g2.setDrawColor(1);
    u8g2.drawFrame(0, 52, 128, 12);
    u8g2.drawBox(2, 54, (int(floor(config.infoSwitchTime / float(10) * 124))), 8);
    u8g2.setFontMode(0);
    u8g2.sendBuffer();
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

// 按钮1处理函数
void btn1Handler()
{
    static byte lastCONStatusCode;
    switch (btn1.getClickType()) //判断按钮事件类型,进行不同的处理
    {
    case _ActionPressed:           // 按下
        digitalWrite(ledPin, LOW); // 开启LED指示灯
        break;

    case _PressShortRelease:        //单击弹起
        digitalWrite(ledPin, HIGH); // 关闭LED指示灯
        switch (CONStatusCode)
        {
        case 4: //状态码4(即进图调速模式)情况下单击将调节切换帧显示速度
            SPDAdjustTimer.detach();
            SPDAdjustTimer.once(3, closeSPDCtrl, lastCONStatusCode); //长时间无操作自动退出调速界面
            config.infoSwitchTime++;
            if (config.infoSwitchTime >= 11)
                config.infoSwitchTime = 1;
            saveConfig(); // 保存到EEPROM
            break;
        case 2:
        case 3: // 2,3情况下将来回切换画面
            if (++CONStatusCode >= 4)
                CONStatusCode = 2;
            serverInfoIndex = 0;
            weatherInfoIndex = 0; // 切换信息显示界面需要把天气或者服务器信息显示帧重置为第0帧
            break;
        }
        break;

    case _PressLongRelease:
        digitalWrite(ledPin, HIGH); // 关闭LED指示灯
        switch (CONStatusCode)
        {
        case 4: //状态码4(即进图调速模式)情况下单击将调节切换帧显示速度
            SPDAdjustTimer.detach();
            SPDAdjustTimer.once(3, closeSPDCtrl, lastCONStatusCode);
            config.infoSwitchTime++;
            if (config.infoSwitchTime >= 11)
                config.infoSwitchTime = 1;
            saveConfig(); // 保存到EEPROM
            break;
        case 2:
        case 3: // 2,3情况下将来回切换画面
            if (++CONStatusCode >= 4)
                CONStatusCode = 2;
            break;
        }
        break;

    case _HoldRelease:              // 按住弹起
        digitalWrite(ledPin, HIGH); // 关闭LED指示灯
        switch (CONStatusCode)
        {
        case 4: //状态码4(即进图调速模式)情况下长按将退出调速模式
            closeSPDCtrl(lastCONStatusCode);
            break;
        case 2:
        case 3:                                //状态码23长按进入将调节切换帧显示速度模式
            lastCONStatusCode = CONStatusCode; // 保存当前屏幕显示模式
            CONStatusCode = 4;
            SPDAdjustTimer.once(5, closeSPDCtrl, lastCONStatusCode);
            break;
        }
        break;

    case _HoldContinuous:        // 按住只是闪灯提醒用户,按住弹起才会执行设置
        SPDAdjustTimer.detach(); // 调速界面下,在按住按钮时,禁止长时间无操作自动退出功能
        static long lastTime = millis();
        long nowTime = millis();
        if (nowTime - lastTime >= 200)
        {
            digitalWrite(ledPin, !digitalRead(ledPin)); // 闪烁led灯
            lastTime = millis();
        }
        break;
    }
}

// 长按超时
void closeSPDCtrl(byte lastCONStatusCode)
{
    SPDAdjustTimer.detach();           // 关闭长时间无操作自动退出功能定时器
    CONStatusCode = lastCONStatusCode; // 回复上次状态码
}

// 绘制天气界面
void weatherDraw()
{
    String emptyStr = ""; // 空字符串
    u8g2.clearBuffer();
    if (weatherInfo.statusCode.length() != 0) // 反回了错误码
    {
        u8g2.drawXBMP(0, 0, 50, 50, nan99); // 错误icon
        u8g2.setFont(u8g2_font_tenthinnerguys_tf);
        u8g2.drawUTF8(50 + (128 - 50 - u8g2.getUTF8Width("ERROE")) / 2, 23, "ERROE!"); // 错误标题
        u8g2.setFont(u8g2_font_wqy13_t_gb2312a);
        u8g2.drawUTF8(50 + (128 - 50 - u8g2.getUTF8Width("数据错误:(")) / 2, 40, "数据错误:("); // 错误内容
        u8g2.drawLine(0, 50, 128, 50);                                                          // 底部横线
        u8g2.drawUTF8(0, 63, "错误代码:");                                                      // 底部错误码显示
        u8g2.setFont(u8g2_font_tenthinnerguys_tf);
        u8g2.drawStr(128 - u8g2.getUTF8Width(weatherInfo.statusCode.c_str()), 63, weatherInfo.statusCode.c_str()); // 错误码内容
    }
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
    webRootHandle();   // 获取到浏览器发送的请求后返回首页更新
    WiFi.disconnect(); // 关闭wifi连接,会在loop中重新连接
}

void webXinzhiHandle()
{
    strcpy(config.xinzhiKey, webserver.arg("xinzhiKey").c_str());
    strcpy(config.city, webserver.arg("city").c_str()); // 更新config
    weatherNow.config(config.xinzhiKey, config.city, "c");
    forecast.config(config.xinzhiKey, config.city, "c");
    saveConfig();
    webRootHandle(); // 获取到浏览器发送的请求后返回首页更新
}

void webServerHandle()
{
    client.stop();
    strcpy(config.serverIP, webserver.arg("serverIP").c_str());
    config.serverPort = webserver.arg("serverPort").toInt(); // 更新config
    // 保存到EEPROM
    saveConfig();
    webRootHandle(); // 获取到浏览器发送的请求后返回首页更新
}

// config保存到EEPROME
void saveConfig()
{
    EEPROM.begin(1024);
    uint8_t *p = (uint8_t *)(&config);
    for (int i = 0; i < sizeof(config); i++)
    {
        EEPROM.write(i, *(p + i));
    }
    EEPROM.commit();
}

// 从EEPROM中更新config
void loadConfig()
{
    EEPROM.begin(1024);
    uint8_t *p = (uint8_t *)(&config);
    for (int i = 0; i < sizeof(config); i++)
    {
        *(p + i) = EEPROM.read(i);
    }
    EEPROM.commit();
}