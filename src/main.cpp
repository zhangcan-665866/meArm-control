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
// 爪子：0 = 完全闭合，90 = 完全张开；上电初值停在 60
int dangqianZhua = 60;
// 爪子目标角度
int mubiaoZhua = 60;
// 摇杆中心值
const int yaogan_zhongxin = 512;
// 死区大小(防止轻微触动抖动)
const int siqu = 30;
// 摇杆采样间隔(毫秒)：采样不能太密，否则"松手回弹"两次采样之间只变几个 LSB，跟手推没法区分
const int yaoganJianGe = 20;
// 两次采样之间偏移量允许的最大变化：超过它说明摇杆在快速运动(多半是松手回弹)，本次不采纳
const int bianhuaXianzhi = 60;
// 四个轴的当前偏移量(留成全局，想临时打印时直接 Serial.print 就行)
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
// 推着走的一步：dir = +1 / -1，把目标角推进 1°(不超出 0 ~ shangxian)。
// 关键：只有舵机已经跟到目标角了，才允许再推一步。
// 这样目标角永远贴着实际角度，松手时不会还欠着一截，让机械臂自己继续跑。
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
void chuliChuanKou()
{
    while (Serial.available() > 0)
    {
        char mingling = Serial.read();

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
    gengxinZhua();   // 上电时也把爪子输出一次
    // 开启串口
    Serial.begin(9600);//有可能要调成38400
    Serial.println("当前模式:摇杆模式");
}

void loop()
{
    // 每轮处理串口命令
    chuliChuanKou();
    // 获取当前运行时间
    unsigned long xianzai = millis();

    // 摇杆按固定间隔采样，不能每轮都读：
    // 采样太密时"松手回弹"每次只变化几个 LSB，就没办法和手慢慢推区分开了
    if (moshi == 0 && xianzai - shangciYaoganShijian >= (unsigned long)yaoganJianGe)
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
