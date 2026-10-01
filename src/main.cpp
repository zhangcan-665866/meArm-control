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
int jiaoduZhua = 0;
// 摇杆中心值
const int yaogan_zhongxin = 512;
// 死区大小(防止轻微触动抖动)
const int siqu = 30;
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
    claw.write(jiaoduZhua);
    // 开启串口
    Serial.begin(9600);//有可能要调成38400
}

void loop()//反复执行
{gengxinYaogan();

    delay(20);
}