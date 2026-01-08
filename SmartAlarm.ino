 #include <LiquidCrystal.h>
 #include <LiquidCrystal_I2C.h>
 #include <Keypad.h>
 #include <Wire.h>
 #include "RTClib.h"

//Making lcd and rtc (real time clock) objects
LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS1307 rtc;

const byte ROWS = 4;
const byte COLS = 4;
const byte buzzer = 10;

// Keypad layout
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

// Pins connected to keypad
byte rowPins[ROWS] = {9, 8, 7, 6};
byte colPins[COLS] = {5, 4, 3, 2};

//Making keypad object
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

//Variables for math challenge
bool mathMode = false;
bool waitingAnswer = false;

int problemCount = 0;  // 0–2 (3 problems total)
int firstNum, secondNum;
char operation;
int correctAnswer;

unsigned long problemStartTime;
const unsigned long PROBLEM_TIME = 15000; // 15 seconds

char answerBuffer[5];
byte answerPos = 0;

//Variables for alarm logic
bool alarmMode = false;      // Are we in alarm setting mode?
char alarmInput[4];          // Stores the 4 digits typed (HHMM)
int inputPos = 0;             // Current position in alarmInput
int alarmH = -1, alarmMin = -1; // Final saved alarm
bool alarmRunning = false, alarmFinished = false, alarmTextDisplayed = false;

 void setup() {
  pinMode(buzzer, OUTPUT);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(5,0);

  Wire.begin();

  if (!rtc.begin()) {
    while (1);
  }

  if (!rtc.isrunning()) {
    // Set the date and time of rtc to compile time
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  //  Seed randomness using RTC + millis
  DateTime now = rtc.now();
  randomSeed(now.unixtime() + millis());
 }


void loop() {

  //Math logic
  if(mathMode){
    // If time expired 
    if(millis() - problemStartTime >= PROBLEM_TIME) {
      lcd.clear();
      lcd.setCursor(3,1);
      lcd.print("Time's Up!");
      delay(3000);

      // Start alarm again
      mathMode = false;
      alarmRunning = true;
      return; //Prevent executing the rest of loop()
    }

    //Read keypad
    char k = keypad.getKey();

    //Only allow digits
    if(k >= '0' && k <= '9' && answerPos < 4) {
      answerBuffer[answerPos++] = k;
      lcd.print(k);
    }

    //Submit answer
    if(k == 'D') {
      int userAnswer = atoi(answerBuffer);
      //If user answer is correct
      if(userAnswer == correctAnswer) {
        problemCount++;
        //If all 3 problems are solved
        if(problemCount >= 3) {
          lcd.clear();
          lcd.setCursor(3, 0);
          lcd.print("Alarm Off!");
          lcd.setCursor(1, 1);
          lcd.print("Enjoy your day");
          delay(3000);
          lcd.clear();

          //Exit math mode
          mathMode = false;
          alarmFinished = true;
          noTone(buzzer);
        } else {
          //If it's not third question
          lcd.clear();
          lcd.setCursor(4, 1);
          lcd.print("Correct!");
          delay(2000);
          generateProblem();
        }
      } 
      //If answer is wrong
      else {
        //Display message
        lcd.clear();
        lcd.setCursor(1, 0);
        lcd.print("Wrong answer!");
        lcd.setCursor(3, 1);
        lcd.print("Try again");
        delay(3000);
        //Display same problem again
        lcd.clear();
        lcd.setCursor(4,0);
        lcd.print("Answer:");
        lcd.setCursor(4,1);
        lcd.print(firstNum);
        lcd.print(operation);
        lcd.print(secondNum);
        lcd.print("=");

        //Restart timer and answerBuffer
        problemStartTime = millis();
        answerPos = 0;
        memset(answerBuffer, 0, sizeof(answerBuffer));
      }
    }
    return; //Skip the rest of loop
  }



  //Store keypad input and get current time
  char key = keypad.getKey();
  DateTime now = rtc.now();

  //If time reaches alarm time or alarm is already running
  if((now.hour() == alarmH && now.minute() == alarmMin && !alarmFinished) || alarmRunning){
    if(!alarmTextDisplayed){
      lcd.clear();
      lcd.setCursor(5,0);
      lcd.print("Alarm");
      alarmTextDisplayed = true;
    }
    lcd.setCursor(5,1);
    displayTime(now);
    alarmRunning = true;
    static unsigned long lastToneChange = 0; //Time of the last buzzer tone change
    static bool toneHigh = true; 

    //Buzzer logic
    unsigned long currentMillis = millis();

    //Change frequency every half of a second
    if(currentMillis - lastToneChange >= 500){
      lastToneChange = currentMillis;
      if(toneHigh){
        tone(buzzer, 1000);
      } else {
        tone(buzzer, 500);
      }
      toneHigh = !toneHigh;
    }

    // Check if user press any key
    if(key){
      //Silence the buzzer and switch to math mode
      noTone(buzzer);
      alarmRunning = false;
      mathMode = true;
      alarmFinished = true;
      alarmTextDisplayed = false;

      problemCount = 0;
      //Display the first math problem
      generateProblem();
    }
    
    return;
  }

  // When alarm minute pass, restart alarmFinished 
  // so it can fire again in 24 h
  if(now.hour() == alarmH && now.minute() > alarmMin){
    alarmFinished = false;
  }

    //Display time
    lcd.setCursor(11,0);
    displayTime(now);

    //If key is entered
    if(key){
      // If we are not currently inside alarm setting mode and
      // A is entered
      if(!alarmMode && key == 'A'){
          alarmMode = true;
          inputPos = 0;
          lcd.setCursor(0,1);
          lcd.print("Set Alarm: "); // label
          for(int i=0;i<4;i++) alarmInput[i] = ' '; // clear buffer
      }
      else{
        // We are in alarm setting mode
        if(key == 'C'){
          // Cancel alarm setting
          alarmMode = false;
          lcd.setCursor(0,1);
          lcd.print("                "); // clear row
        }
        else if(key == 'D'){
          // Save alarm if 4 digits entered
          if(inputPos == 4){
            alarmH = (alarmInput[0]-'0')*10 + (alarmInput[1]-'0');
            alarmMin = (alarmInput[2]-'0')*10 + (alarmInput[3]-'0');
            alarmMode = false;
            lcd.setCursor(0,1);
            lcd.print("Alarm Set"); // confirmation
            delay(2000);
            lcd.setCursor(0,1);
            lcd.print("                ");
          }
        }
        else if(inputPos < 4){
          // Save digits to buffer
          if(key >= '0' && key <= '9')  alarmInput[inputPos] = key;
          else return;
         
          // If entering hours or :
          if(inputPos <= 2) {
            lcd.setCursor(11 + inputPos,1); // show digits after "Set Alarm: "
          }else{
            //If entering minutes
            lcd.setCursor(11 + inputPos+1,1); 
          }
          //Display : between h and min
          if(inputPos == 2)  lcd.print(":");

          //Input validation for hours (>23)
          if(inputPos == 1 && ((alarmInput[0] - '0')*10 + (alarmInput[1] - '0') > 23)){
              lcd.setCursor(11, 1);
              alarmInput[0] = '0';
              lcd.print('0');
              alarmInput[1] = '0';
              lcd.print('0');
          //Input validation for minutes (>59)
          }else if(inputPos == 3 && ((alarmInput[2] - '0')*10 + (alarmInput[3] - '0') > 59)){
            lcd.setCursor(14, 1);
            alarmInput[2] = '5';
            lcd.print('5');
            alarmInput[3] = '9';
            lcd.print('9');
          //If input is valid display it
          }else{
            lcd.print(key);
          }

          inputPos++;
        }
      }
    }
  
  //Delay the loop
  delay(100);
}


//Custom function for generating math problem
void generateProblem() {
  //Generate random numbers and operation
  firstNum = random(10, 100);
  secondNum = random(10, 100);
  operation = (random(0, 3) == 0) ? '-' : '+';  // more + than -

  //If second number is greater and operation is -, swap numbers
  if(operation == '-' && secondNum > firstNum) {
    int t = firstNum;
    firstNum = secondNum;
    secondNum = t;
  }

  //Calculate correct answer
  correctAnswer = (operation == '+') ? 
                  firstNum + secondNum : 
                  firstNum - secondNum;

  //Display problem on screen
  lcd.clear();
  lcd.setCursor(4,0);
  lcd.print("Answer:");
  lcd.setCursor(4,1);
  lcd.print(firstNum);
  lcd.print(operation);
  lcd.print(secondNum);
  lcd.print("=");

  //Reset answerBuffer
  answerPos = 0;
  memset(answerBuffer, 0, sizeof(answerBuffer));

  //Set starting time
  problemStartTime = millis();
  waitingAnswer = true;
}

//Custom function for displaying current time
void displayTime(DateTime now){
  if(now.hour() < 10) lcd.print("0");
  lcd.print(now.hour());
  lcd.print(":");
  if(now.minute() < 10) lcd.print("0");
  lcd.print(now.minute());
}

