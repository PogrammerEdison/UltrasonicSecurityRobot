#include <stdio.h>
#include <Servo.h>
#include <string.h>
#include <AFMotor.h>
#include "Ultrasonic.h"
#include <SoftwareSerial.h>
#define tx A5
#define rx A3
#define melody 5

SoftwareSerial bluetooth(rx, tx); //RX, TX
AF_DCMotor rightBack(1);
AF_DCMotor rightFront(2);
AF_DCMotor leftFront(3);
AF_DCMotor leftBack(4);
Servo myServo;
Ultrasonic ultrasonic(8);


byte motorSpeed = 255;
float velocity = 0.3;
byte redLED = 9;
byte blueLED = 10;
byte servoPin = 3;
boolean doneDriving = false;
boolean requested = false;
String recievedNum="";
String recievedInstruction="";
short int angleCounter=-180;

int main_melody[] = {659,659,0,659,0,523,659,784,0,392,0};
int rhythm[] = {250,250,250,250,250,250,500,500,500,500,500};


typedef struct Path {
  String instruction="";
  int magnitude;
} Path;



void setup() {
  rightBack.run(RELEASE);                         //Ensure all motors are stopped
  rightFront.run(RELEASE);
  leftFront.run(RELEASE);
  leftBack.run(RELEASE);

  myServo.attach(servoPin);
  myServo.write(0); 
  Serial.begin(9600);
  
  pinMode(redLED, OUTPUT);
  pinMode(blueLED, OUTPUT);

  bluetooth.begin(9600);
  pinMode(tx, OUTPUT);
  pinMode(rx, INPUT);
}


void loop(){
  if(bluetooth.available()>0){
    receiveEvent();
  }
}

//receiveEvent reads data sent by the master. If it receives a character, then it will append it to instruction string
//If it is a digit, it will append it to the number string. The function will return 0 once it reaches the terminator in the data sent by the master '>'.
//If the master sends a command to scan, alarm or drive, then it will call the respective functions.
int receiveEvent() {
  Path msg;
  char c= bluetooth.read(); 
  if (c!='>'){ 
    if (isDigit(c)){
      recievedNum+=c; //appends the digit to the number String, for scan and alarm, these will just be placeholder values '0', for drive it will be magnitude
    }else if (isAlpha(c)){
      recievedInstruction+=c;//appends the characte to the instruction string
    }
    return 0;
  }
  msg.instruction=recievedInstruction;
  msg.magnitude=recievedNum.toInt(); //convert the number string to integer
  
  if (msg.instruction == "ALARM") {
    alarm();
  } else if (msg.instruction == "SCAN") {
    scan(msg);
  } else {
    drive(msg);
  }
  recievedNum="";
  recievedInstruction="";
}

//Causes the Piezo to play a siren sound and RGB to turn blue and red 
void alarm() {
  int f = 911;
  boolean changePitch = false;
  while(f != 912){

    if(f == 634 or changePitch == true){
      tone(5, f);
      changePitch = true;
      f++;
    } else {
      tone(5, f);
      f--;
    }
    if(f%20 == 0){
      rgb(255,0);
    } else if(f%10 == 0){
      rgb(0,255);
    }
    delay(10);
  }
  noTone(5);
  rgb(0,0);
  delay(2);
  
} 

//scan causes the servo to turn 180 degrees and uses the ultrasonic sensor to work out the distance between any objects
//at the angle and adds them to an array, then sends the values to the master for processing.
void scan(Path msg) {
  short int scanData[180];
  for (int i=0;i<180;i++){
    Serial.println("SCAN: "+String(i));
    myServo.write(180-abs(angleCounter)); //servo rotates by 1 degree 
    short int distance = ultrasonic.MeasureInCentimeters(); //sends an ultrasonic wave to workout the distance between the object the wave reflects off
    scanData[(180-abs(angleCounter))]=distance;
    delay(10);
    angleCounter++;
    if (angleCounter>180){
      angleCounter=-180;
    }else if (angleCounter==0){
      angleCounter++;
    }
  }
  for (int i=0;i<180;i++){
    Serial.println(scanData[i]);
    bluetooth.print(String(scanData[i])+",");
  }
  bluetooth.print(">");
}

//Drive takes the path as a parameter and determines how long, and what direction the motors 
//should turn in order to achieve movement in the instructed direction and magnitude
void drive(Path msg) {
            rightBack.setSpeed(motorSpeed);
            rightFront.setSpeed(motorSpeed);
            leftFront.setSpeed(motorSpeed);
            leftBack.setSpeed(motorSpeed);
  delay(100);
  Serial.println(msg.instruction);
  if (msg.instruction.equals("FORWARD")) { //makes all the motors go forwards
    rightBack.run(FORWARD);
    rightFront.run(FORWARD);
    leftFront.run(FORWARD);
    leftBack.run(FORWARD);
    Serial.println(msg.magnitude/velocity);
    delay(msg.magnitude/velocity); //keeps the motors running for the time needed to achieve the magnitude
  } else if (msg.instruction.equals("BACKWARD")) { //makes all the motors go backwards
    rightBack.run(BACKWARD);
    rightFront.run(BACKWARD);
    leftFront.run(BACKWARD);
    leftBack.run(BACKWARD);
    Serial.println(msg.magnitude/velocity);
    delay(msg.magnitude/velocity);
  } else if (msg.instruction.equals("LEFT")) { //makes the motors on the left go backwards and the ones on the right go forwards to make the car rotate left
    rightBack.run(FORWARD);
    rightFront.run(FORWARD);
    leftFront.run(BACKWARD);
    leftBack.run(BACKWARD);
    Serial.println("Moved left");
//    delay(round(550*(msg.magnitude/90)));
    if (msg.magnitude==90){
      delay(550); //keeps motors running until it has turned 90 degrees
    }else{
      delay(280); //keeps motors running until it has turned 45 degrees
    }
  } else if (msg.instruction.equals("RIGHT")) { //makes the motors on the right go backwards and the ones on the left go forwards to make the car rotate left
    rightBack.run(BACKWARD);
    rightFront.run(BACKWARD);
    leftFront.run(FORWARD);
    leftBack.run(FORWARD);
    Serial.println("Moved right");
    if (msg.magnitude==90){
      delay(625); //keeps motors running until it has turned 90 degrees
    }else{
      delay(350); //keeps motors running until it has turned 45 degrees
    }
  }
  //stops all the motors
  rightBack.run(RELEASE);
  rightFront.run(RELEASE);
  leftFront.run(RELEASE);
  leftBack.run(RELEASE);
  bluetooth.write(">"); //sends the terminator value to the master indicating that it has performed the movement
}


//makes the RGB led turn blue and red
void rgb (byte redValue, byte  blueValue) {
  analogWrite(redLED, redValue);
  analogWrite(blueLED, blueValue);
}
