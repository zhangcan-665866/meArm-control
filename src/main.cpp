#include <Arduino.h>
#include <Servo.h>
//暂设舵机引脚，拿到板子的时候再改
const int SERVO_X_PIN = 9;
const int SERVO_Y_PIN = 8;
const int SERVO_Z_PIN = 7;
const int CLAW_PIN = 6;
//摇杆引脚
const int yaogan_X_PIN = A0;
const int yaogan_Y_PIN = A1;
const int yaogan_Z_PIN = A2;
const int yaogan_Zhua_PIN = A3;
//创建四个舵机对象
Servo servoX;
Servo servoY;
Servo servoZ;
Servo claw;
// 爪子：0 = 完全闭合，90 = 完全张开（和 X/Y/Z 一样直接用数字，90 就是上电初值）
// 设置初始角度为90°
// X/Y/Z当前角度
int dangqianJiaoduX = 90;
int dangqianJiaoduY = 90;
int dangqianJiaoduZ = 90;
// X/Y/Z目标角度
int mubiaoJiaoduX = 90;
int mubiaoJiaoduY = 90;
int mubiaoJiaoduZ = 90;
// 爪子当前角度(上电默认完全张开)
int dangqianZhua = 60;   // 上电停在 60（=舵机120°）
// 爪子目标角度
int mubiaoZhua = 60;     // 必须和上面成对
// 摇杆中心值
const int yaogan_zhongxin = 512;
// 死区大小(防止轻微触动抖动)
const int siqu = 30;
// 摇杆采样间隔(毫秒)：采样不能太密，否则"松手回弹"两次采样之间只变几个LSB，跟手推没法区分
const int yaoganJianGe = 20;
// 两次采样之间偏移量允许的最大变化：超过它说明摇杆在快速运动(多半是松手回弹)，本次不采纳
const int bianhuaXianzhi = 60;
// 四个轴的当前偏移量
int pianyiX = 0;
int pianyiY = 0;
int pianyiZ = 0;
int pianyiZhua = 0;
// 上一次采到的偏移量(松手检测用)
int shangciPianyiX = 0;
int shangciPianyiY = 0;
int shangciPianyiZ = 0;
int shangciPianyiZhua = 0;
// 上一次读取摇杆的时间
unsigned long shangciYaoganShijian = 0;
// ===== 任务三：录制 / 播放 / 回中 =====
// 录制：每 200ms 存一次四个轴的角度，最多 75 点 = 15 秒（占 300 字节内存）
// 注意：UNO 只有 2048 字节内存，这个缓冲区别开太大，否则程序会崩
const int LUZHI_JIANGE = 200;      // 采样间隔(毫秒)
const int LUZHI_ZUIDA  = 75;      // 最大点数
byte luzhiBuf[LUZHI_ZUIDA][4];     // [第几个点][0=X 1=Y 2=Z 3=爪子]
int  luzhiChang = 0;               // 已录点数
bool luzhiZhong = false;           // 正在录制
unsigned long luzhiShijian = 0;    // 上次采样时间
bool bofangZhong = false;          // 正在播放
int  bofangSuoyin = 0;             // 播到第几个点
unsigned long bofangShijian = 0;
// 上电位置（"回中"就回到这里；以后改初始角度，它会自动跟着变）
int huiZhongX = 90, huiZhongY = 90, huiZhongZ = 90, huiZhongZhua = 60;
// 延迟时间范围
const int minYanshi = 5;
const int maxYanshi = 50;
// 机械臂延迟时间(速度)
int yanshi = 35;
// 上一次更新舵机的时间
unsigned long shangciGengxinShijian = 0;
// 控制模式
// 0 = 摇杆模式
// 1 = 串口模式
int moshi = 0;
// 当前舵机
char dangqianDuoji = 0;
// 当前正在读取的角度
int shuruJiaodu = 0;
// 判断当前是否已经读取到数字
bool youShuzi = false;
unsigned long shangciTime = 0;//计时输出位置任务二
//机械臂位置点
struct Point
{
    int x;
    int y;
    int z;
    int claw;
};//根据实物修改
// A靠近物体
Point A_qu = {90,90,90,90};
// A夹取位置
Point A_jia = {90,90,60,90};
// A夹住后抬起
Point A_tai = {90,90,100,0};
// A放置位置
Point A_fang = {140,80,100,0};
// A释放位置
Point A_song = {140,80,60,90};
//B
Point B_qu = {90,90,90,90};
Point B_jia = {90,90,60,90};
Point B_tai = {90,90,100,0};
Point B_fang = {120,80,100,0};
Point B_song = {120,80,60,90};
//C
Point C_qu = {90,90,90,90};
Point C_jia = {90,90,60,90};
Point C_tai = {90,90,100,0};
Point C_fang = {160,80,100,0};
Point C_song = {160,80,60,90};
void moveTo(Point p)
{
    mubiaoJiaoduX = p.x;
    mubiaoJiaoduY = p.y;
    mubiaoJiaoduZ = p.z;
    mubiaoZhua = constrain(p.claw, 0, 90);
}
bool daoDa(){
    if(
        abs(dangqianJiaoduX - mubiaoJiaoduX) <= 1 &&
        abs(dangqianJiaoduY - mubiaoJiaoduY) <= 1 &&
        abs(dangqianJiaoduZ - mubiaoJiaoduZ) <= 1 &&
        abs(dangqianZhua - mubiaoZhua) <= 1
    )
    {
        return true;
    }
    return false;
}
//声明
void gengxinDuoji();
void gengxinZhua();
void dengDaiDaoDa()
{
    while(!daoDa())
    {
        gengxinDuoji();
        gengxinZhua();
        delay(20);
    }
}
void zhixingDongzuo(
Point qu,
Point jia,
Point tai,
Point fang,
Point song
)
{
   //1、到物体上方
    moveTo(qu);
    dengDaiDaoDa();
    //2、下降
    moveTo(jia);
    dengDaiDaoDa();
    //3、夹紧
    mubiaoZhua = 0;
    dengDaiDaoDa();
    //4、抬起
    moveTo(tai);
    dengDaiDaoDa();
    //5、移动到放置位置
    moveTo(fang);
    dengDaiDaoDa();
    //6、下降释放
    moveTo(song);
    dengDaiDaoDa();
    //7、松开
    mubiaoZhua = 90;
    dengDaiDaoDa();
    //8、回到初始附近
    moveTo(qu);
}
void tuiJinYiBu(int &mubiao, int dangqian, int dir, int shangxian)
{
    if (mubiao != dangqian)
    {
        return;
    }
    mubiao += dir;
    mubiao = constrain(mubiao, 0, shangxian);
}
void gengxinYaogan()
{
    // 读取四个轴，减去中心值得到偏移量(-512 ~ +511)
    pianyiX    = analogRead(yaogan_X_PIN)    - yaogan_zhongxin;
    pianyiY    = analogRead(yaogan_Y_PIN)    - yaogan_zhongxin;
    pianyiZ    = analogRead(yaogan_Z_PIN)    - yaogan_zhongxin;
    pianyiZhua = analogRead(yaogan_Zhua_PIN) - yaogan_zhongxin;

    // 【校准用】打开下面四行的注释，烧录后开串口监视器，手别碰摇杆，
    // 记下四个轴稳定的偏移量；若偏离 0 超过死区，请据实测值修改第 32 行 yaogan_zhongxin
    // Serial.print(pianyiX);    Serial.print(" ");
    // Serial.print(pianyiY);    Serial.print(" ");
     //Serial.print(pianyiZ);    Serial.print(" ");
     //Serial.println(pianyiZhua);

    // 【推着走】杆偏离中心就慢慢走，回中就不动 —— 不看摇杆位置，只看"推没推"。
    // 每拍只推进 1°，而且必须等舵机先跟上，所以目标角贴着实际角度，
    // 松手立刻停在原地，绝不会一路冲到端点。
    // 那个"变化量"条件是滤掉松手瞬间弹簧回弹的瞬态，免得被回弹蹭着走一两度。
    if (abs(pianyiX) > siqu && abs(pianyiX - shangciPianyiX) <= bianhuaXianzhi)
    {
        tuiJinYiBu(mubiaoJiaoduX, dangqianJiaoduX, (pianyiX > 0) ? 1 : -1, 180);
    }
    if (abs(pianyiY) > siqu && abs(pianyiY - shangciPianyiY) <= bianhuaXianzhi)
    {
        tuiJinYiBu(mubiaoJiaoduY, dangqianJiaoduY, (pianyiY > 0) ? 1 : -1, 180);
    }
    if (abs(pianyiZ) > siqu && abs(pianyiZ - shangciPianyiZ) <= bianhuaXianzhi)
    {
        tuiJinYiBu(mubiaoJiaoduZ, dangqianJiaoduZ, (pianyiZ > 0) ? 1 : -1, 180);
    }
    // 爪子 0°=夹紧, 90°=松开；同样推着走、松手停住
    if (abs(pianyiZhua) > siqu && abs(pianyiZhua - shangciPianyiZhua) <= bianhuaXianzhi)
    {
        tuiJinYiBu(mubiaoZhua, dangqianZhua, (pianyiZhua > 0) ? 1 : -1, 90);
    }

    // 记录本次读数，供下一次采样判断变化量
    shangciPianyiX = pianyiX;
    shangciPianyiY = pianyiY;
    shangciPianyiZ = pianyiZ;
    shangciPianyiZhua = pianyiZhua;
}
void xianshiJiaodu()
{
    if(millis()-shangciTime >= 500)
    {
    shangciTime = millis();
    
    Serial.print("X:");
    Serial.print(dangqianJiaoduX);

    Serial.print(" Y:");
    Serial.print(dangqianJiaoduY);

    Serial.print(" Z:");
    Serial.print(dangqianJiaoduZ);

    Serial.print(" Claw:");
    Serial.println(dangqianZhua);
}
}
//保存爪子角度
void baocunJiaodu()
{
    // 如果没有读取到数字，就不处理
    if (!youShuzi)
    {
        return;
    }
    // 根据当前舵机保存角度
    switch (dangqianDuoji)
{
    case 'x':
        mubiaoJiaoduX = constrain(shuruJiaodu, 0, 180);
        Serial.print("X目标角度 = ");
        Serial.println(mubiaoJiaoduX);
        break;
    case 'y':
        mubiaoJiaoduY = constrain(shuruJiaodu, 0, 180);
        Serial.print("Y目标角度 = ");
        Serial.println(mubiaoJiaoduY);
        break;
    case 'z':
       mubiaoJiaoduZ = constrain(shuruJiaodu, 0, 180);
       Serial.print("Z目标角度 = ");
       Serial.println(mubiaoJiaoduZ);
        break;
    // 下一轮
}
    shuruJiaodu = 0;
    youShuzi = false;

}
// 从"遥控板"和"电脑串口"两路取一个字符（遥控板优先），都没有返回 -1
int duZiFu()
{
    if (Serial.available() > 0)
    {
        return Serial.read();
    }
    return -1;
}
void chuliChuanKou()
{
    int duDao = duZiFu();
    while (duDao >= 0)
    {
        char mingling = (char)duDao;

        // O：爪子打开
        if (mingling == 'O')
        {
            mubiaoZhua = 90;
            Serial.println("爪子：打开");
        }
        
        // S：爪子关闭
        else if (mingling == 'S')
        {
            mubiaoZhua = 0;
            Serial.println("爪子：关闭");
        }
         // H：提高速度
        else if (mingling == 'H')
        {
            yanshi -= 5;
            yanshi = constrain(
                yanshi,
                minYanshi,
                maxYanshi
            ); Serial.print("当前延迟 = ");
               Serial.println(yanshi);
        }
        // L：降低速度
        else if (mingling == 'L')
        {
            yanshi += 5;
            yanshi = constrain(
                yanshi,
                minYanshi,
                maxYanshi
            );
                Serial.print("当前延迟 = ");
                Serial.println(yanshi);
    }   // M：切换控制模式
    else if (mingling == 'M')
    {    moshi = !moshi;
     if (moshi == 0)
    {
        Serial.println("当前模式：摇杆模式");
    }
         else
    {
        Serial.println("当前模式：串口模式");
    }
}
    // ===== 任务三：遥控板按键 =====
    // R：按键2 —— 录制 开/关
    else if (mingling == 'R')
    {
        if (!luzhiZhong)
        {
            bofangZhong = false;      // 正在播放就先停掉
            moshi = 0;                // 录制必须在摇杆模式
            luzhiChang = 0;
            luzhiZhong = true;
            luzhiShijian = millis();
            Serial.println("开始录制：用摇杆操控机械臂，再按一次 R 结束");
        }
        else
        {
            luzhiZhong = false;
            Serial.print("录制结束：");
            Serial.print(luzhiChang);
            Serial.print(" 个点，约 ");
            Serial.print(luzhiChang * LUZHI_JIANGE / 1000);
            Serial.println(" 秒");
            if (luzhiChang * LUZHI_JIANGE < 10000)
            {
                Serial.println("注意：不足 10 秒，考试要求大于 10 秒，建议重录");
            }
        }
    }
    // P：按键3 —— 播放上一次录制
    else if (mingling == 'P')
    {
        if (luzhiChang == 0)
        {
            Serial.println("还没录过，先按 R 录制");
        }
        else if (luzhiZhong)
        {
            Serial.println("正在录制，先按 R 结束");
        }
        else
        {
            bofangZhong = true;
            bofangSuoyin = 0;
            bofangShijian = millis() - LUZHI_JIANGE;   // 立刻输出第一个点
            Serial.print("开始播放：共 ");
            Serial.print(luzhiChang);
            Serial.println(" 个点");
        }
    }
    // G：按键4 —— 回中(回到上电位置)
    else if (mingling == 'G')
    {
        luzhiZhong = false;
        bofangZhong = false;
        mubiaoJiaoduX = huiZhongX;
        mubiaoJiaoduY = huiZhongY;
        mubiaoJiaoduZ = huiZhongZ;
        mubiaoZhua    = huiZhongZhua;
        Serial.println("回中");
    }
    // x. y. z
    else if (mingling == 'x' ||mingling == 'y' ||mingling == 'z')
     {
            dangqianDuoji = mingling;
            shuruJiaodu = 0;
            youShuzi = false;
        }
        // 数字
        else if (mingling >= '0' && mingling <= '9')
        {
            shuruJiaodu =shuruJiaodu * 10 +(mingling - '0');//字符运算
            youShuzi = true;
        }
        // 逗号
        else if (mingling == ',')
        {
            baocunJiaodu();
        }
         // 回车 / 换行
        else if (mingling == '\r' ||mingling == '\n')
        {
            baocunJiaodu();
            dangqianDuoji = 0;
        }
        else if(mingling=='A')
{
    Serial.println("执行A搬运");

    zhixingDongzuo(
    A_qu,
    A_jia,
    A_tai,
    A_fang,
    A_song
    );
}
else if(mingling=='B')
{
    Serial.println("执行B搬运");

    zhixingDongzuo(
    B_qu,
    B_jia,
    B_tai,
    B_fang,
    B_song
    );
}
else if(mingling=='C')
{
    Serial.println("执行C搬运");

    zhixingDongzuo(
    C_qu,
    C_jia,
    C_tai,
    C_fang,
    C_song
    );

    duDao = duZiFu();   // 取下一个字符
}
}
}
// 更新XYZ舵机
void gengxinDuoji()
{
    // X轴
    if (dangqianJiaoduX < mubiaoJiaoduX)
    {
        dangqianJiaoduX++;
    }
    else if (dangqianJiaoduX > mubiaoJiaoduX)
    {
        dangqianJiaoduX--;
    }
    // Y轴
    if (dangqianJiaoduY < mubiaoJiaoduY)
    {
        dangqianJiaoduY++;
    }
    else if (dangqianJiaoduY > mubiaoJiaoduY)
    {
        dangqianJiaoduY--;
    }
    // Z轴
    if (dangqianJiaoduZ < mubiaoJiaoduZ)
    {
        dangqianJiaoduZ++;
    }
    else if (dangqianJiaoduZ > mubiaoJiaoduZ)
    {
        dangqianJiaoduZ--;
    }
    // 输出到舵机
    servoX.write(dangqianJiaoduX);
    servoY.write(dangqianJiaoduY);
    servoZ.write(dangqianJiaoduZ);
}
//更新爪子函数
void gengxinZhua()
{
    if (dangqianZhua < mubiaoZhua)
    {
        dangqianZhua++;
    }
    else if (dangqianZhua > mubiaoZhua)
    {
        dangqianZhua--;
    }

    // 0 -> 180°(完全闭合)，90 -> 90°(完全张开)
    claw.write(180 - dangqianZhua);
}
// ===== 任务三：录制采样 / 播放回放（跑在 loop 里，不阻塞）=====
void chuliLuzhiBofang()
{
    unsigned long xianzai = millis();

    // 录制：每 LUZHI_JIANGE 毫秒存一个点
    if (luzhiZhong && xianzai - luzhiShijian >= (unsigned long)LUZHI_JIANGE)
    {
        luzhiShijian = xianzai;
        if (luzhiChang < LUZHI_ZUIDA)
        {
            luzhiBuf[luzhiChang][0] = (byte)dangqianJiaoduX;
            luzhiBuf[luzhiChang][1] = (byte)dangqianJiaoduY;
            luzhiBuf[luzhiChang][2] = (byte)dangqianJiaoduZ;
            luzhiBuf[luzhiChang][3] = (byte)dangqianZhua;
            luzhiChang++;
        }
        else
        {
            luzhiZhong = false;
            Serial.println("录制到上限(15秒)，已自动结束");
        }
    }

    // 播放：按同样节奏，把录到的角度送回目标角
    if (bofangZhong && xianzai - bofangShijian >= (unsigned long)LUZHI_JIANGE)
    {
        bofangShijian = xianzai;
        if (bofangSuoyin < luzhiChang)
        {
            mubiaoJiaoduX = luzhiBuf[bofangSuoyin][0];
            mubiaoJiaoduY = luzhiBuf[bofangSuoyin][1];
            mubiaoJiaoduZ = luzhiBuf[bofangSuoyin][2];
            mubiaoZhua    = luzhiBuf[bofangSuoyin][3];
            bofangSuoyin++;
        }
        else
        {
            bofangZhong = false;
            Serial.println("播放结束");
        }
    }
}
void setup()//执行一次
{
    // 舵机和引脚绑定
    servoX.attach(SERVO_X_PIN);
    servoY.attach(SERVO_Y_PIN);
    servoZ.attach(SERVO_Z_PIN);
    claw.attach(CLAW_PIN);
    //开始执行角度
    servoX.write(dangqianJiaoduX);
    servoY.write(dangqianJiaoduY);
    servoZ.write(dangqianJiaoduZ);
    // 记下上电时的位置（任务三"回中"就回到这里）
    huiZhongX = dangqianJiaoduX;
    huiZhongY = dangqianJiaoduY;
    huiZhongZ = dangqianJiaoduZ;
    huiZhongZhua = dangqianZhua;
    gengxinZhua();   // 上电时也把爪子输出一次
    // 开启串口
    Serial.begin(9600);//有可能要调成38400
    Serial.println("当前模式:摇杆模式");
    Serial.println("按键: R=录制  P=播放  G=回中");
}

void loop()
{
    // 每轮处理串口命令
    chuliChuanKou();
    // 任务三：录制采样 / 播放回放
    chuliLuzhiBofang();
    // 获取当前运行时间
    unsigned long xianzai = millis();

    // 摇杆按固定间隔采样，不能每轮都读：
    // 采样太密时"松手回弹"每次只变化几个 LSB，就没办法和手慢慢推区分开了
    // 播放期间不读摇杆，免得两边同时改目标角打架
    if (moshi == 0 && !bofangZhong && xianzai - shangciYaoganShijian >= (unsigned long)yaoganJianGe)
    {
        shangciYaoganShijian = xianzai;
        gengxinYaogan();
    }
    // 只有达到设定时间间隔，才更新舵机
    if (xianzai - shangciGengxinShijian >= (unsigned long)yanshi)
    {
        // 记录本次更新时间
        shangciGengxinShijian = xianzai;
        // 每经过yanshi毫秒移动1度
        gengxinDuoji();
        gengxinZhua();
        xianshiJiaodu();
    }
}
