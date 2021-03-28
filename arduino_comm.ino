/*
  This is a test sketch for the Adafruit assembled Motor Shield for Arduino v2
  It won't work with v1.x motor shields! Only for the v2's with built in PWM
  control

  For use with the Adafruit Motor Shield v2
  ---->  http://www.adafruit.com/products/1438
*/


#include <Wire.h>
#include <Adafruit_MotorShield.h>
#include <Servo.h>

// Create the motor shield object with the default I2C address
Adafruit_MotorShield AFMS = Adafruit_MotorShield();
// Or, create it with a different I2C address (say for stacking)
// Adafruit_MotorShield AFMS = Adafruit_MotorShield(0x61);

// Connect a stepper motor with 200 steps per revolution (1.8 degree)
// to motor port #1 (M1 and M2)   port #2 (M3 and M4)
Adafruit_StepperMotor *myMotor1 = AFMS.getStepper(200, 1);  // Y smaller motor
Adafruit_StepperMotor *myMotor2 = AFMS.getStepper(200, 2);  // X larger motor

Servo myServo;

#define COMMAND_NONE	0
#define COMMAND_RELEASE	1
#define COMMAND_INIT	2
#define COMMAND_GOTO	3
#define COMMAND_SERVO	4

#define	SUB_STEP		16
#define MAX_STEPS		(200*16)

#define SERVO_HOME		60




class FValue
{
  public:

    FValue()
    {
      init();
    }

    void
    init()
    {
      decimal = 0;
      negative = false;
      value = 0;
    }

    float
    eval()
    {
      float	f = value;

      if (decimal)
        f *= decimal;

      if (negative)
        f = -f;
      return f;
    }


    void
    feed(char c)
    {
      if (c >= '0' && c <= '9')
      {
        value *= 10;
        value += c - '0';
        if (decimal)
          decimal *= 0.1f;
      }
      else if (c == '-')
      {
        negative = !negative;
      }
      else if (c == '+')
      {
        negative = false;
      }
      else if (c == '.')
      {
        decimal = 1;
      }
    }

  private:

    float		decimal;
    bool	negative;
    float	value;
};

FValue  motorValue[2];
int   valueIndex = 0;
int   motorPos[2] = { 0, 0 };

char  motorCommand = COMMAND_NONE;

void
setup()
{
  Serial.begin(9600);           // set up Serial library at 9600 bps

  Serial.write('2');
  Serial.write('\n');

  AFMS.begin();  // create with the default frequency 1.6KHz
  //AFMS.begin(1000);  // OR with a different frequency, say 1KHz
}

void
gotoPosition()
{
  int x0 = motorPos[0];
  int y0 = motorPos[1];

  float v0 = motorValue[0].eval();
  float v1 = motorValue[1].eval();

  // convert to steps

  int x1 = int(v0 * MAX_STEPS / 360.0f + 0.5f);
  int y1 = int(v1 * MAX_STEPS / 360.0f + 0.5f);

  x0 %= MAX_STEPS;
  y0 %= MAX_STEPS;
  x1 %= MAX_STEPS;
  y1 %= MAX_STEPS;

  int dx = abs(x1 - x0);
  if (dx > (MAX_STEPS / 2))
  {
    if (x0 < x1)
      x0 += MAX_STEPS;
    else
      x1 += MAX_STEPS;
    dx = abs(x1 - x0);
  }
  int sx = x0 < x1 ? 1 : -1;

  int dy = abs(y1 - y0);
  if (dy > (MAX_STEPS / 2))
  {
    if (y0 < y1)
      y0 += MAX_STEPS;
    else
      y1 += MAX_STEPS;
    dy = abs(y1 - y0);
  }
  int sy = y0 < y1 ? 1 : -1;

  int err = (dx > dy ? dx : -dy) / 2;
  int e2;

  //	Serial.println("");

  while (1)
  {
    if (x0 == x1 && y0 == y1)
      break;

    e2 = err;
    if (e2 > -dx)
    {
      err -= dy;
      x0 += sx;

      if (sx > 0)
        myMotor1->onestep(FORWARD, MICROSTEP);
      else
        myMotor1->onestep(BACKWARD, MICROSTEP);
    }

    if (e2 < dy)
    {
      err += dx;
      y0 += sy;

      if (sy > 0)
        myMotor2->onestep(FORWARD, MICROSTEP);
      else
        myMotor2->onestep(BACKWARD, MICROSTEP);
    }

#if 0
    Serial.print(" POS ");
    Serial.print(x0, DEC);
    Serial.print(",");
    Serial.println(y0, DEC);
#endif

  }

  motorPos[0] = x0;
  motorPos[1] = y1;

  motorPos[0] %= MAX_STEPS;
  motorPos[1] %= MAX_STEPS;
}

void
executeMotor()
{
  int r = 0;
  int i;

  // SINGLE, DOUBLE, INTERLEAVE or MICROSTEP

  switch (motorCommand)
  {
    case COMMAND_GOTO:
      Serial.print("Goto ");
      Serial.print(motorValue[0].eval());
      Serial.print(",");
      Serial.print(motorValue[1].eval());
      gotoPosition();
      r = 1;
      break;

    case COMMAND_RELEASE:
      myMotor1->release();
      myMotor2->release();
      myServo.detach();
      Serial.print(" Release ");
      r = 1;
      break;


    case COMMAND_INIT:
      myServo.attach(10);
      myServo.write(SERVO_HOME);
      delay(15);
      motorPos[0] = 0;
      motorPos[1] = 0;
      r = 1;
      Serial.print(" Init ");
      break;

    case COMMAND_SERVO:
      myServo.write(motorValue[0].eval());
      delay(15);
      Serial.print(" Servo ");
      Serial.print(motorValue[0].eval());
      r = 1;
      break;
  }

  Serial.write(r ? " 1" : " 0");
  Serial.write('\n');

}


void loop()
{

  if (Serial.available() > 0)
  {
    char c = Serial.read();

    switch (c)
    {
      case 'g':
        motorCommand = COMMAND_GOTO;
        break;

      case ',':
        valueIndex = 1;
        break;

      case 'v':
        motorCommand = COMMAND_SERVO;
        break;

      case 'r':
        motorCommand = COMMAND_RELEASE;
        break;

      case 'i':
        motorCommand = COMMAND_INIT;
        break;

      case '\n':
      case '\0':
        executeMotor();
        motorCommand = COMMAND_NONE;
        valueIndex = 0;
        motorValue[0].init();
        motorValue[1].init();
        break;

      default:
        motorValue[valueIndex].feed(c);
        break;
    }
  }
}
