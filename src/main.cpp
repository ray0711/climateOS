
#include <Arduino.h>

#include <SoftwareSerial.h>
#include <Nextion.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <SNTPtime.h>
#include <secrets.h>

SNTPtime NTPch("pt.pool.ntp.org");
strDateTime dateTime;

#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BME280 bme; // I2C

const char *ssid = "WIFI_SSID";
const char *password = "WIFI_PASSWORD";
const char *host = "nextion";

// used only internally
int fileSize = 0;
bool result = true;
long dataMatrix[160][3];
bool enableDraw = false;

// displaying
SoftwareSerial nextionS(D4, D3);
Nextion myNextion(nextionS, 57600); //create a Nextion object named myNextion using the nextion serial port @ 9600bps

unsigned long lastUpdate = 0l;
unsigned long lastSnpTimeUpdate = 0l;
unsigned long lastUpdateTimeDisplay = 0l;

const uint32_t I2C_ACK_TIMEOUT = 2000;
String colors[3] = {"BLUE", "RED", "GREEN"};

void drawDataMatrix();
long currentDataSlot()
{
  long x = ((60 * dateTime.hour) + (dateTime.minute)) / 10;
  //long x = (4 * dateTime.hour) + (dateTime.minute / 15);
  Serial.println("Slot: " + String(x));
  return x;
}

void nextionStateListen()
{
  String message = myNextion.listen();
  if (message != "")
  {
    Serial.println(message);
    if (message == "70 age=gr")
    {
      enableDraw = true;
      drawDataMatrix();
    }
    else if (message == "70 age=m")
    {
      enableDraw = false;
    }
  }
}

void drawDataMatrix()
{
  Serial.println("Drawing graph");
  // initialize dataMatrix
  myNextion.sendCommand("fill 0,40,320,240,WHITE");
  for (int i = 0; i < 24; i++)
  {
    int pos = 4 + 13 * i;
    String cmd = "draw " + String(pos) + ",40," + String(pos) + ",240,GRAY";
    Serial.println(cmd.c_str());
    //myNextion.sendCommand(cmd.c_str());
  }

  long x = 8 +  currentDataSlot() * 2;
  String cmd1 = "draw " + String(x) + ",40," + String(x) + ",240,30495";
  Serial.println(cmd1.c_str());
  myNextion.sendCommand(cmd1.c_str());

  for (int i = 0; i < 160; i++)
  {
    for (int j = 0; j < 3; j++)
    {
      if (dataMatrix[i][j] > 0)
      {
        String color = colors[j];
        String cmd = "draw " + String((i * 2) + 8) +  "," + String((240 - dataMatrix[i][j])) + "," + String((i * 2) + 8 + 1) + "," + String((240 - dataMatrix[i][j]) + 1) + "," + colors[j];
        Serial.println(cmd.c_str());
        myNextion.sendCommand(cmd.c_str());
        yield();
        nextionStateListen();
        if (enableDraw == false)
        {
          return;
        }
      }
    }
  }
}

void printValues()
{
  Serial.print("Temperature = ");
  Serial.print(bme.readTemperature());
  Serial.println(" *C");

  Serial.print("Pressure = ");

  Serial.print(bme.readPressure() / 100.0F);
  Serial.println(" hPa");

  Serial.print("Approx. Altitude = ");
  Serial.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
  Serial.println(" m");

  Serial.print("Humidity = ");
  Serial.print(bme.readHumidity());
  Serial.println(" %");

  Serial.println();
  myNextion.setComponentText("page0.t0", (String) "" + bme.readTemperature());
  myNextion.setComponentText("page0.t3", (String) "" + bme.readHumidity());
  myNextion.setComponentText("page1.t1", (String) "" + WiFi.localIP().toString());

  // get time from internal clock
  // NTPch.printDateTime(dateTime);

  byte actualHour = dateTime.hour;
  byte actualMinute = dateTime.minute;
  byte actualsecond = dateTime.second;
  int actualyear = dateTime.year;
  byte actualMonth = dateTime.month;
  byte actualday = dateTime.day;
  byte actualdayofWeek = dateTime.dayofWeek;

  // if(myNextion.pageId()==1){
  //   drawDataMatrix();
  // }

  Serial.println("Valid: " + String(dateTime.valid));

  if (dateTime.valid)
  {
    //Serial.println("XXXXXXXXXXx: " );
    long x = currentDataSlot(); //(((4 * dateTime.hour) + (dateTime.minute / 15) + dateTime.second) % 160);
    // Serial.println(x);
    Serial.println("Slot: " + x);

    dataMatrix[x][0] = map(bme.readTemperature(), 0, 40, 0, 200);
    dataMatrix[x][1] = map(bme.readHumidity(), 0, 100, 0, 200);
    dataMatrix[x][2] = map(bme.readPressure() / 100.0F, 800, 1500, 0, 200);

    Serial.println(dataMatrix[x][0]);
    Serial.println(dataMatrix[x][1]);
    Serial.println(dataMatrix[x][2]);
  }
}

boolean setSnpTime()
{
  while (!NTPch.setSNTPtime())
  {
    Serial.print("."); // set internal clock
  }
  Serial.println();
  Serial.println("Time set");
  return true;
}

void setup(void)
{
  Serial.begin(115200);
  Serial.println("+++++++++++++ BOOT ++++++++++++++++++");
  bool status;

  // default settings
  // (you can also pass in a Wire library object like &Wire2)
  // SDA = 004 = D2
  // SCL = 005 = D1
  status = bme.begin(0x76);

  if (!status)
  {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
  }
  myNextion.init();
  //Serial.setDebugOutput(false);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Wifi connecting ");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  WiFi.setAutoReconnect(true);
  Serial.println();
  Serial.println("WiFi connected");
  setSnpTime();

  // initialize dataMatrix
  byte last = 120;
  for (int j = 0; j < 3; j++)
  {
    for (int i = 0; i < 160; i++)
    {
      randomSeed(0);
      dataMatrix[i][j] = 0;
      //   if (dataMatrix[i][j] < 1)
      //   {
      //     dataMatrix[i][j] = 0;
      //   }
      //   if (dataMatrix[i][j] > 200)
      //   {
      //     dataMatrix[i][j] = 120;
      //   }
      //   last = dataMatrix[i][j];
      //
    }
  }
}

void updateTimeDisplay()
{
  //Serial.println("update timedisplay");
  //NTPch.printDateTime(dateTime);
  char buffer[5] = "";
  sprintf(buffer, "%02d:%02d", dateTime.hour, dateTime.minute);
  myNextion.setComponentText("t7", buffer);
  //Serial.println(buffer);
}

void loop(void)
{
  if ((lastUpdate + 10000l) < millis())
  {
    printValues();
    lastUpdate = millis();
  }
  if (lastSnpTimeUpdate + 60 * 60 * 1000l < millis())
  {
    dateTime.valid = setSnpTime();
    lastSnpTimeUpdate = millis();
  }
  nextionStateListen();

  if (lastUpdateTimeDisplay + 1000l < millis())
  {
    dateTime = NTPch.getTime(0.0, 0);
    updateTimeDisplay();
    lastUpdateTimeDisplay = millis();
  }
}
