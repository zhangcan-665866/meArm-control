#include <Arduino.h>
#include <Servo.h>
//暂设舵机引脚，拿到板子的时候再改
const int SERVO_X_PIN = 9;
const int SERVO_Y_PIN = 10;
const int SERVO_Z_PIN = 11;
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
// 设置初始角度为90°
// X/Y/Z当前角度
int dangqianJiaoduX = 90;
int dangqianJiaoduY = 90;
int dangqianJiaoduZ = 90;
// X/Y/Z目标角度
int mubiaoJiaoduX = 90;
int mubiaoJiaoduY = 90;
int mubiaoJiaoduZ = 90;
// 爪子当前角度
int dangqianZhua = 0;
// 爪子目标角度
int mubiaoZhua = 0;
// 摇杆中心值
const int yaogan_zhongxin = 512;
// 死区大小(防止轻微触动抖动)
const int siqu = 30;
// 延迟时间范围
const int minYanshi = 5;
const int maxYanshi = 50;
// 机械臂延迟时间(速度)
int yanshi = 20;
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
void gengxinYaogan()
{
    // 读取两个摇杆的四个轴
    int yaoganX = analogRead(yaogan_X_PIN);
    int yaoganY = analogRead(yaogan_Y_PIN);
    int yaoganZ = analogRead(yaogan_Z_PIN);
    int yaoganZhua = analogRead(yaogan_Zhua_PIN);

    // 计算四个轴相对于摇杆中心的偏移量
    int pianyiX = yaoganX - yaogan_zhongxin;
    int pianyiY = yaoganY - yaogan_zhongxin;
    int pianyiZ = yaoganZ - yaogan_zhongxin;
    int pianyiZhua = yaoganZhua - yaogan_zhongxin;

    // X轴控制底座舵机
    if (abs(pianyiX) > siqu)
    {
        // 摇杆偏移越大，单次移动步数越多
        int bushuX = map(abs(pianyiX), siqu, 511, 1, 3);
        bushuX = constrain(bushuX, 1, 3);
        if (pianyiX > 0)
        {
            mubiaoJiaoduX += bushuX;
        }
        else
        {
            mubiaoJiaoduX -= bushuX;
        }
    }

    // Y轴控制左边舵机
    if (abs(pianyiY) > siqu)
    {
        int bushuY = map(abs(pianyiY), siqu, 511, 1, 3);
        bushuY = constrain(bushuY, 1, 3);

        if (pianyiY > 0)
        {
            mubiaoJiaoduY += bushuY;
        }
        else
        {
            mubiaoJiaoduY -= bushuY;
        }
    }
    // 控制z轴
    if (abs(pianyiZ) > siqu)
    {
        int bushuZ = map(abs(pianyiZ), siqu, 511, 1, 3);
        bushuZ = constrain(bushuZ, 1, 3);
        if (pianyiZ > 0)
        {
            mubiaoJiaoduZ += bushuZ;
        }
        else
        {
            mubiaoJiaoduZ -= bushuZ;
        }
    }
    // 控制爪子
    if (abs(pianyiZhua) > siqu)
    {
        int bushuZhua = map(abs(pianyiZhua), siqu, 511, 1, 3);
        bushuZhua = constrain(bushuZhua, 1, 3);
        if (pianyiZhua > 0)
        {
            mubiaoZhua += bushuZhua;
        }
        else
        {
            mubiaoZhua -= bushuZhua;
        }
    }
    // 限制四个目标角度
    mubiaoJiaoduX = constrain(mubiaoJiaoduX, 0, 180);
    mubiaoJiaoduY = constrain(mubiaoJiaoduY, 0, 180);
    mubiaoJiaoduZ = constrain(mubiaoJiaoduZ, 0, 180);
    // 爪子暂按0到90度，机械臂到货后再校准
    mubiaoZhua = constrain(mubiaoZhua, 0, 90);
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

    claw.write(dangqianZhua);
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
   claw.write(dangqianZhua);
    // 开启串口
    Serial.begin(9600);//有可能要调成38400
    Serial.println("当前模式:摇杆模式");
}

void loop()
{
    // 每轮处理串口命令
    chuliChuanKou();
    // 每轮都读取摇杆
    if (moshi == 0)
    {
        gengxinYaogan();
    }
    // 获取当前运行时间
    unsigned long xianzai = millis();
    // 只有达到设定时间间隔，才更新舵机
    if (xianzai - shangciGengxinShijian >= (unsigned long)yanshi)
    {
        // 记录本次更新时间
        shangciGengxinShijian = xianzai;
        // 每经过yanshi毫秒移动1度
        gengxinDuoji();
        gengxinZhua();
    }
}