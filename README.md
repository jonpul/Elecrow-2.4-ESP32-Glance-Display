# ESP32 Rectangular Glance Display
This is the companion project to https://github.com/jonpul/ESP32-Glance-Display and runs (as is) on the Elecrow 2.4" ESP32 WROOM device (https://www.amazon.com/dp/B0C8T6K21H). 
Select ESP32-WROOM-DA as the target ESP32 board in the Arduino IDE. 

I have integrated these into a USB power desk accessory (https://www.aliexpress.us/item/3256806558595584.html). 
I removed the front acrylic piece and substituted a blank piece of black acrylic with these two displays mounted side by side and powered inside the box. 

This project rotates through screens showing:
* Inspirational quotes from https://www.stands4.com/services/v2/quotes.php (requires sign up and a free API key)
* Dad jokes from https://icanhazdadjoke.com/
* Countdown to my anniversary (see yearMarried to set marriage year), halloween and christmas.

Tap and hold to refresh the thought or joke page with a new one. 
Tap to manually advance to the next page. 

The project demonstrates the following:
* REST API calls. See getTime, getDadJoke, getThought
* Breaking lines of text to fit display width. See breakStringIntoLines
* Very basic touch detection (this is a resistive touch display so not great for touch)
* Drawing bitmap images using TFT_eSPI. See displayDaysToEvent and displayDadJoke
* Number ordinal indicators (-th, -st, -rd) See setYearOrdinal
* Date math. See daysBetweenDateAndNow

The TFT_eSPI zip file in this repo is configured for this Elecrow board with the ILI934 driver and touch enabled. 
