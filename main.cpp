#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <Ticker.h>
#include <Arduino.h>
#include <ESP8266HTTPClient.h>
// #include <WiFiClientSecure.h>
#include <ArduinoJson.h>

//引脚定义
#define SDA 4
#define SCL 5
#define KEY 2
#define LED1 16
#define LED2 14
#define LED3 12
#define LED4 13
#define BAT A0

#define CH450_SYSOFF    0x0400					// 关闭显示
#define CH450_SYSON     0x0401					// 开启显示
#define CH450_DIG1      0x1200		            // 数码管位2显示,需另加8位数据
#define CH450_DIG2      0x1300		            // 数码管位3显示,需另加8位数据
#define CH450_DIG3      0x1400		            // 数码管位4显示,需另加8位数据
#define CH450_DIG4      0x1500					// 数码管位5显示,需另加8位数据
#define CH450_DIG5      0x1600					// 数码管位6显示,需另加8位数据
#define CH450_DIG6      0x1700		            // 数码管位7显示,需另加8位数据

#define		CH450_I2C_ADDR1		0x40			// CH450的地址
#define		CH450_I2C_MASK		0x3E			// CH450的高字节命令掩码

static const char URL[] = "ntp.ntsc.ac.cn";     //NTP时间服务器
static const char* URL2 = "http://apis.tianapi.com/ncov/index?key=f72495e1ce4e7d4ee939822c870ad774";     //天气信息API接口，用户key已删除

const int timezone = 8;
unsigned int localPort = 8888;

unsigned int smg[11] = {0x003f,0x0006,0x005b,0x004f,0x0066,0x006d,0x007d,0x0007,0x007f,0x006f,0x0080};  //0 1 2 3 4 5 6 7 8 9 . 
unsigned int smg2[11] = {0x003f,0x0030,0x005b,0x0079,0x0074,0x006d,0x006f,0x0038,0x007f,0x007d,0x0080};  //0 1 2 3 4 5 6 7 8 9 . 

//变量定义
int timenow[4] = {0};
int datanow[5] = {0};
int secondnow = 0;
int secondflag = 0;
int showchange = 0;
int keycount = 0;
int keylongcount = 0;
int keydown = 0;
int keyflag = 0;
int mode = 1;
int battery = 0;
int netcount = 0;
int errorcount = 0;
int doconnect = 0;
int LEDCOUNT = 0x80;
uint8_t second_cnt = 0;
uint8_t minute_cnt = 0;
uint8_t hour_cnt = 0;

int result_desc_suspectedIncr;
int result_desc_curedIncr;
int result_desc_deadIncr;

unsigned char turn = 0;
unsigned char change = 0;
unsigned char over = 0;

WiFiUDP Udp;
WiFiManager wifimanager;

// WiFiClientSecure https;
HTTPClient http;
WiFiClient client;

time_t getNtpTime();
time_t prevDisplay = 0;
void digitalClockDisplay();   //数码管显示
void printDigits(int digits);   //串口调试
void sendNTPpacket(IPAddress &address);   //发送NTP数据包
void printDigitsString(int digits);   //串口调试
void printDigitsmg(int h,int m);    //第一块数码管显示
void printDigitsmg2(int mo,int d,int wd);   //第二块数码管显示
void connect_callback(void);    //网络连接回调函数
void ledshow_callback(void);    //电量显示回调函数
void timegetshow_callback(void);    //时间显示回调函数
void keypross_callback(void);   //按键处理回调函数
void batjarje_callback(void);   //电量检测回调函数
void time_callback(void);   //时间处理回调函数
void get_callback(void);    //获取时间回调函数
void getdata_callback(void);    //获取疫情数据回调函数
void showdata_callback(void);   //疫情数据显示回调函数


/********************多任务初始化***********************************/
Ticker connect(connect_callback,50);
Ticker ledshow(ledshow_callback,500);
Ticker timegetshow(timegetshow_callback,500);
Ticker keypross(keypross_callback,200);
Ticker batjarje(batjarje_callback,500);
Ticker timeprocess(time_callback,1000,0,MILLIS);
Ticker getprocess(get_callback,0,1);
Ticker getdataprocess(getdata_callback,180000,0,MILLIS);
Ticker showdataprocess(showdata_callback, 5000, 0, MILLIS);

/*******************CH450驱动*************************************/
void delayus(int i) {

  while(i--);

}

//IIC驱动

void I2C_Start(void) {

    digitalWrite(SDA,1);
    digitalWrite(SCL,1);
    delayus(10);
    digitalWrite(SDA,0);
    delayus(10);
    digitalWrite(SCL,0);
    delayus(10);
    
}

void I2C_Stop(void) {

    digitalWrite(SDA,0);
    delayus(10);
    digitalWrite(SCL,1);
    delayus(10);
    digitalWrite(SDA,1);
    delayus(10);

}

//CH450写数据

void WriteByte(unsigned char dat) {

    unsigned char i;
    for(i = 0;i < 8;i ++)
    {
        if(dat & 0x80)
        {
            digitalWrite(SDA,1);
        }
        else
        {
            digitalWrite(SDA,0);
        }
        delayus(10);
        digitalWrite(SCL,1);
        dat <<= 1;
        delayus(10);
        digitalWrite(SCL,0);
    }

    digitalWrite(SDA,1);
    delayus(10);
    digitalWrite(SCL,1);
    delayus(10);
    digitalWrite(SCL,0);

}

//CH450写指令

void WriteCmd(unsigned short cmd) {

    I2C_Start();
    WriteByte(((unsigned char)(cmd >> 7) & CH450_I2C_MASK) | CH450_I2C_ADDR1);
    WriteByte((unsigned char)cmd);
    I2C_Stop();

}

/*****************初始化*****************************/
void setup() {

//引脚初始化
  pinMode(SDA,OUTPUT);
  pinMode(SCL,OUTPUT);
  pinMode(LED1,OUTPUT);
  pinMode(LED2,OUTPUT);
  pinMode(LED3,OUTPUT);
  pinMode(LED4,OUTPUT);
  pinMode(KEY,INPUT_PULLUP);
  pinMode(BAT,NUM_ANALOG_INPUTS);

  delay(1);

  digitalWrite(SDA,1);
  digitalWrite(SCL,1);

//串口初始化
  Serial.begin(115200);

//任务启动
  connect.start();
  ledshow.start();
  keypross.start();
  timegetshow.start();
  batjarje.start();
  getdataprocess.start();

  delay(1000);
  Serial.println("10000000");

//OTA在线升级
  ArduinoOTA.setHostname("ESP8266");
  ArduinoOTA.setPassword("1234");
  ArduinoOTA.begin();
  Serial.println("HELLO ESP8266!");

}

/***************主循环***********************************/
void loop() {
//对按键进行响应
  while (mode)
  {
    ledshow.update();
    delay(1000);
    connect.update();
    break;
  }
  
  while (!mode)
  {
    keypross.update();
    timegetshow.update();
    timeprocess.update();
    batjarje.update();
    getdataprocess.update();
    break;
  }

  if(!digitalRead(KEY))
  {
    keydown = 1;
  }
  else
  {
    keydown = 0;
  }  

}

/*****************数码管显示************************/
void digitalClockDisplay()
{
//串口打印，用于调试
  printDigits(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(year());
  Serial.print(".");
  Serial.print(month());
  Serial.print(".");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(weekday());
  Serial.println();

//先获取一次数据
if(!over)
  {
    if(WiFi.status() == WL_CONNECTED)
    {
      getdata_callback();
      over = 1;
    }
  }

//时间日期切换
  if(showchange)
  {
    printDigitsmg2(month(),day(),weekday());
    WriteCmd(CH450_SYSOFF);
    WriteCmd(CH450_DIG1 | smg[datanow[0]]);
    WriteCmd(CH450_DIG2 | smg[datanow[1]] | smg[10]);
    WriteCmd(CH450_DIG3 | smg[datanow[2]]);
    WriteCmd(CH450_DIG4 | smg2[datanow[3]]);
    WriteCmd(CH450_DIG5);
    WriteCmd(CH450_DIG6 | smg2[datanow[4]]);
  }
  else
  {
    if(secondnow != second())
    {
      secondflag = ~secondflag;
      if(turn >= 2)
      {
        turn = 0;
      }
      else
      {
        turn ++;
      }
      if(change <= 5)
      {
        change ++;
      }
      else
      {
        change = 0;
      }
    }
    secondnow = second();

    printDigitsmg(hour(),minute());
    if(change == 0)
    {
      WriteCmd(CH450_SYSOFF);
      WriteCmd(CH450_DIG1);
      WriteCmd(CH450_DIG2 | smg[timenow[0]]);
      WriteCmd(CH450_DIG3 | smg[timenow[1]]);
      WriteCmd(CH450_DIG4 | smg2[timenow[2]]);
      WriteCmd(CH450_DIG5 | smg2[timenow[3]]);
      WriteCmd(CH450_DIG6);
      if(secondflag)
      {
        WriteCmd(CH450_DIG3 | smg[timenow[1]]);
        WriteCmd(CH450_DIG4 | smg2[timenow[2]]);
      }
      else
      {
        WriteCmd(CH450_DIG3 | (smg[10] | smg[timenow[1]]) );
        WriteCmd(CH450_DIG4 | (smg2[10] | smg2[timenow[2]]) );
      }
    }
    //疫情信息显示切换
    else if(change == 4)
    {
      switch(turn)
      {
        case 0:
        {
            WriteCmd(CH450_SYSOFF);
            WriteCmd(CH450_DIG4 | smg2[(result_desc_suspectedIncr % 1000 / 100)]);
            WriteCmd(CH450_DIG5 | smg2[(result_desc_suspectedIncr % 100 / 10)]);
            WriteCmd(CH450_DIG6 | smg2[(result_desc_suspectedIncr % 10)]);
            WriteCmd(CH450_DIG1 | 0x5e);
            WriteCmd(CH450_DIG2);
            WriteCmd(CH450_DIG3 | smg[(result_desc_suspectedIncr / 1000)]);
            WriteCmd(CH450_SYSON);
            break;
        }
        case 1:
        {
            WriteCmd(CH450_SYSOFF);
            WriteCmd(CH450_DIG4 | smg2[(result_desc_curedIncr % 1000 / 100)]);
            WriteCmd(CH450_DIG5 | smg2[(result_desc_curedIncr % 100 / 10)]);
            WriteCmd(CH450_DIG6 | smg2[(result_desc_curedIncr % 10)]);
            WriteCmd(CH450_DIG1 | 0x39);
            WriteCmd(CH450_DIG2);
            WriteCmd(CH450_DIG3 | smg[(result_desc_curedIncr / 1000)]);
            WriteCmd(CH450_SYSON);
            break;
        }
        case 2:
        {
            WriteCmd(CH450_SYSOFF);
            WriteCmd(CH450_DIG4 | smg2[(result_desc_deadIncr % 1000 / 100)]);
            WriteCmd(CH450_DIG5 | smg2[(result_desc_deadIncr % 100 / 10)]);
            WriteCmd(CH450_DIG6 | smg2[(result_desc_deadIncr % 10)]);
            WriteCmd(CH450_DIG1 | 0x79);
            WriteCmd(CH450_DIG2);
            WriteCmd(CH450_DIG3 | smg[(result_desc_deadIncr / 1000)]);
            WriteCmd(CH450_SYSON);
            break;
        }
        default :
          break;
      }
    }

  }

  if(hour() >= 22 && hour() <= 6)
    WriteCmd(CH450_SYSOFF);
  else
    WriteCmd(CH450_SYSON);
}

//串口打印，调试
void printDigitsString(int digits)
{

  int dig;
  dig = (int)digits;
  Serial.println(dig);
  
}

//数据处理
void printDigitsmg(int h,int m)
{
  int hh,mm;
  hh = (int)h;
  mm = (int)m;
  timenow[0] = hh / 10;
  timenow[1] = hh % 10;
  timenow[2] = mm / 10;
  timenow[3] = mm % 10;

}

//数据处理
void printDigitsmg2(int mo,int d,int wd)
{
  int momo,dd,wdwd;
  momo = (int)mo;
  dd = (int)d;
  wdwd = (int)wd;
  datanow[0] = momo / 10;
  datanow[1] = momo % 10;
  datanow[2] = dd / 10;
  datanow[3] = dd % 10;
  if(wdwd == 1)
  {
    datanow[4] = 7;
  }
  else 
  {
    datanow[4] = --wdwd;
  }

}

//串口打印，调试
void printDigits(int digits)
{
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

/*********************获取NTP数据********************************************/
const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(URL, ntpServerIP);
  Serial.print(URL);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timezone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");

  //网络中断的处理
  if(errorcount < 5)
  {
    errorcount ++;
    ESP.reset();
    // connect.update();
  }
  else
  {
    WriteCmd(CH450_SYSOFF);
    WriteCmd(CH450_DIG1 | 0x0079);
    WriteCmd(CH450_DIG2 | 0x0050);
    WriteCmd(CH450_DIG3 | 0x0050);
    WriteCmd(CH450_DIG4 | 0x0063);
    WriteCmd(CH450_DIG5 | 0x0042);
    WriteCmd(CH450_SYSON);
  }
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

/*******************各种回调函数*****************************/
//联网
void connect_callback(void)
{
  wifimanager.autoConnect("ESP8266");
  delay(2000);
  Udp.begin(localPort);
  setSyncProvider(getNtpTime);
  setSyncInterval(300);
  mode = 0; 
  Serial.println("HELLO ESP8266!");
}

//点灯
void ledshow_callback(void)
{
  int i = 0;
  while (mode)
  {
    delay(100);

    LEDCOUNT >>= i;
    switch (LEDCOUNT)
    {
    case 0x80:
      digitalWrite(LED1,1);
      digitalWrite(LED2,0);
      digitalWrite(LED3,0);
      digitalWrite(LED4,0);
      break;
    case 0x40:
      digitalWrite(LED1,0);
      digitalWrite(LED2,1);
      digitalWrite(LED3,0);
      digitalWrite(LED4,0);
      break;
    case 0x20:
      digitalWrite(LED1,0);
      digitalWrite(LED2,0);
      digitalWrite(LED3,1);
      digitalWrite(LED4,0);
      break;
    case 0x01:
      digitalWrite(LED1,0);
      digitalWrite(LED2,0);
      digitalWrite(LED3,0);
      digitalWrite(LED4,1);
      break;
    }
    if(i < 5)
    {
      i++;
    }
    else
    {
      i = 0;
      LEDCOUNT = 0x80;
    }
    break;
  }
}

//显示
void timegetshow_callback(void)
{
  if(!mode)
  {
    digitalClockDisplay();
  }
  else
  {
    secondflag = ~secondflag;
    WriteCmd(CH450_SYSOFF);
    if(secondflag)
    {
      WriteCmd(CH450_DIG1 | smg[8] | smg[10]);
      WriteCmd(CH450_DIG2 | smg[8] | smg[10]);
      WriteCmd(CH450_DIG3 | smg[8] | smg[10]);
      WriteCmd(CH450_DIG4 | smg2[8] | smg2[10]);
      WriteCmd(CH450_DIG5 | smg2[8] | smg2[10]);
      WriteCmd(CH450_DIG6 | smg2[8] | smg2[10]);
      WriteCmd(CH450_SYSON);
    }
    doconnect = 1;
    connect.resume();
  }
}

//按键
void keypross_callback(void)
{
  if(!digitalRead(KEY))
  {
    keylongcount ++;
    keyflag = 1;
  }
  if(keylongcount >= 0 && keylongcount < 20)
  {
    if(digitalRead(KEY) && keyflag)
    {
      keylongcount = 0;
      keyflag = 0;
      showchange = 1;
    }
  }
  else
  {
    if(!digitalRead(KEY))
    {
      keylongcount = 0;
      keyflag = 0;
      mode = 1;
      if(mode)
      {
        wifimanager.resetSettings();
        ledshow.resume();
        connect.resume();
      }
      else
      {
        ledshow.stop();
        connect.stop();
      }
    }
  }

  if(showchange)
  {
    keycount ++;
  }
  if(keycount > 15)
  {
    showchange = 0;
    keycount = 0;
  }
}

//ADC采集
void batjarje_callback(void)
{
  battery = analogRead(BAT);
  if(battery >= 735)
  {
    digitalWrite(LED1,1);
    digitalWrite(LED2,1);
    digitalWrite(LED3,1);
    digitalWrite(LED4,1);
  }
  else if(690 < battery && battery <= 735)
  {
    digitalWrite(LED2,1);
    digitalWrite(LED3,1);
    digitalWrite(LED4,1);
    digitalWrite(LED1,0);
  }
  else if(640 < battery && battery <= 690)
  {
    digitalWrite(LED3,1);
    digitalWrite(LED4,1);
    digitalWrite(LED1,0);
    digitalWrite(LED2,0);
  }
  else if(620 < battery && battery <= 640)
  {
    digitalWrite(LED4,1);
    digitalWrite(LED1,0);
    digitalWrite(LED2,0);
    digitalWrite(LED3,0);
  }
  else
  {
    if(secondflag)
    {
      digitalWrite(LED4,1);
      digitalWrite(LED1,0);
      digitalWrite(LED2,0);
      digitalWrite(LED3,0);
    }
    else
    {
      digitalWrite(LED1,0);
      digitalWrite(LED2,0);
      digitalWrite(LED3,0);
      digitalWrite(LED4,0);
    }
  }
}

//时间计算，通过每隔一段时间去开启WIFI获取时间和疫情信息，达到低功耗目的
//实测WIFI保持连接状态工作电流110mA，低功耗处理后工作电流8mA，峰值电流110mA，持续5s
void time_callback(void)
{
  if(second_cnt >= 59)
  {
    second_cnt = 0;
    minute_cnt ++; 
  }
  else
    second_cnt += 1; 

  if(minute_cnt >= 59)
  {
    minute_cnt = 0;
    hour_cnt += 1;
    getprocess.resume();
    getprocess.update();
  }

  if(hour_cnt >= 23)
    hour_cnt = 0;
}

//时间处理
void get_callback(void)
{
  Serial.println("get time");
  second_cnt = second();
  minute_cnt = minute();
  hour_cnt = hour();
  getprocess.stop();
  connect.stop();
  Serial.println("get time stop");
}

//解析获取的json数据
void getdata_callback(void)
{
  Serial.println("start client");
  http.begin(client, URL2);
  Serial.println("cliented");
  if(WiFi.status() == WL_CONNECTED)
  {
    int code = http.GET();
    Serial.println(code);
      if(code == HTTP_CODE_OK)
      {
        Serial.println(URL2);
        String responsePayload = http.getString();
        DynamicJsonDocument doc(6144);
        DeserializationError error = deserializeJson(doc, responsePayload);

        if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.f_str());
            return;
        }
        JsonObject result = doc["result"];
        JsonObject result_desc = result["desc"];
        result_desc_suspectedIncr = result_desc["suspectedIncr"]; // 40
        result_desc_curedIncr = result_desc["curedIncr"]; // 726
        result_desc_deadIncr = result_desc["deadIncr"]; // 50

        Serial.println(result_desc_suspectedIncr);
        Serial.println(result_desc_curedIncr);
        Serial.println(result_desc_deadIncr);
      }
  http.end();
  }
}

//数据处理，便与显示
void showdata_callback(void)
{
  if(turn >= 2)
  {
    turn = 0;
  }
  else
  {
    turn ++;
  }
  switch(turn)
  {
    case 0:
    {
        WriteCmd(CH450_SYSOFF);
        WriteCmd(CH450_DIG1 | smg[(result_desc_suspectedIncr / 10)]);
        WriteCmd(CH450_DIG2 | smg[(result_desc_suspectedIncr % 10)]);
        WriteCmd(CH450_SYSON);
        break;
    }
    case 1:
    {
        WriteCmd(CH450_SYSOFF);
        WriteCmd(CH450_DIG1 | smg[(result_desc_curedIncr / 100)]);
        Serial.print(result_desc_curedIncr / 100);
        WriteCmd(CH450_DIG2 | smg[(result_desc_curedIncr / 10 % 10)]);
        Serial.print(result_desc_curedIncr / 10 % 10);
        WriteCmd(CH450_DIG3 | smg[result_desc_curedIncr % 100]);
        Serial.println(result_desc_curedIncr % 100);
        Serial.println(result_desc_curedIncr);
        WriteCmd(CH450_SYSON);
        break;
    }
    case 2:
    {
        WriteCmd(CH450_SYSOFF);
        WriteCmd(CH450_DIG1 | smg[(result_desc_deadIncr / 10)]);
        WriteCmd(CH450_DIG2 | smg[(result_desc_deadIncr % 10)]);
        WriteCmd(CH450_SYSON);
        break;
    }
    default :
      break;
  }
  
}
