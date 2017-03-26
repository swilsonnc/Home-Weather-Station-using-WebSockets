# Home-Weather-Station-Using-WebSockets
NodeMCU and BME280 serving temp, humidity, dew point, and pressure to a webpage using websockets.


## Description

A Nodemcu v1.0 and a BME280 serving sensor values to a webpage opened on a device via websockets.  The page is a simple layout that can be changed fairly easy to suit your needs.  I use it by displaying the page 24/7 on an old tablet sitting on my entertainment center beside my TV where it is easy to read from any seat.  Showing uptime of the NodeMCU is a plus so you can quickly see how long it's been running.  You can click on the forecast widget to get more info if you want to.  If you click and of the sensor reading words it will load a history pge showing thingspeak readings.

## Installation

Connection of the BME280 (not Adafruit) to the NodeMCU is as follows:

Vin --> 3v
Grd --> grd
SCL --> D1
SDA --> D2

Fill in your wifi and thingspeak information and upload the sketch.
Watch your serial readout for the ip address the device gets and load that in your browser making sure to add the port at the end (http://ip:8888)  If you want you can change the port to whatever you want.  You can also goto ip:8890/update to load a httpupdater page where you can upload a compiled bin to the device.  You can also upload a sketch using Arduino OTA.  You choose.

## What else

I also monitor the readings using a javascript page found here: http://community.thingspeak.com/forum/announcements/thingspeak-live-chart-multi-channel-second-axis-historical-data-csv-export/ for pulling and monitoring thingspeak data in a much better graph.  I created a simple app using the free MIT App Inventor 2 online at http://ai2.appinventor.mit.edu .  I created an android app with a simple webview component pointing to my ip:port and forwarded the port to my NodeMCU ip in my router.

## Legal

Use of this software is at your own risk. I will not be held responsible for any damages, direct or indirect, to any equipment or person/s who uses this software.

## Pictures

<img src="http://192.34.128.72/weather/weather.jpg">
