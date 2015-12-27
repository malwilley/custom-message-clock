#include <Timezone.h>
#include <Wire.h>
#include <DS3232RTC.h>
#include <SPI.h>
#include <SD.h>
#include <LiquidCrystal.h>
#include <Time.h>

// LCD GLOBAL VARIABLES
LiquidCrystal lcd(2, 3, 4, 5, 6, 7);

// TIMEZONE VARIABLES
TimeChangeRule usCDT = { "CDT", Second, Sun, Mar, 2, -300 }; // UTC - 5hr
TimeChangeRule usCST = { "CST", First, Sun, Nov, 2, -360 }; // UTC - 6hr
Timezone usCentral(usCDT, usCST);
TimeChangeRule *tcr;
time_t utc, local;

// SD GLOBAL VARIABLES
const int CHIP_SELECT = 8;
File file;
const String FILENAME = "robit.txt";
int numMessages;
int messageLine;
char c;

int messageLengthSec = 200; // how long the message stays before changing (in seconds)
int timer = messageLengthSec + 1; // start timer above threshold to create new message on reset

// Opens the text file and reads the total number of lines.
// Also syncs with the clock module.
void setup() {
	//Serial.begin(115200);

	// LCD INITIALIZATION
	lcd.begin(20, 4);
	lcd.clear();

	// SD INITIALIZATION
	pinMode(10, OUTPUT);
	if (!SD.begin(CHIP_SELECT)) {
		return;
	}
	//Serial.println("Card initialized.");
	file = SD.open(FILENAME);
	if (file) {
		//Serial.println(FILENAME + " opened successfully!");
		c = file.read();
		numMessages = 1;
		while (file.available()) {
			if (c == '\n') {
				numMessages++;
			}
			c = file.read();
		}
		file.close();
	}
	else {
		//Serial.println("Unable to open " + FILENAME);
	}
	
	// Uncomment on first run to set time
	//setDateTime();
	setSyncProvider(RTC.get);
	long time = (long)now();
	//Serial.println(time);
	randomSeed(time*time);
}

void setDateTime(){
	setTime(17, 21, 11, 31, 10, 2015); // hr, min, sec, day, month, year
	RTC.set(now());
}

// Updates time 1/sec and draws a new message 1/200sec
void loop() {
	updateTime();

	// Determines how long each message stays before it is changed (in seconds)
	if (timer > 200) {
		printNewMessage();
		timer = 0;
	}

	delay(1000); // 1 second 
	timer++;
}

// LCD METHODS

void clearLine(int linenum) {
	lcd.setCursor(0, linenum);
	lcd.print("                    ");
}

// Updates the first two lines of the display in the format:
// 00:00 on Weekday
// Month 00th
void updateTime() {
	utc = now();
	local = usCentral.toLocal(utc);

	String minStr;
	String secStr;
	String topLineMessage;
	String secondLineMessage;

	if (minute(local) < 10) {
		minStr = "0" + String(minute(local));
	}
	else  {
		minStr = String(minute(local));
	}

	lcd.setCursor(0, 0);
	topLineMessage = String(hourFormat12(local)) + ":" + minStr + " on " + dayStr(weekday(local));
	lcd.print(topLineMessage);
	for (int i = 0; i < 20 - topLineMessage.length(); i++) {
		lcd.print(' ');
	}
	// SECOND LINE
	secondLineMessage = monthStr(month(local)) + String(" ") + String(day(local));
	if ((day(local) == 1) || (day(local) == 21) || (day(local) == 31)) {
		secondLineMessage += "st";
	}
	else if ((day(local) == 2) || (day(local) == 22)) {
		secondLineMessage += "nd";
	}
	else if ((day(local) == 3) || (day(local) == 23)) {
		secondLineMessage += "rd";
	}
	else {
		secondLineMessage += "th";
	}
	lcd.setCursor(0, 1);
	lcd.print(secondLineMessage);
	for (int i = 0; i < 20 - secondLineMessage.length(); i++) {
		lcd.print(" ");
	}
}

// Displays a message on the last two lines of the display.
// Message must be <= 40 characters
void writeMessage(String text) {
	if (text.length() > 40) {
		lcd.setCursor(0, 2);
		lcd.print("Message > 40 chars");
		return;
	}
	if (text.length() > 20) {
		String line1 = text.substring(0, 20);
		String line2 = text.substring(20);
		lcd.setCursor(0, 2);
		lcd.print(line1);
		lcd.setCursor(0, 3);
		lcd.print(line2);
		return;
	}
	lcd.setCursor(0, 2);
	lcd.print(text);
}

// SD METHODS

// Reads the text file at the specifed line (zero based) and populates
// the input char array with the format message[wordIndex][charIndex]
void getMessageAtLine(int linenum, char message[][20]) {
	int wordIndex = -1;
	int letterIndex = 0;
	bool endOfLine = false;
	for (int i = 0; i < 16; i++) {
		for (int j = 0; j < 20; j++) {
			message[i][j] = '\0';
		}
	}
	file = SD.open(FILENAME);
	for (int i = 0; i < linenum; i++) {
		c = file.read();
		while (!(c == '\n')) {
			c = file.read();
			//Serial.print(c);
		}
	}
	while (!endOfLine) {
		c = file.read();
		wordIndex++;
		letterIndex = 0;
		while (!(c == ' ')) {
			if ((c == '\n') || (c == '\r')) {
				endOfLine = true;
				break;
			}
			message[wordIndex][letterIndex] = c;
			c = file.read();
			letterIndex++;
		}
	}
	file.close();
}

// LCD + SD METHODS

// Reads a message at a random line number and prints it
// on the last two lines of the lcd display
void printNewMessage() {
	char message[16][20];

	int index = random(0, numMessages - 1);

	//Serial.println(index);
	getMessageAtLine(index, message);
	clearLine(2);
	clearLine(3);
	lcd.setCursor(0, 2);
	int charsLeft = 20;
	int wordIndex = 0;
	messageLine = 2;
	String currWord;
	while (!(message[wordIndex][0] == '\0')) {
		currWord = String(message[wordIndex]);
		if ((currWord.length() > charsLeft) || (charsLeft <= 0)) {
			messageLine = 3;
			lcd.setCursor(0, 3);
			charsLeft = 20;
			lcd.print(currWord);
			//Serial.print(currWord);
			charsLeft -= (currWord.length() + 1);
		}
		else {
			if (wordIndex > 0) {
				lcd.print(' ');
				//Serial.print(' ');
			}
			lcd.print(currWord);
			//Serial.print(currWord);
			charsLeft -= (currWord.length() + 1); // for space character
		}
		wordIndex++;
		//Serial.println(charsLeft);
	}
	for (int i = 0; i < charsLeft + 1; i++) {
		lcd.print(' ');
	}
}