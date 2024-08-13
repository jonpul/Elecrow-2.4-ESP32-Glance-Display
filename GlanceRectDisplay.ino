
// days between dates https://stackoverflow.com/questions/73844879/calculate-the-number-of-days-between-two-dates-in-c-convert-date-to-days


#include <TFT_eSPI.h>     // display 
#include <WiFi.h>         
#include <HTTPClient.h>
#include <Arduino_JSON.h> // https://github.com/arduino-libraries/Arduino_JSON
#include "secrets.h"
#include <JPEGDecoder.h>
#include <time.h>
#include <math.h>

// icons
#include "dadjoke.h"
#include "hearts.h"
#include "halloween.h"
#include "xmastree.h"

#define lcdBacklightPin 27
#define minimum(a,b)     (((a) < (b)) ? (a) : (b))

// custom fonts (generated from https://oleddisplay.squix.ch/)
#include "Roboto_22.h"
#include "Roboto_12.h"
// built in font and GFXFF reference handle
#define GFXFF 1
// font aliases
#define ROBOTO22 &Roboto_22
#define ROBOTO12 &Roboto_12

// gesture thresholds
#define MILLIS_LONGPRESS 500      // how long makes a tap into a tap and hold?
#define SWIPE_PIXEL_VARIANCE 40   // how many pixels to ignore from the start for a swipe gesture 
#define SWIPE_MIN_LENGTH 30       // how many pixels required to make a swipe

enum touchGesture 
{
  NOTOUCH = 0,
  TAP = 1,
  LONGTAP = 2,
  SWIPERIGHT =3,
  SWIPELEFT = 4,
  SWIPEDOWN = 5,
  SWIPEUP = 6    
};
int startX, startY;
int curX, curY;
int tapX, tapY;
long startTouchMillis, endTouchMillis;
enum touchGesture gestureResult = NOTOUCH;

int year = 0;
int month = 0;
int day = 0;
int yearMarried = 1987;

// Pages
// originally an enum but it wasn't actually bringing much to the party so just doing it manually
//  THOUGHT/QUOTE = 0
//  DADJOKE = 1
//  DAYSTOANNIVERSARY = 2
//  DAYSTOHALLOWEEN = 3
//  DAYSTOCHRISTMAS = 4
const int numPages = 5;
int curPage = 0;
bool pageJustChanged = false;

// timer values
#define PAGE_DISPLAY_MILLIS 30000         // how long to display a page
int lastPageDisplayMillis = millis();     // initialize page pause timer
#define THOUGHT_REFRESH_MILLIS 3600000    // how long to get a new thought  (3600000 = 1 hr)
long lastThoughtRefreshMillis = millis(); // how long since we last refreshed the thought
#define DADJOKE_REFRESH_MILLIS 1800000    // how long to get a new joke
long lastDadJokeRefreshMillis = millis(); // how long since we last refreshed the joke
#define DATE_REFRESH_MILLIS 14400000      // how long to refresh date (1440000000 = 4 hrs)
long lastDateRefreshMillis = millis();    // how long since we last refreshed the date


// have a few stored quotes in case we have rate limit or connection problems
String cannedThoughts[] = {"The shoe that fits one person pinches another. There is no recipe for living that suits all cases.","Neither a borrower nor a lender be.", "The greatest glory in living lies not in never falling, but in rising every time we fall.", "The way to get started is to quit talking and begin doing.", "Your time is limited, so don't waste it living someone else's life.","If you set your goals ridiculously high and it's a failure, you will fail above everyone else's success."};
String cannedAuthors[] = {"Carl Jung","William Shakespeare", "Nelson Mandela", "Walt Disney", "Steve Jobs", "James Cameron"};
String thought = "";
String author = "";
int thoughtLines = 0;

String joke = "";

#define DEBUG
uint16_t touchX, touchY;
uint16_t calData[5] = { 557, 3263, 369, 3493, 3 };

TFT_eSPI tft = TFT_eSPI(); 

void setup() {
  // put your setup code here, to run once:
  // prep the display
  Serial.begin(115200);
  //Backlight Pins
  pinMode(lcdBacklightPin, OUTPUT);
  digitalWrite(lcdBacklightPin, HIGH);
  
  //attempt connection with retries and exit if failed
  connectWifi(false);
  tft.begin();
  tft.setRotation(3);  // landscape with USB port on the left and sd card slot on top
  tft.setTouch(calData);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE);
  if(!WiFi.isConnected())
  {
    Serial.println("Not connected");
    tft.fillScreen(TFT_RED);
    tft.drawString("No Wifi connection", 10, 10,4);
    delay(50000);
  }
  if(WiFi.isConnected()) // only makes sense to do this if we have a connection
  {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Ready...", 10,10,4);
  
    //  drawArrayJpeg(tinyxmastree, sizeof(tinyxmastree), 10, 20); // Draw a jpeg image stored in memory
    //  drawArrayJpeg(tinyhalloween, sizeof(tinyhalloween), 10, 70); // Draw a jpeg image stored in memory
    //  drawArrayJpeg(tinyhearts, sizeof(tinyhearts), 70, 20); // Draw a jpeg image stored in memory
    //  drawArrayJpeg(tinydadjoke, sizeof(tinydadjoke), 70, 70); // Draw a jpeg image stored in memory

    getThought(true);
    while(thought.length()<1 || thought.length()>100)
    {
      Serial.println("Thought too long, getting another");
      getThought(true);
    }
    thought = breakStringIntoLines(thought,true);
    #ifdef DEBUG
      Serial.println(thought);
    #endif
    displayThought();  

    getDadJoke(true);
    joke = breakStringIntoLines(joke,false);
    Serial.println(joke);
  }
  getTime(true);
}

void loop() {
  // get a new quote manually and reset the timer
  bool touched = tft.getTouch( &touchX, &touchY, 600);
  if ( touched )
  {
    if(startTouchMillis == 0)  // new touch
    {
      startX = touchX;
      startY = touchY;
      curX = startX;
      curY = startY;
      startTouchMillis = millis();
    }
    else  // continuing touch
    {
      curX = touchX;
      curY = touchY;
    }
  }
  else  // no touch
  {
    if(startTouchMillis !=0)   // a touch just ended
    {
      endTouchMillis = millis();   
      if(endTouchMillis - startTouchMillis >= MILLIS_LONGPRESS) // tap & hold
      {
        tapX = curX;
        tapY = curY;
        gestureResult = LONGTAP;
        Serial.println("Long tap");
      }
      // else if((curX > startX) &&
      //         (abs(curX-startX) > SWIPE_MIN_LENGTH) && 
      //         (abs(curY-startY)< SWIPE_PIXEL_VARIANCE))  // right swipe
      // {
      //   gestureResult = SWIPERIGHT;
      //   Serial.println("Swipe right");
      // }
      // else if((curX < startX) &&
      //         (abs(curX-startX) > SWIPE_MIN_LENGTH) && 
      //         (abs(curY-startY)< SWIPE_PIXEL_VARIANCE))  // left swipe
      // {
      //   gestureResult = SWIPELEFT;
      //   Serial.println("Swipe left");
      // }
      // else if((curY > startY) &&
      //         (abs(curY-startY) > SWIPE_MIN_LENGTH) && 
      //         (abs(curX-startX)< SWIPE_PIXEL_VARIANCE))  // down swipe
      // {
      //   gestureResult = SWIPEUP;
      //   Serial.println("Swipe up");
      // }
      // else if((curY < startY) &&
      //         (abs(curY-startY) > SWIPE_MIN_LENGTH) && 
      //         (abs(curX-startX)< SWIPE_PIXEL_VARIANCE))  // up swipe
      // {
      //   gestureResult = SWIPEDOWN;
      //   Serial.println("Swipe down");
      // } 
      else // must be a regular tap if we've gotten here
      {
        tapX = curX;
        tapY = curY;
        gestureResult = TAP;
      }
      // reset the touch tracking
      startTouchMillis = 0;  
      startX = 0;
      startY = 0;
      curX = 0;
      curY = 0;
      endTouchMillis = 0;
    }
  } // end gesture handling

  if(gestureResult == TAP)
  {
      curPage++;
      lastPageDisplayMillis = millis(); // reset page display timer
      if(curPage > (numPages-1))
      {
        curPage = 0;
      }
      gestureResult = NOTOUCH;
      pageJustChanged = true;
      tft.fillScreen(TFT_BLACK); 
  }
  if (gestureResult == LONGTAP) 
  {
    switch(curPage)
    {
      case 0: // thought/quote, get a new one
        thought = "";
        while(thought.length()<1 || thought.length()>100)
        {
          #ifdef DEBUG
            Serial.println("Thought too long, getting another");
          #endif
          getThought(false);
        }
        thought = breakStringIntoLines(thought,true);
        lastThoughtRefreshMillis = millis();           
        pageJustChanged = true;
        lastPageDisplayMillis = millis(); // reset page display timer
        break; 
      case 1: // dad joke, get a new one
        joke = "";
        while(joke.length()<1 || joke.length()>115)
        {
          #ifdef DEBUG
            Serial.println("Joke too long, getting another");
          #endif
          getDadJoke(false);
        }
        joke = breakStringIntoLines(joke,false);
        lastDadJokeRefreshMillis = millis();   
        pageJustChanged = true;
        lastPageDisplayMillis = millis(); // reset page display timer
        break;
      case 2: // anniversary
        break;
      case 3: // halloween
        break;
      case 4: // christmas
        break;
    }
    gestureResult = NOTOUCH;
  }
    // display the page based on the type
  switch(curPage)
  {
    case 0:  // thought/quote
      if(millis() > (THOUGHT_REFRESH_MILLIS + lastThoughtRefreshMillis))
      {
        getThought(false); 
        thought = breakStringIntoLines(thought, true);
        lastThoughtRefreshMillis = millis();
      }
      if(pageJustChanged)
        displayThought();
      pageJustChanged = false;
      break;
    case 1:  // dad joke
      if(millis() > (DADJOKE_REFRESH_MILLIS + lastDadJokeRefreshMillis))
      {
          getDadJoke(false);
          joke = breakStringIntoLines(joke, false);
          Serial.println(joke);
          lastDadJokeRefreshMillis = millis();
      }
      if(pageJustChanged)
        displayDadJoke();
      pageJustChanged = false;
      break;
    case 2:  // days til anniversary
        if(pageJustChanged)
          displayDaysToEvent(2, daysBetweenDateAndNow(2024,10,30), setYearOrdinal((year-yearMarried)));
        pageJustChanged = false;
        break;
    case 3: // halloween
      if(pageJustChanged)
        displayDaysToEvent(1, daysBetweenDateAndNow(2024,10,31), "");
      pageJustChanged = false;
      break;
    case 4: // christmas
      if(pageJustChanged)
        displayDaysToEvent(0, daysBetweenDateAndNow(2024,12,25), "");
      pageJustChanged = false;
      break;
  }
  // check the page timer and increment the page if its time (unless stopwatch is running)
  if(millis() > (lastPageDisplayMillis + PAGE_DISPLAY_MILLIS))
  {
    curPage++;
    lastPageDisplayMillis = millis(); // reset page display timer
    if(curPage > (numPages-1))
    {
      curPage = 0;
    }
    //gestureResult = NOTOUCH;
    pageJustChanged = true;
    tft.fillScreen(TFT_BLACK);
  }  
  // update the date periodically
  if(millis() > (lastDateRefreshMillis + DATE_REFRESH_MILLIS))
  {
      getTime(false);
      Serial.println("date refreshed by timer");
      lastDateRefreshMillis = millis();
      // no need for pageJustChanged as we're not displaying the date anywhere
  }
}

// quietMode = true will try to connect but won't show any messages. 
bool connectWifi(bool quietMode)
{
 // Connect to Wi-Fi
  WiFi.begin(SSID, SSID_PASSWORD);
  while (!WiFi.isConnected()) 
  {
    delay(1500);
    if(!quietMode)
    {
      Serial.println("Connecting Wifi");
    }
  }
  if(WiFi.isConnected())
  {
    if(!quietMode)
    {
      Serial.println("Connected!");
      Serial.println(WiFi.localIP());
    }
    return(true);
  }
}

void getThought(bool firstRun)
{
  #ifdef DEBUG
    if(firstRun)
    {
      Serial.println("STARTUP: getting thought");
    }
    else
    {
      Serial.println("NON-STARTUP: getting thought");
    }
  #endif

  JSONVar jsonObj = null;

  if(WiFi.isConnected())
  {
    // Initialize the HTTPClient object
    HTTPClient http;
    tft.fillCircle(tft.width()-6,tft.height()-6,5,TFT_BLUE); // draw refresh indicator dot
    
    // Construct the URL using token from secrets.h  
    //this is weatherapi.com one day forecast request, which also returns location and current conditions
    // use zipcode if there is one, otherwise use public IP location 
    String url = "https://www.stands4.com/services/v2/quotes.php?uid="+(String)THOUGHT_USERID+"&tokenid="+(String)THOUGHT_TOKEN+"&format=json";
    
    // Make the HTTP GET request 
    http.begin(url);
    int httpCode = http.GET();

    String payload = "";
    // Check the return code
    if (httpCode == HTTP_CODE_OK) {
      // If the server responds with 200, return the payload
      payload = http.getString();
      #ifdef DEBUG
        Serial.println(payload);
      #endif
    } 
    else 
    {
      // For any other HTTP response code, print it
      #ifdef DEBUG
        Serial.println(F("Received unexpected HTTP response:"));
        Serial.println(httpCode);
      #endif
      http.end();
      // if there was an error just use a canned quote
      selectCannedThought();
      return;
    }
    // End the HTTP connection
    http.end();

    // if there was an error just use a canned quote
    if(payload.substring(2,7)=="error")
    {
      selectCannedThought();
      return; 
    }

    // Parse response
    jsonObj = JSON.parse(payload);
    // // Read values
    thought = (String)jsonObj["result"]["quote"];
    author = (String)jsonObj["result"]["author"];

    #ifdef DEBUG
      Serial.println("Thought refreshed");
    #endif

  }
  else if(!firstRun)
  {
    connectWifi(true);
  }
  tft.fillCircle(tft.width()-6,tft.height()-6,5,TFT_BLACK); // erase refresh indicator dot
}

void displayThought()
{
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1); 
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(ROBOTO22);

  // center thought lines vertically on the screen
  int thoughtStartY = (tft.height()/2)-((tft.fontHeight(GFXFF)*thoughtLines)/2);  
  //tft.setCursor(10, thoughtStartY);
  //tft.printf(thought.c_str());
  int authorY = thoughtStartY+(tft.fontHeight(GFXFF)*thoughtLines);

  tft.setViewport(5, thoughtStartY-17,314, (authorY-thoughtStartY)+20);
  tft.setCursor(0,20);
  tft.print(thought.c_str());
  //tft.setCursor(tft.width()/2, authorY);  // need to set this position before changing smaller font
  tft.setFreeFont(ROBOTO12);
  //tft.setCursor(305-tft.textWidth(author.c_str()), authorY);  // need to set this position before changing smaller font
  String paddedAuthor = author+"   ";
  tft.setCursor(tft.width()-tft.textWidth(paddedAuthor.c_str()), (authorY-thoughtStartY)+9);  // need to set this position before changing smaller font
  tft.setTextDatum(MR_DATUM);
  tft.print(paddedAuthor.c_str());

  tft.resetViewport();

  // draw filigree
  // top
  tft.drawFastHLine(40, thoughtStartY-30, tft.width()-80, TFT_DARKGREY);
  //tft.drawFastHLine(40, thoughtStartY-31, tft.width()-80, TFT_DARKGREY);
  tft.drawCircle(40, thoughtStartY-35, 3, TFT_DARKGREY);
  //tft.drawCircle(40, thoughtStartY-36, 4, TFT_DARKGREY);
  tft.drawCircle(tft.width()-40, thoughtStartY-35, 3, TFT_DARKGREY);
  //tft.drawCircle(tft.width()-40, thoughtStartY-36, 4, TFT_DARKGREY);
  tft.drawCircle(tft.width()/2, thoughtStartY-35, 3, TFT_DARKGREY);
  //tft.drawCircle(tft.width()/2, thoughtStartY-36, 4, TFT_DARKGREY);
  //bottom
  tft.drawFastHLine(40, authorY+20, tft.width()-80, TFT_DARKGREY);
  //tft.drawFastHLine(40, authorY+21, tft.width()-80, TFT_DARKGREY);
  tft.drawCircle(40, authorY+25, 3, TFT_DARKGREY);
  //tft.drawCircle(40, authorY+26, 4, TFT_DARKGREY);
  tft.drawCircle(tft.width()-40, authorY+25, 3, TFT_DARKGREY);
  //tft.drawCircle(tft.width()-40, authorY+26, 4, TFT_DARKGREY);
  tft.drawCircle(tft.width()/2, authorY+25, 3, TFT_DARKGREY);
  //tft.drawCircle(tft.width()/2, authorY+26, 4, TFT_DARKGREY);
}

String breakStringIntoLines(String item, bool countThoughtLines)
{
  const int lineSize = 24;
  const int forwardWindow = 4; // how many chars to look ahead to find a space to replace with \n
  const int backWindow = 10; // how many chars to look back to find a space if forwardWindow doesn't have one

  String thoughtWithBreaks = "";
  int pos = 0;
  
  if(countThoughtLines)
    thoughtLines = 1;  // used to position the author name in display

 //Serial.printf("Input length: %d\n",thought.length());

  // handle the easy case - no breaks needed
  if(item.length()<=lineSize)
  {
    return item;
  }
  else
  {
    pos = 0;

    while((item.length()-pos) > lineSize)
    {
      bool splitMade = false;
      pos += lineSize;
      for(int i=0; i<forwardWindow; i++)
      {
        //Serial.println("Searching forward");
        // look ahead within the window to find a space
        if(isSpace(item.charAt(pos+i)))
        {
          item.setCharAt(pos+i,'\n');
          //Serial.println("Sub made");
          splitMade = true;
          pos= (pos + i) + 1; // reset position to where we made the replacement. That's now the start of the next line. 
          if (countThoughtLines)
            thoughtLines+=1;
          break;
        }
      }
      if(!splitMade)
      {
        for(int i=0; i<backWindow; i++)
        {
          //Serial.println("Searching backward");
          if(isSpace(item.charAt(pos-i)))
          {
            item.setCharAt(pos-i,'\n');
            //Serial.println("sub made");
            splitMade = true;
            pos = (pos-i) + 1; // reset position to where we made the replacement. That's now the start of the next line.
            if(countThoughtLines)
              thoughtLines+=1;
            break;
          }
        }
      }
    }
    //Serial.println(pos);
    //Serial.println(lineSize);
    //Serial.println(thought.length());
  }
  return item;
}

void selectCannedThought()
{
  int randThought = random(sizeof(cannedThoughts)/sizeof(String));
  thought = cannedThoughts[randThought];
  author = cannedAuthors[randThought];
}

String getMonthDay()
{
  // return a string in the form "mm/dd" - two digits each month and year
  return "08/11";
}

// void getEvent(bool firstRun)
// {
//   #ifdef DEBUG
//     if(firstRun)
//     {
//       Serial.println("STARTUP: getting on this day");
//     }
//     else
//     {
//       Serial.println("NON-STARTUP: getting on this day");
//     }
//   #endif

//   String monthDay = getMonthDay();

//   JSONVar jsonObj = null;

//   if(WiFi.isConnected())
//   {
//     // Initialize the HTTPClient object
//     HTTPClient http;
//     tft.fillCircle(tft.width()-6,tft.height()-6,5,TFT_BLUE); // draw refresh indicator dot
    
//     // Construct the URL using token from secrets.h  
//     //this is weatherapi.com one day forecast request, which also returns location and current conditions
//     // use zipcode if there is one, otherwise use public IP location 
//     String url = "https://api.wikimedia.org/feed/v1/wikipedia/en/onthisday/selected/"+monthDay;
    
//     // Make the HTTP GET request 
//     http.begin(url);
//     int httpCode = http.GET();

//     String payload = "";
//     // Check the return code
//     if (httpCode == HTTP_CODE_OK) {
//       // If the server responds with 200, return the payload
//       payload = http.getString();
//       #ifdef DEBUG
//         Serial.println(payload);
//       #endif
//     } 
//     else 
//     {
//       // For any other HTTP response code, print it
//       #ifdef DEBUG
//         Serial.println(F("Received unexpected HTTP response:"));
//         Serial.println(httpCode);
//       #endif
//       http.end();
//       // if there was an error just use a placeholder
//       eventText = "A network connection issue prevented this device from getting a more interesting real event that had happened.";
//       eventYear = "Now";
//       return;
//     }
//     // End the HTTP connection
//     http.end();

//     // Parse response
//     jsonObj = JSON.parse(payload);

//     Serial.println(jsonObj["events"][0]);
//     Serial.println(jsonObj["events"][0][0]);
//     Serial.println(jsonObj["events"][0][0][0]);

//     objRec(jsonObj);

    
//     // TODO figure out how many events came back
//     // pick one at random
//     // check if it is too long and if it is, pick another event at random


//     // Read values
//     eventText = (String)jsonObj["result"]["quote"];
//     int eventYearNbr = (int)jsonObj["result"]["author"];
//     if (eventYearNbr <0)
//     {
//       eventYear = (String)abs(eventYearNbr)+"BC";
//     }
//     else 
//     {
//       eventYear = (String)eventYearNbr;
//     }

//     #ifdef DEBUG
//       Serial.println("On this day refreshed");
//     #endif
//   }
//   else if(!firstRun)
//   {
//     connectWifi(true);
//   }
//   tft.fillCircle(tft.width()-6,tft.height()-6,5,TFT_BLACK); // erase refresh indicator dot
// }
// void objRec(JSONVar jsonObj) {
//   Serial.println("{");
//   for (int x = 0; x < jsonObj.keys().length(); x++) {
//     if ((JSON.typeof(jsonObj[jsonObj.keys()[x]])).equals("object")) {
//       Serial.print(jsonObj.keys()[x]);
//       Serial.println(" : ");
//       objRec(jsonObj[jsonObj.keys()[x]]);
//     }
//     else {
//       Serial.print(jsonObj.keys()[x]);
//       Serial.print(" : ");
//       Serial.println(jsonObj[jsonObj.keys()[x]]);
//     }
//   }
//   Serial.println("}");
// }


void getDadJoke(bool firstRun)
{
  #ifdef DEBUG
  if(firstRun)
  {
    Serial.println("STARTUP: getting dad joke");
  }
  else
  {
    Serial.println("NON-STARTUP: getting dad joke");
  }
  #endif

  JSONVar jsonObj = null;

  if(WiFi.isConnected())
  {
    // Initialize the HTTPClient object
    HTTPClient http;
    tft.fillCircle(tft.width()-6,tft.height()-6,5,TFT_BLUE); // draw refresh indicator dot
    
    // Construct the URL using token from secrets.h  
    //this is weatherapi.com one day forecast request, which also returns location and current conditions
    // use zipcode if there is one, otherwise use public IP location 
    String url = "https://icanhazdadjoke.com/";
    
    //Make the HTTP GET request 
    http.begin(url);
    http.addHeader("accept","application/json");
    int httpCode = http.GET();

    String payload = "";
    // Check the return code
    if (httpCode == HTTP_CODE_OK) {
      // If the server responds with 200, return the payload
      payload = http.getString();
      #ifdef DEBUG
        Serial.println(payload);
      #endif
    } 
    else 
    {
      // For any other HTTP response code, print it
      #ifdef DEBUG
        Serial.println(F("Received unexpected HTTP response:"));
        Serial.println(httpCode);
      #endif
      http.end();
      // if there was an error just use a placeholder
      joke = "My friend told me that pepper is the best seasoning for a roast, but I took it with a grain of salt.";
    }
    // End the HTTP connection
    http.end();

   // Parse response
    jsonObj = JSON.parse(payload);
    joke = (String)jsonObj["joke"];
    // replace select unicode chars (leave the backslash)
    // u2018 = '
    joke.replace("\u2018","\'");
    // u2019 = '
    joke.replace("\u2019","\'");
    // u201c = "
    joke.replace("\u201c","\"");
    // u201d = "
    joke.replace("\u201d","\"");
    // change \n to space
    joke.replace("\n"," ");
    // remove \r
    joke.replace("\r", "");

    #ifdef DEBUG
      Serial.println("joke refreshed");
    #endif
  }
  else if(!firstRun)
  {
    connectWifi(true);
  }
  tft.fillCircle(tft.width()-6,tft.height()-6,5,TFT_BLACK); // erase refresh indicator dot
}

void displayDadJoke()
{
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1); 
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(ROBOTO22);
  drawArrayJpeg(tinydadjoke, sizeof(tinydadjoke), 5, 5); // icon in upper left
  drawArrayJpeg(tinydadjoke, sizeof(tinydadjoke), tft.width()-55, 5); // icon in upper right

  int jokeStartY = 60;

  tft.setViewport(5, jokeStartY-17,314, 180);
  tft.setCursor(0,40);
  tft.print(joke.c_str());
  tft.resetViewport();
}

void drawArrayJpeg(const uint8_t arrayname[], uint32_t array_size, int xpos, int ypos) {

  int x = xpos;
  int y = ypos;

  JpegDec.decodeArray(arrayname, array_size);
  renderJPEG(x, y);
  
}

void displayDaysToEvent(int eventType, int numDays, String anniversaryNbr)
{
  // 0 Christmas
  // 1 Halloween
  // 2 Anniversary
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1); 
  tft.setTextColor(TFT_WHITE);
  tft.drawString((String)numDays, 170, 75,7);
  tft.setFreeFont(ROBOTO22);
  tft.drawString("days until",180, 120,GFXFF);

  switch (eventType)
  {
    case 0: // christmas
      drawArrayJpeg(xmastree_100px, sizeof(xmastree_100px), 5, (tft.height()-100)/2); 
      tft.setTextColor(TFT_GREEN);
      tft.drawString("CHRISTMAS!!!",180, 150, GFXFF);
      break;
    case 1:
      drawArrayJpeg(halloween_100px, sizeof(halloween_100px), 5, ((tft.height()-50)/2)-40); 
      tft.setTextColor(TFT_ORANGE);
      tft.drawString("HALLOWEEN!!!",180, 150, GFXFF);
      break;
    case 2:
      drawArrayJpeg(hearts_100px, sizeof(hearts_100px), 5, ((tft.height()-50)/2)-20); 
      tft.setTextColor(TFT_PINK);
      tft.drawString(anniversaryNbr,180, 150, GFXFF);
      tft.drawString("ANNIVERSARY!!!",180, 180, GFXFF);
      break;
  }
}

void getTime(bool firstRun)
{
  #ifdef DEBUG
    if(firstRun)
    {
      Serial.println("STARTUP: getting time");
    }
    else
    {
      Serial.println("NON-STARTUP: getting time");
    }
  #endif
  if(WiFi.isConnected())
  {
    JSONVar jsonObj = null;

    // Initialize the HTTPClient object
    HTTPClient http;
    
    // Construct the URL using token from secrets.h  
    //this is WorldTimeAPI.org time request for current public IP
    String url = "https://worldtimeapi.org/api/ip";
    // Make the HTTP GET request 
    http.begin(url);
    int httpCode = http.GET();

    String payload = "";
    // Check the return code
    if (httpCode == HTTP_CODE_OK) {
      // If the server responds with 200, return the payload
      payload = http.getString();
    } else if (httpCode == HTTP_CODE_UNAUTHORIZED) {
      // If the server responds with 401, print an error message
      #ifdef DEBUG
        Serial.println(F("Time API Key error."));
        Serial.println(String(http.getString()));
      #endif
    } else {
      // For any other HTTP response code, print it
      #ifdef DEBUG
        Serial.println(F("Received unexpected HTTP response:"));
        Serial.println(httpCode);
      #endif
    }
    // End the HTTP connection
    http.end();

    // Parse response
    jsonObj = JSON.parse(payload);
    
    // get local time 
    const char* localTime = jsonObj["datetime"];

    #ifdef DEBUG
      Serial.println(payload);
      Serial.println(localTime);
    #endif
    parseTime(localTime);
    tft.fillScreen(TFT_BLACK);
    #ifdef DEBUG
      Serial.println("Time refreshed");
    #endif
  }
  else if(!firstRun)
  {
    connectWifi(true);
  }
}

void parseTime(const char* localTime)
{
  // parse the local time we got from the weather request for use by the time page (ignores second)
  // int tYr = (localTime.substring(0,4)).toInt();
  // int tMon = (localTime.substring(5,7)).toInt();
  // int tDay = (localTime.substring(8,10)).toInt();
  // int tHr = (localTime.substring(11,13)).toInt();
  // int tMin = (localTime.substring(14)).toInt();

  // time string looks like "2024-07-07T09:40:04.944551-05:00",
  // consts to avoid magic numbers
  const int startYr = 0;
  const int lenYr = 4;
  const int startMon = 5;
  const int lenMon = 2;
  const int startDay = 8;
  const int lenDay =  2;
  const int startHr = 11;
  const int lenHr = 2;
  const int startMin = 14;
  const int lenMin = 2;
  const int startSec = 17;
  const int lenSec = 2;
  
  // extract the year
  char cYr[lenYr+1];
  for (int i=startYr; i<lenYr; i++)
  {
    cYr[i]=localTime[i];
  }
  cYr[lenYr]='\0';
  int tYr;
  sscanf(cYr,"%d", &tYr);
  year = tYr;

  // extract the month
  char cMon[lenMon+1];
  for (int i=0; i<lenMon; i++)
  {
    cMon[i]=localTime[i+startMon];
  }
  cMon[lenMon]='\0';
  int tMon;
  sscanf(cMon,"%d", &tMon);
  month = tMon;
  // extract the day
  char cDay[lenDay+1];
  for (int i=0; i<lenDay; i++)
  {
    cDay[i]=localTime[i+startDay];
  }
  cDay[2]='\0';
  int tDay;
  sscanf(cDay,"%d", &tDay);
  day = tDay;

  // extract the hour
  char cHr[lenHr+1];
  for (int i=0; i<lenHr; i++)
  {
    cHr[i]=localTime[i+startHr];
  }
  cHr[lenHr]='\0';
  int tHr;
  sscanf(cHr,"%d", &tHr);

  // extract the minute
  char cMin[lenMin+1];
  for (int i=0; i<lenMin; i++)
  {
    cMin[i]=localTime[i+startMin];
  }
  cMin[lenMin]='\0';
  int tMin;
  sscanf(cMin,"%d", &tMin);

  // extract the minute
  char cSec[lenMin+1];
  for (int i=0; i<lenSec; i++)
  {
    cSec[i]=localTime[i+startSec];
  }
  cSec[lenSec]='\0';
  int tSec;
  sscanf(cSec,"%d", &tSec); 
}

String setYearOrdinal(int nbrYears)
{

  // Use this approach to add the st, nd, rd, th
    if ((nbrYears % 10 == 1) && (nbrYears % 100 != 11))
        return (String)nbrYears+"st";
    else if ((nbrYears % 10 == 2) && (nbrYears % 100 != 12))
        return (String)nbrYears+"nd";
    else if ((nbrYears % 10 == 3) && (nbrYears % 100 != 13))
        return (String)nbrYears+"rd";
    else
        return (String)nbrYears+"th";
}

int daysBetweenDateAndNow (int tYear, int tMonth, int tDay)
{
    struct tm tm1 = { 0 };
    struct tm tm2 = { 0 };

    /* date 1: today's date in year/month/day globals */
    tm1.tm_year = year - 1900;
    tm1.tm_mon = month - 1;
    tm1.tm_mday = day;
    tm1.tm_hour = tm1.tm_min = tm1.tm_sec = 0;
    tm1.tm_isdst = -1;

    /* date 2: the target date passed in  */
    tm2.tm_year = tYear - 1900;
    tm2.tm_mon = tMonth - 1;
    tm2.tm_mday = tDay;
    tm2.tm_hour = tm2.tm_min = tm2.tm_sec = 0;
    tm2.tm_isdst = -1;

    time_t t1 = mktime(&tm1);
    time_t t2 = mktime(&tm2);

    double dt = difftime(t2, t1);
    int days = round(dt / 86400);
  
    if(days<0)
    {
      // this means we are already past the date for this year and need to add one to the year and calc again. 
      tm2.tm_year = (tYear+1) - 1900;
      t2 = mktime(&tm2);
      double dt = difftime(t2, t1);
      days = (round(dt / 86400));
    }
    return days;
}

//####################################################################################################
// Draw a JPEG on the TFT, images will be cropped on the right/bottom sides if they do not fit
//####################################################################################################
// This function assumes xpos,ypos is a valid screen coordinate. For convenience images that do not
// fit totally on the screen are cropped to the nearest MCU size and may leave right/bottom borders.
void renderJPEG(int xpos, int ypos) {

  // retrieve information about the image
  uint16_t *pImg;
  uint16_t mcu_w = JpegDec.MCUWidth;
  uint16_t mcu_h = JpegDec.MCUHeight;
  uint32_t max_x = JpegDec.width;
  uint32_t max_y = JpegDec.height;

  // Jpeg images are draw as a set of image block (tiles) called Minimum Coding Units (MCUs)
  // Typically these MCUs are 16x16 pixel blocks
  // Determine the width and height of the right and bottom edge image blocks
  uint32_t min_w = minimum(mcu_w, max_x % mcu_w);
  uint32_t min_h = minimum(mcu_h, max_y % mcu_h);

  // save the current image block size
  uint32_t win_w = mcu_w;
  uint32_t win_h = mcu_h;

  // save the coordinate of the right and bottom edges to assist image cropping
  // to the screen size
  max_x += xpos;
  max_y += ypos;

  // read each MCU block until there are no more
  while (JpegDec.read()) {
	  
    // save a pointer to the image block
    pImg = JpegDec.pImage ;

    // calculate where the image block should be drawn on the screen
    int mcu_x = JpegDec.MCUx * mcu_w + xpos;  // Calculate coordinates of top left corner of current MCU
    int mcu_y = JpegDec.MCUy * mcu_h + ypos;

    // check if the image block size needs to be changed for the right edge
    if (mcu_x + mcu_w <= max_x) win_w = mcu_w;
    else win_w = min_w;

    // check if the image block size needs to be changed for the bottom edge
    if (mcu_y + mcu_h <= max_y) win_h = mcu_h;
    else win_h = min_h;

    // copy pixels into a contiguous block
    if (win_w != mcu_w)
    {
      uint16_t *cImg;
      int p = 0;
      cImg = pImg + win_w;
      for (int h = 1; h < win_h; h++)
      {
        p += mcu_w;
        for (int w = 0; w < win_w; w++)
        {
          *cImg = *(pImg + w + p);
          cImg++;
        }
      }
    }

    // calculate how many pixels must be drawn
    uint32_t mcu_pixels = win_w * win_h;

    tft.startWrite();

    // draw image MCU block only if it will fit on the screen
    if (( mcu_x + win_w ) <= tft.width() && ( mcu_y + win_h ) <= tft.height())
    {

      // Now set a MCU bounding window on the TFT to push pixels into (x, y, x + width - 1, y + height - 1)
      tft.setAddrWindow(mcu_x, mcu_y, win_w, win_h);

      // Write all MCU pixels to the TFT window
      while (mcu_pixels--) {
        // Push each pixel to the TFT MCU area
        tft.pushColor(*pImg++);
      }

    }
    else if ( (mcu_y + win_h) >= tft.height()) JpegDec.abort(); // Image has run off bottom of screen so abort decoding

    tft.endWrite();
  }
}
