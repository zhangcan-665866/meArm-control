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
//创建四个舵机对象
Servo servoX;
Servo servoY;
Servo servoZ;
Servo claw;
// 设置初始角度为90°
int jiaoduX = 90;
int jiaoduY = 90;
int jiaoduZ = 90;
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
// 当前舵机
char dangqianDuoji = 0;
// 当前正在读取的角度
int shuruJiaodu = 0;
// 判断当前是否已经读取到数字
bool youShuzi = false;
void gengxinYaogan()
{
    // 读取摇杆X和Y
    int yaoganX = analogRead(yaogan_X_PIN);
    int yaoganY = analogRead(yaogan_Y_PIN);

    // 计算偏移量
    int pianyiX = yaoganX - yaogan_zhongxin;
    int pianyiY = yaoganY - yaogan_zhongxin;
    //X轴控制

    if (abs(pianyiX) > siqu)
    {
        // 根据摇杆偏移量计算移动步数动的越大越快
        int bushuX = map(abs(pianyiX), siqu, 511, 1, 3);

        // 限制步数
        bushuX = constrain(bushuX, 1, 3);

        if (pianyiX > 0)
        {
            jiaoduX += bushuX;
        }
        else
        {
            jiaoduX -= bushuX;
        }
    }
    //Y轴控制

    if (abs(pianyiY) > siqu)
    {
        // 根据摇杆偏移量计算移动步数
        int bushuY = map(abs(pianyiY), siqu, 511, 1, 3);

        // 限制步数
        bushuY = constrain(bushuY, 1, 3);

        if (pianyiY > 0)
        {
            jiaoduY += bushuY;
        }
        else
        {
            jiaoduY -= bushuY;
        }
    }

    //限制角度
    jiaoduX = constrain(jiaoduX, 0, 180);
    jiaoduY = constrain(jiaoduY, 0, 180);

    // 输出舵机角度 
    servoX.write(jiaoduX);
    servoY.write(jiaoduY);
}
//读取爪子命令函数
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
        jiaoduX = constrain(shuruJiaodu, 0, 180);
        break;
    case 'y':
        jiaoduY = constrain(shuruJiaodu, 0, 180);
        break;
    case 'z':
        jiaoduZ = constrain(shuruJiaodu, 0, 180);
        break;
    // 下一轮
    shuruJiaodu = 0;
    youShuzi = false;
}
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
        }

        // S：爪子关闭
        else if (mingling == 'S')
        {
            mubiaoZhua = 0;
        }
         // H：提高速度
        else if (mingling == 'H')
        {
            yanshi -= 5;
            yanshi = constrain(
                yanshi,
                minYanshi,
                maxYanshi
            );
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
    }   // x. y. z
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
    servoX.write(jiaoduX);
    servoY.write(jiaoduY);
    servoZ.write(jiaoduZ);
   claw.write(dangqianZhua);
    // 开启串口
    Serial.begin(9600);//有可能要调成38400
}

void loop()//反复执行
{   gengxinYaogan();
    chuliChuanKou();
    gengxinZhua();
    servoZ.write(jiaoduZ);
    delay(yanshi);
}