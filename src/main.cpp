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
TimeChangeRule usCDT = {"CDT", Second, Sun, Mar, 2, -300}; // UTC - 5hr
TimeChangeRule usCST = {"CST", First, Sun, Nov, 2, -360};  // UTC - 6hr
Timezone usCentral(usCDT, usCST);
TimeChangeRule *tcr;
time_t utc, local;

// SD GLOBAL VARIABLES
File file;
const String FILENAME = "robit.txt";
int numMessages;
char c;

int timer = 5 + 1; // start timer above threshold to create new message on reset

// Opens the text file and reads the total number of lines.
// Also syncs with the clock module.
void setup()
{
    const int CHIP_SELECT = 8;

    Serial.begin(9600);

    // LCD INITIALIZATION
    lcd.begin(20, 4);
    lcd.clear();

    // SD INITIALIZATION
    pinMode(10, OUTPUT);
    if (!SD.begin(CHIP_SELECT))
    {
        Serial.println("Card failed or not present");
        return;
    }
    Serial.println("Card initialized.");

    file = SD.open(FILENAME);
    if (file)
    {
        Serial.println(FILENAME + " opened successfully!");
        c = file.read();
        numMessages = 1;
        while (file.available())
        {
            if (c == '\n')
            {
                numMessages++;
            }
            c = file.read();
        }
        Serial.println("There are " + String(numMessages) + " messages");
        Serial.println("Closing file");
        file.close();
    }
    else
    {
        Serial.println("Unable to open " + FILENAME);
    }

    // Uncomment on first run to set time
    //setDateTime();
    setSyncProvider(RTC.get);
    long time = (long)now();
    //Serial.println(time);
    randomSeed(time * time);
}

void setDateTime()
{
    setTime(17, 21, 11, 31, 10, 2015); // hr, min, sec, day, month, year
    RTC.set(now());
}

// LCD METHODS

void clearLine(unsigned int linenum)
{
    lcd.setCursor(0, linenum);
    lcd.print("                    ");
}

// Updates the first two lines of the display in the format:
// 00:00 on Weekday
// Month 00th
void updateTime()
{
    utc = now();
    local = usCentral.toLocal(utc);

    String minStr;
    String secStr;
    String topLineMessage;
    String secondLineMessage;

    if (minute(local) < 10)
    {
        minStr = "0" + String(minute(local));
    }
    else
    {
        minStr = String(minute(local));
    }

    lcd.setCursor(0, 0);
    topLineMessage = String(hourFormat12(local)) + ":" + minStr + " on " + dayStr(weekday(local));
    lcd.print(topLineMessage);
    for (unsigned int i = 0; i < 20 - topLineMessage.length(); i++)
    {
        lcd.print(' ');
    }
    // SECOND LINE
    secondLineMessage = monthStr(month(local)) + String(" ") + String(day(local));
    if ((day(local) == 1) || (day(local) == 21) || (day(local) == 31))
    {
        secondLineMessage += "st";
    }
    else if ((day(local) == 2) || (day(local) == 22))
    {
        secondLineMessage += "nd";
    }
    else if ((day(local) == 3) || (day(local) == 23))
    {
        secondLineMessage += "rd";
    }
    else
    {
        secondLineMessage += "th";
    }
    lcd.setCursor(0, 1);
    lcd.print(secondLineMessage);
    for (unsigned int i = 0; i < 20 - secondLineMessage.length(); i++)
    {
        lcd.print(" ");
    }
}
// SD METHODS

// Reads the text file at the specifed line (zero based) and populates
// the input char array with the format message[wordIndex][charIndex]
void getMessageAtLine(int linenum, char message[])
{
    for (int i = 0; i < 40; i++)
    {
        message[i] = NULL;
    }

    if (!SD.exists(FILENAME))
    {
        Serial.println(F("File doesn't exist"));
        return;
    }
    file = SD.open(FILENAME);
    if (file)
    {
        Serial.println(F("File opened"));
    }
    else
    {
        Serial.println(F("Error: File could not be opened"));
        return;
    }
    for (int i = 0; i < linenum; i++)
    {
        c = file.read();
        while (!(c == '\n'))
        {
            c = file.read();
        }
    }

    c = file.read();
    unsigned int letterIndex = 0;
    while (!(c == '\n') && !(c == '\r'))
    {
        message[letterIndex] = c;
        c = file.read();
        Serial.print(c);
        letterIndex++;
    }

    Serial.println();
    file.close();
}

// Returns -1 if message should be printed normally with no word breaks
// Otherwise, returns index of word that needs to be pushed to a new line
int checkForWordBreak(char *message)
{
    const unsigned int numLines = 2;

    unsigned int lineIndex = 0;
    unsigned int charsLeftInLine = 20;
    unsigned int wordIndex = 0;
    unsigned int charIndex = 0;
    unsigned int currWordLength = 0;
    int wordBreakIndex = -1;

    while (message[charIndex] != NULL && charIndex < 40)
    {
        if (lineIndex >= numLines)
        {
            return (wordBreakIndex == 0) ? wordBreakIndex : -1;
        }
        if (charsLeftInLine <= 0)
        {
            wordBreakIndex = wordIndex;
            lineIndex++;
            charsLeftInLine = 20 - currWordLength;
        }
        if (message[charIndex] == ' ')
        {
            currWordLength = 0;
            wordIndex++;
        }
        charIndex++;
        charsLeftInLine--;
    }
    return (wordBreakIndex > 0) ? wordBreakIndex : -1;
}

// LCD + SD METHODS

// Reads a message at a random line number and prints it
// on the last two lines of the lcd display
void printNewMessage()
{
    char message[40];

    int index = random(0, numMessages - 1);

    getMessageAtLine(index, message);

    clearLine(2);
    clearLine(3);
    lcd.setCursor(0, 2);
    unsigned int charsLeft = 20;
    unsigned int wordIndex = 0;
    int wordBreakIndex = checkForWordBreak(message);

    for (unsigned int i = 0; i < 40; i++)
    {
        if (message[i] == NULL)
        {
            return;
        }

        if (charsLeft == 0)
        {
            charsLeft = 20;
            lcd.setCursor(0, 3);
        }

        if (message[i] == ' ')
        {
            wordIndex++;
            if (wordIndex == wordBreakIndex)
            {
                lcd.setCursor(0, 3);
                charsLeft = 20;
                continue;
            }
        }

        lcd.print(message[i]);
        charsLeft--;
    }
}

// Updates time 1/sec and draws a new message 1/200sec
void loop()
{
    updateTime();

    // Determines how long each message stays before it is changed (in seconds)
    if (timer > 20)
    {
        printNewMessage();
        timer = 0;
    }

    delay(1000); // 1 second
    timer++;
}