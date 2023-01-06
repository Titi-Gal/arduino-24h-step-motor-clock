#include <AccelStepper.h>
#include <GyverOLED.h>
#include <JC_Button.h>
#include <Wire.h>
#include <RTC.h>

//oled definitions:
GyverOLED<SSD1306_128x64, OLED_BUFFER> oled;

//RTC definitions
static DS3231 RTC;

//buttons definitions:
//buttons to ground and digital using internal PULL_UP resistor
#define ButtonPin4 4
#define ButtonPin5 5
#define ButtonPin6 6              
Button button4(ButtonPin4);
Button button5(ButtonPin5);
Button button6(ButtonPin6);
const unsigned long longPress = 1000;

//motor definitions:
#define motorPin1  8      
#define motorPin2  9      
#define motorPin3  10     
#define motorPin4  11     
#define MotorInterfaceType 8 // 4 wires halfsteps
#define stepsPerRevolution 4096  //steps per revolution
AccelStepper stepper = AccelStepper(MotorInterfaceType, motorPin1, motorPin3, motorPin2, motorPin4); //Initialize instance

//other variables
int state = 0;
unsigned long menuTimer = 0;
uint8_t hoursDec = 0;
uint8_t hoursUnit = 0;
uint8_t modhoursUnit;
uint8_t minsDec = 0;
uint8_t minsUnit = 0;
uint8_t minOld = 60;
uint8_t checkSetClockState;
char *strToDraw;

//functions definitions
uint16_t position_OfTime (uint32_t hours, uint16_t mins);
uint16_t time_ofPosition(void);
uint8_t hours_ofPosition (void);
uint8_t mins_ofPosition (void);
void Position_InShortestPath(uint16_t move_To);
void draw_timeAndPointer(uint8_t hoursDec, uint8_t hoursUnit, uint8_t minsDec, uint8_t minsUnit, uint8_t pointer);
void draw_smallText(char *strToDraw, uint8_t pos);
void draw_smallNumber(uint8_t number, uint8_t pos);
void draw_menu1(void);
void draw_menu2(void);
void draw_menu3(void);
void oled_off(void);
uint8_t checkSetClock(void);

void setup() {
  oled.init();
  oled.autoPrintln(true); //atumatic line jump when in end of line
  oled.clear(); //clear buffer
  oled.update();

  RTC.begin();
  RTC.setHourMode(CLOCK_H24); //set to 24 hours mode
  RTC.setDate(01,01,00); ///date set to DD/MM/YY 01/01/1900
  RTC.setWeek(2); // 01/01/1900 was a monday, this number is supposed to be mod7, weekdays are not used in this clock, so whatever

  stepper.setMaxSpeed(1000);
  stepper.setCurrentPosition(position_OfTime(RTC.getHours(), RTC.getMinutes()));
  
  button4.begin();
  button5.begin();
  button6.begin();
}

void loop()
{
  button4.read();
  button5.read();
  button6.read();
  switch (state)
  {
    //keeping time, button 4 long set clock, button 5 long reset position, any button short draws menu on oled
    case 0:
      if (RTC.getMinutes() != minOld)
      {
        minOld = RTC.getMinutes();
        Position_InShortestPath(position_OfTime(RTC.getHours(), RTC.getMinutes()));
      }
      else if (button5.pressedFor(longPress))
      {
        //oled: write
        Position_InShortestPath(position_OfTime(12,0));
      }
      else if (button4.pressedFor(longPress))
      { 
        draw_menu2();
        state ++; //state to wait release
      }
      else if (button4.wasReleased() || button5.wasReleased() || button6.wasReleased())
      {
        draw_menu1();
        menuTimer = millis();
      }
      else if(millis() - menuTimer >= longPress*10)
      {
        oled_off();
      }
      break;
    case 1: //longpress released goes to next state
      if (button4.wasReleased())
        state ++;
      break;

    //button 5 and 6 long to ajust position. button 4 long confirm and goes to set time now
    case 2:
      while (button5.read())
      {
        stepper.setSpeed(-stepper.maxSpeed()/2);
        stepper.runSpeed();
      }
      while (button6.read())
      {
        stepper.setSpeed(stepper.maxSpeed()/2);
        stepper.runSpeed();
      }
      if (button4.pressedFor(longPress))
        {
        stepper.disableOutputs();
        stepper.enableOutputs();
        stepper.setCurrentPosition(position_OfTime(12, 0));
        hoursDec = RTC.getHours() / 10;
        hoursUnit = RTC.getHours() % 10;
        minsDec = RTC.getMinutes() / 10;
        minsUnit = RTC.getMinutes() % 10;
        draw_menu3();
        draw_timeAndPointer(hoursDec, hoursUnit, minsDec, minsUnit, 0);
        state ++; //state to wait release
        }
    case 3: //longpress released goes to next state
      if (button4.wasReleased())
        state ++;
      break;

    //button 5 and 6 short ajust hours dec, hour unit, min dec, min unit. button 4 long confirm and goes to keep time
    case 4: //hour dec
      if (button5.wasReleased())
      {
        hoursDec = (hoursDec + 1) % 3;
        draw_timeAndPointer(hoursDec, hoursUnit, minsDec, minsUnit, 0);
      }
      else if (button6.wasReleased())
      {
        if (hoursDec == 2)
          modhoursUnit = 4;
        else
          modhoursUnit = 10;
        draw_timeAndPointer(hoursDec, hoursUnit, minsDec, minsUnit, 1);
        state ++;
      }
      else if (button4.pressedFor(longPress))
      {
        checkSetClockState = checkSetClock(); //state to go when released
        state = 8; //state to wait release
      }
      break;
    case 5: //hour unit
      if (button5.wasReleased())
      {
        hoursUnit = (hoursUnit + 1) % modhoursUnit;
        draw_timeAndPointer(hoursDec, hoursUnit, minsDec, minsUnit, 1);
      }
      else if (button6.wasReleased())
      {
        draw_timeAndPointer(hoursDec, hoursUnit, minsDec, minsUnit, 2);
        state ++;
      }
      else if (button4.pressedFor(longPress))
      {
        checkSetClockState = checkSetClock(); //state to go when released
        state = 8; //state to wait release
      }
      break; 
    case 6: //min dec
      if (button5.wasReleased())
        {
          minsDec = (minsDec + 1) % 6;
          draw_timeAndPointer(hoursDec, hoursUnit, minsDec, minsUnit, 2);
        }
      else if (button6.wasReleased())
      {
        draw_timeAndPointer(hoursDec, hoursUnit, minsDec, minsUnit, 3);
        state ++;
      }
      else if (button4.pressedFor(longPress))
      {
        checkSetClockState = checkSetClock(); //state to go when released
        state = 8; //state to wait release
      }
      break;
    case 7: //min unit
      if (button5.wasReleased())
        {
          minsUnit = (minsUnit + 1) % 10;
          draw_timeAndPointer(hoursDec, hoursUnit, minsDec, minsUnit, 3);
        }
      else if (button6.wasReleased())
      {
        draw_timeAndPointer(hoursDec, hoursUnit, minsDec, minsUnit, 0);
        state = 4;
      }
      else if (button4.pressedFor(longPress))
      {
        checkSetClockState = checkSetClock(); //state to go when released
        state = 8; //state to wait release
      }
      break;
      
    case 8: //longpress released goes to state returned by checkSetClock
      if (button4.wasReleased())
      {
        state = checkSetClockState;
      }
      break;
  }
}

//motor position relative to time functions
/*steps of motor works in mod stepsPerRevolution,
this way there is a correspondence between steps in one revolution to minuts in the day
in this motor 4096 steps of motor to 1440 minuts in day
when it completes a revolution if "overflows" to 0
*/

uint16_t position_OfTime(uint32_t hours, uint16_t mins) {
  //minuts of day converted in steps of motor
  return round(stepsPerRevolution * ((hours * 60 + mins) / (float)1440));
}
void Position_InShortestPath(uint16_t move_To)
{
  //moves the motor to a position in steps in the direction of the shortest path
  if (move_To < stepper.currentPosition())
  {
    if (move_To - stepper.currentPosition() < -(stepsPerRevolution / 2))
    {
      //clockwise
      stepper.move((move_To - stepper.currentPosition()) + stepsPerRevolution);
      stepper.setSpeed(stepper.maxSpeed());
    }
    else
    {
      //anticlockwise
      stepper.moveTo(move_To);
      stepper.setSpeed(-stepper.maxSpeed());
    }
  }
  else
  {
    if (move_To - stepper.currentPosition() > stepsPerRevolution / 2)
    {
      //clockwise
      stepper.move((move_To - stepper.currentPosition()) - stepsPerRevolution);
      stepper.setSpeed(-stepper.maxSpeed());
    }
    else
    {
      //anticlockwise
      stepper.moveTo(move_To);
      stepper.setSpeed(stepper.maxSpeed());
    }
  }
  while(stepper.distanceToGo() != 0)
  {
    stepper.runSpeed();
  }
  delay(5); //empirical minimal delay necessary to wait for the motor to spin before disabling votlage to output
  stepper.setCurrentPosition(move_To);
  stepper.disableOutputs();
  stepper.enableOutputs();
}

//if set clock is valid return state turn of oled and return state 0 else updtate display to valid time and return state 5
uint8_t checkSetClock(void)
{
  if (hoursDec == 2 && hoursUnit > 3)
  {
    hoursUnit = 3;
    modhoursUnit = 4;
    draw_timeAndPointer(hoursDec, hoursUnit, minsDec, minsUnit, 1);
    return 5;
  }
  else
  {
    RTC.setHours(hoursDec * 10 + hoursUnit);
    RTC.setMinutes(minsDec * 10 + minsUnit);
    RTC.setSeconds(0);
    minOld = 60;
    oled_off();
    return 0;
  }
}

//oled draw functions

//get time of motor position and get hours or minutes
uint16_t time_ofPosition(void)
{
  //steps of motor converted in minuts of day
  return round((1440 / (float)stepsPerRevolution) * stepper.currentPosition());
}
uint8_t hours_ofPosition(void)
{
  //steps of motor converted to hours of day
  return time_ofPosition() / (float)60;
}
uint8_t mins_ofPosition(void)
{
  //steps of motor converted to the minuts of the hour
  return time_ofPosition() % 60;
}

//draw small texts and numbers
void draw_smallText(char *strToDraw, uint8_t pos)
{  
  oled.setCursorXY(pos % 21 * 6, ((int)pos/21) * 8);
  oled.print(strToDraw);
}
void draw_smallNumber(uint8_t number, uint8_t pos)
{
  oled.setCursorXY(pos % 21 * 6, ((int)pos/21) * 8);
  oled.print(number);
}

//screens
void draw_timeAndPointer(uint8_t hoursDec, uint8_t hoursUnit, uint8_t minsDec, uint8_t minsUnit, uint8_t pointer)
{
  oled.setScale(4);
  oled.textMode(BUF_REPLACE);
  oled.setCursorXY(4, 32);
  oled.print(hoursDec);
  oled.print(hoursUnit);

  oled.setCursorXY(53, 32);
  oled.print(":");

  oled.setCursorXY(75, 32);
  oled.print(minsDec);
  oled.print(minsUnit);

  oled.rect(11, 24, 110, 27, OLED_CLEAR); // limpa o tamanho de um retangulo em (x0,y0,x1,y1)
  oled.setScale(2);
  oled.textMode(BUF_ADD);
  char point = 46;
  switch(pointer)
  {
    case 0:
      oled.setCursorXY(9, 14);
      oled.print(point);
      break;
    case 1:
      oled.setCursorXY(34, 14);
      oled.print(point);
      break;
    case 2:
      oled.setCursorXY(81, 14);
      oled.print(point);
      break;
    case 3:
      oled.setCursorXY(105, 14);
      oled.print(point);
      break;
  }
  oled.update();
}

void draw_menu1(void)
{ 
  oled.setPower(true);
  oled.clear();
  oled.setScale(1);
  strToDraw = "COUNTING TIME";
  draw_smallText(strToDraw, 0);
  draw_smallNumber(hours_ofPosition() / 10, 14);
  draw_smallNumber(hours_ofPosition() % 10, 15);
  strToDraw = ":";
  draw_smallText(strToDraw, 16);
  draw_smallNumber(mins_ofPosition() / 10, 17);
  draw_smallNumber(mins_ofPosition() % 10, 18);
  strToDraw = "LS";
  draw_smallText(strToDraw, 42);
  draw_smallText(strToDraw, 45);
  draw_smallText(strToDraw, 48);
  strToDraw = "L";
  draw_smallText(strToDraw, 52);
  strToDraw = "S";
  draw_smallText(strToDraw, 57);
  strToDraw = ".";
  draw_smallText(strToDraw, 63);
  draw_smallText(strToDraw, 66);
  draw_smallText(strToDraw, 69);
  strToDraw = "Long Short";
  draw_smallText(strToDraw, 73);
  strToDraw = "L";
  draw_smallText(strToDraw, 105);
  draw_smallText(strToDraw, 129);
  strToDraw = "Set Clock";
  draw_smallText(strToDraw, 115);
  strToDraw = "Reset Pos";
  draw_smallText(strToDraw, 136);
  oled.update();
}

void draw_menu2(void)
{
  oled.setPower(true);
  oled.clear();
  oled.setScale(1);
  strToDraw = "L";
  draw_smallText(strToDraw, 0);
  draw_smallText(strToDraw, 3);
  draw_smallText(strToDraw, 6);
  strToDraw = "Set Pos 12hOK <";
  draw_smallText(strToDraw, 10);
  strToDraw = ">";
  draw_smallText(strToDraw, 27);
  oled.update();
}

void draw_menu3(void)
{ 
  oled.setPower(true);
  oled.clear();
  oled.setScale(1);
  strToDraw = "L";
  draw_smallText(strToDraw, 0);
  strToDraw = "S";
  draw_smallText(strToDraw, 3);
  draw_smallText(strToDraw, 6);
  strToDraw = "Set Time NowOK ^";
  draw_smallText(strToDraw, 9);
  strToDraw = ">";
  draw_smallText(strToDraw, 27);
  oled.update();
}

void oled_off(void)
{
  oled.clear();
  oled.update();
  oled.setPower(false);
}
