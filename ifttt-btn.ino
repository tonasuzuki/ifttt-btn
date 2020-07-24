//
// press the button, send an event message to IFTTT.
// (only ESP-WROOM-02)
//
// 2020/06/06 tonasuzuki
//
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoOTA.h>

//WLAN configuration.
const char ssid[] = "your wlan SSID";
const char pass[] = "youe wlan password";

//IFTTT trigger KEY
#define IFTTT_MAKER_KEY "insert your IFTTT maker key"
#define IFTTT_EVENT_NAME "esp-buttons%d"
//IFTTT Sever configuration.
#define HTTP_PORT 80
#define HTTP_HOST "maker.ifttt.com"

//GPIO configurations
//Number of buttons
#define BUTTON_NUM 3
// BUTTON-GPIO number (GPIO16 cannot be used)
int nButtonPins[] = {14, 12, 13};
// LED-GPIO number
#define LED_PIN 5


//to light-sleep after pressing the button (msec)
//if not light-sleep,Set to 0
#define BUTTON_SLEEP_TIME (300*1000)
//#define BUTTON_SLEEP_TIME 0

//WLAN connection retry-count
#define CONNECT_ERROR_TIME 20
//http post retry-count
#define HTTP_POST_RETRY 4

//
#define ERROR_LED_BLINK 10


#define HTTP_BUFFER_MAX  512
char szSendBuffer[HTTP_BUFFER_MAX];
char szURI[256];


//IFTTT trigger BUFFER
#define TRIGGER_BUFFER_SIZE 32
char szEventName[TRIGGER_BUFFER_SIZE];
char szTriggerValue1[TRIGGER_BUFFER_SIZE];
char szTriggerValue2[TRIGGER_BUFFER_SIZE];
char szTriggerValue3[TRIGGER_BUFFER_SIZE];

//sleep Wifi time
#if BUTTON_SLEEP_TIME
unsigned long lSleepTime;
unsigned long lNowTime;
#endif


void setup()
{
  int nPin;
  boolean bWifiConnect = false;

  // Open serial communications and wait for port to open:
  Serial.begin(9600);

  // initialize digital pin(LED_PIN) as an output.
  pinMode(LED_PIN, OUTPUT);
  // initialize the pushbutton pin as an input:
  for (nPin = 0; nPin< BUTTON_NUM; nPin++)
  {
    pinMode(nButtonPins[nPin], INPUT_PULLUP);
    Serial.printf(IFTTT_EVENT_NAME,nPin+1);
    Serial.printf(" : from GPIO %d \n",nButtonPins[nPin]);
  }

  //Connect wifi
  while (!bWifiConnect)
  { 
    bWifiConnect = ConnectWifi();
    delay(1000);
  }
  //
  ArduinoOTASetup();
  //
  szTriggerValue1[0]='\0';
  szTriggerValue2[0]='\0';
  szTriggerValue3[0]='\0';

#if BUTTON_SLEEP_TIME
  lNowTime=millis();
  lSleepTime=lNowTime+BUTTON_SLEEP_TIME ;
#endif
}

void loop()
{
  chkButton();
  ArduinoOTA.handle();
  //
#if BUTTON_SLEEP_TIME
  lNowTime=millis();
  if (lNowTime>lSleepTime)
  {
    //Sleep Board.
    ExecLightSleep();
    delay(50);
    lNowTime=millis();
    lSleepTime=lNowTime+BUTTON_SLEEP_TIME ;
  }
#endif
}

void chkButton()
{
  int nPin;
  int nButtonStatus=0;
	for (nPin = 0; (nPin< BUTTON_NUM); nPin++)
	{
		if (digitalRead(nButtonPins[nPin]) == LOW)
		{
		  nButtonStatus = nPin+1 ;
		}
	}
	if (nButtonStatus > 0)
	{
		PostHttpMessage(nButtonStatus);
	}
}

void PostHttpMessage(int nButton)
{
  int nResult=0;
  int nRetry=HTTP_POST_RETRY;
  boolean bWifiConnect = true;
	//Connect wifi
	if(WiFi.status() != WL_CONNECTED)
	{
		bWifiConnect=ConnectWifi();
	}
	if (bWifiConnect)
	{
	  sprintf(szEventName,IFTTT_EVENT_NAME,nButton);
	  while ( (nResult!=HTTP_CODE_OK) && (nRetry>0) ) 
	  {
	    // turn on LED :
	    digitalWrite(LED_PIN, HIGH);
	    // SEND IFTTT
	    nResult=SendTrigger( szEventName );
	    // turn off LED :
	    digitalWrite(LED_PIN, LOW);
      Serial.print(".");
	    delay(1000);
	    nRetry--;
	  }
	  if (nResult==HTTP_CODE_OK)
	  {
	    Serial.println(szEventName);
	  }
	  else
	  {
	    Serial.println("IFTTT connection error.\n");
	    ErrorLedBlink();
	  }
	}
	else
	{
	    Serial.println("WIFI connection error.\n");
	    ErrorLedBlink();
	}
	digitalWrite(LED_PIN, LOW);     
}


//
boolean ConnectWifi()
{
  boolean bResult= false;
  int nTime=CONNECT_ERROR_TIME;
  //
  WiFi.mode(WIFI_STA);
  //Connect wifi
  WiFi.begin(ssid, pass);
  while ( (WiFi.status() != WL_CONNECTED) && (nTime>=0) ) 
  {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
    Serial.print(".");
    nTime--;
  }
  if (nTime>=0)
  {
    bResult= true;
    Serial.println("ConnectWifi() done.");
    Serial.print("IP address: ");
    Serial.println( WiFi.localIP() );
  }
  else
  {
    Serial.println("ConnectWifi() Timeout.");    
  }
  return (bResult);
}

// 
boolean DisconnectWifi()
{
  boolean bResult= true;
  if (WiFi.status() == WL_CONNECTED)
  {
    bResult= false;
    WiFi.disconnect();
    int nTime=CONNECT_ERROR_TIME;
    while ( (WiFi.status() != WL_IDLE_STATUS) && (nTime>=0) ) 
    {
        Serial.print(".");
        nTime-=1;
        delay(200);
    }
    if (nTime>=0) bResult= true;
  }
  Serial.println("DisconnectWifi() done.");
  return (bResult);
}

//
void ErrorLedBlink()
{
  int n;
  for (n = 0; (n< ERROR_LED_BLINK); n++)
  {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
  }
}


//Send IFTTT event
int SendTrigger(char* pszEventName)
{
  int nHttpCode=false;
  HTTPClient http;

  //Create POST data
  sprintf(szSendBuffer,"{\"value1\":\"%s\",\"value2\":\"%s\",\"value3\":\"%s\"}"
          ,szTriggerValue1,szTriggerValue2,szTriggerValue3);
  //Create POST message
  sprintf(szURI,"/trigger/%s/with/key/%s"
          ,(char*)pszEventName,IFTTT_MAKER_KEY);
  // start connection and send HTTP header
  http.begin(HTTP_HOST,HTTP_PORT,szURI); //HTTP request
  http.addHeader("Content-Type","application/json");
  nHttpCode = http.POST((uint8_t *)szSendBuffer,strlen(szSendBuffer));
  if(nHttpCode) 
  {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] POST... code: %d\n", nHttpCode);
  } else {
      Serial.print("[HTTP] POST... failed, no connection or no HTTP server\n");
  }
  return (nHttpCode);
}


//
void FPMwakeupCallBack(void)
{
  Serial.println("FPMwakeupCallBack start");
  gpio_pin_wakeup_disable();
  wifi_fpm_close(); // disable force sleep function
}

//
void ExecLightSleep() {
  int nPin;
  Serial.println("ExecLightSleep() start.");
  DisconnectWifi();
  WiFi.mode(WIFI_OFF);
  wifi_set_opmode_current(NULL_MODE); 
  wifi_fpm_set_sleep_type(LIGHT_SLEEP_T); 
  wifi_fpm_open();
  for (nPin = 0; (nPin< BUTTON_NUM); nPin++)
  {
//    Serial.println("gpio_pin_wakeup_enable: %d",nButtonPins[nPin]);
    gpio_pin_wakeup_enable(nButtonPins[nPin], GPIO_PIN_INTR_LOLEVEL);
  }
  wifi_fpm_set_wakeup_cb(FPMwakeupCallBack); // Set wakeup callback
  wifi_fpm_do_sleep(0xFFFFFFF); // sleep until gpio activity.
  delay(10);
  Serial.println("ExecLightSleep() wake up.");
}

void ArduinoOTASetup()
{
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
    // NOTE: if updating FS this would be the place to unmount FS using FS.end(
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
    delay(1000);
    ErrorLedBlink();
    ESP.restart();
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
}
