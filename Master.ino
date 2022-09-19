#include <LiquidCrystal.h>
#include <SoftwareSerial.h>
#include <string.h>

#define tx 11
#define rx 12
#define buttonPin 6
#define potentiometerPin A0
#define degOfFree 17.0
#define critical_value 2.11

SoftwareSerial bluetooth(rx,tx); 
LiquidCrystal lcd(10,9,5,4,3,2);

int angleCounter = -180;
int numberOfScans;

bool isEnabled = false;
bool posA=true;
bool doneMoving;
bool filled = false;
bool changeFound = false; 

short int scanData[180];
short int initialA[180];
short int initialB[180];


// Stores the instruction and magnitude- used to store the car movement intructions
typedef struct Path {
  String action;
  short int magnitude;
} Path;

Path AtoB[5] = {{"LEFT",135},{"FORWARD",270},{"RIGHT",90},{"FORWARD",240},{"LEFT",45}};
Path BtoA[5] = {{"LEFT",135},{"FORWARD",240},{"LEFT",90},{"FORWARD",270},{"LEFT",45}};



void setup(){
  lcd.begin(16,2); 
  Serial.begin(9600);
  bluetooth.begin(9600);
  
  pinMode(buttonPin, INPUT);
  pinMode(tx, OUTPUT);
  pinMode(rx, INPUT);
}

void loop(){

  if(digitalRead(buttonPin) == HIGH){
    isEnabled = true;
  }

  // If the user has pressed the button, run
  if(isEnabled){
    numberOfScans = round(analogRead(potentiometerPin)/204.6)+1; //potentiometer pin adjusts number of scans 
    if (posA){
      //if the car is at position A, it will travelled from A to B
      changeLCDDisplay("Moving A to B", numberOfScans); //change the LCD display to tell the user the car is travelling from A to B
      Serial.println("Changing");
      changePosition(AtoB);
    }else{
      //the car is at position B so it will travel from position B to A
      changeLCDDisplay("Moving B to A", numberOfScans); //change the LCD display to tell the user the car is travelling from B to A
      changePosition(BtoA);
    }
    changeLCDDisplay("Scanning", numberOfScans); //change the LCD display to tell the user the car is now scanning the room
    for (int count=0;count<numberOfScans;count++){
     scan();
     removeAnomalies(scanData);
     Serial.println(filled);
     if (posA && filled){ //if position that the car is in has values to compare the scan to, compare the scan taken to the initial scan
       changeLCDDisplay(" Alert!", numberOfScans);
       changeFound = compare(scanData, initialA);
     }else if(filled){
       changeFound = compare(scanData, initialB);
     }
     if(changeFound){ //if a change is detected then it will send the slave "ALARM0>" which will make the slave go into alarm mode
       Serial.println("CHANGEEEEE");
       while(true){
         bluetooth.write("ALARM0>");
         delay(10000);
       }
     }
     memset(scanData, 0, 180);    
     delay(5000);
    }
    
  }
}


//scan function that sends the slave the command to scan.
//If Room A and Room B have no data, then the scan will assign the first scans in both rooms
//rooms as the initial data which the other scans will be compared against
void scan(){
  bluetooth.print("SCAN0>"); //"SCAN0>" will be sent to the slave to tell it to scan. The "0" is just a placeholder which will not be used.
  receivedData(scanData);
  //if the car is in position A or B and it doesn't have anything to compare the scan to, take the scan as the initial scan in which all other scans will be compared to
  if(not filled){
    if(posA){
      memcpy(initialA, scanData, 360); //set data for room A
      filled = true;
    } else{
      memcpy(initialB, scanData, 360);//set data for room B
    }
  }
}


//receivedData is called to read data that is sent from the slave which will be in a format of a string
//separated by delimiters "," and terminated with ">". For scanning, the slave will send 180 values and the values will
//be assigned to the parameter scanData[] with the index being in line with the angle in which the scan
//was taken.
//When the car is moving, it sends the master the termiantor ">" to indicate it's done moving and that master,
//can send the next instruction
int receivedData(short int scanData[]){
  String receivedStr;
  char receivedChar;
  int index = 0;
  Serial.println("HasRecvieved"); 
  while (true){
    if(bluetooth.available() > 0){
      receivedChar = bluetooth.read(); //reads character sent from slave
      if(isDigit(receivedChar)){
        receivedStr += receivedChar;//adds the character to the string 
      }
      else if(receivedChar == ','){ //',' indicates that there are no more characters to be added to that string 
        scanData[index] = receivedStr.toInt(); //casts the string as an integer and add it to the array
        receivedStr = "";//reset receivedString to starting taking in the next value
        index++;
      }
      else if(receivedChar == '>'){ // '>'is used as the termimator to indicate the end of the string or that the car has finished moving
        return 0;
      }
    }
  }
}

//changeLCDDisplay takes a String which is the output message, and an integer which is the number of scans
//the user wants performed and outputs them on the first and second line of the LCD display
void changeLCDDisplay(String outputMessage, int numOfScans){
  Serial.println(outputMessage);
  lcd.clear();
  lcd.setCursor(0,0); 
  lcd.print(outputMessage);
  lcd.setCursor(0,1);
  lcd.print("No. of scans = " + String(numOfScans));
}


//changePosition takes a typedef as the predetermined route the car should take
//and sends the slave the directions and magnitude travel
void changePosition(Path route[]){
  for (int i=0;i<=4;i++){
    bluetooth.print(route[i].action + String(route[i].magnitude) + ">"); //sends the slave the direction and magnitude     
    delay(100);
    receivedData(scanData); //The car will send the master the terminator ">" once it's finished moving
  }
  posA = not posA; //set posA to not posA as the car will have travelled either to or from point A
}


// Compares each value to the mean of the neighbouring values. 
// If the value is more than double the mean, it is set to the mean.
void removeAnomalies(short int arr[]){
  for (int i=1;i<179;i++){
    if (arr[i]>((arr[i-1]+arr[i+1])/2)*2) {
      arr[i]=(arr[i-1]+arr[i+1])/2;
    } else if (arr[i]<((arr[i-1]+arr[i+1])/2)*0.5){
      arr[i]=(arr[i-1]+arr[i+1])/2;
    }
  }
}

// Iterates through every 18 group of values (cluster) and performs a t-test with the initial position data.
// If the absolute t value is larger than the critical value there has been a significant change and true is returned.
bool compare(short int arr[], short int initialData[]){

  for (int cluster=0;cluster<10;cluster++){
    short int difference[18], i, sumOfDifference,sumSquareDifference;
    float meanDifference,sampleVariance,t;

    subArray(initialData,arr,difference,cluster); 

    sumOfDifference =sum(difference,1);
    sumSquareDifference=sum(difference,2);

    meanDifference=sumOfDifference/18.0;

    sampleVariance=(sumSquareDifference-(18*pow(meanDifference,2)))/degOfFree;

    t=(meanDifference-0)/(sqrt(sampleVariance)/sqrt(18));
    if (fabs(t)>critical_value){
      return true;
    }
  }
  return false;
}

// Subtracts each value in the initial data from the scan data and returns an array of the differences.
void subArray(short int array1[], short int array2[], short int result[], int cluster) { 
    for(int i=(cluster*18+0); i< (cluster*18+18);i++){
        result[i%18]=array2[i]-array1[i];
    }
} 

// Returns an array of the square of each value in difference.
int sum(short int arr[], int power){
    int sum = 0;
    for (int i = 0; i < 18; i++){
      sum += pow(arr[i],power);
    }
    return sum;
}
