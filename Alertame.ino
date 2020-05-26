/* Adaptado de el ejemplo original por Cesar Riojas entre otras miles de cosas
    www.arduinocentermx.blogspot.mx
    Autoconnect: https://www.hackster.io/hieromon-ikasamo/esp8266-esp32-connect-wifi-made-easy-d75f45
 */
#define BOT_AND_WIFI
#define AUTOCONNECT
#define ALERT_DEBUG

#include "git-version.h"
#include <ESP8266WiFi.h>
#include <UniversalTelegramBot.h>
#include <ArduinoOTA.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClientSecure.h>
#ifdef AUTOCONNECT
#include <AutoConnect.h>
AutoConnect portal;
#endif

// All Configuration options

#define MY_NAME     "Caldera0"
#define TelegramBotToken "648272766:AAEkW5FaFMeHqWwuNBsZJckFEOdhlSVisEc"
String default_chat_id = "25235518"; // gera
// String default_chat_id = "268186747"; // Agu

#ifdef ALERT_DEBUG
#define DPRINTLN(X)  debug_log(X);
#define DPRINT(X)    debug_log(X, false);
#else
#define DPRINTLN(X)
#define DPRINT(X)
#endif 

bool polarity_inverted = true;

// int digitalInputPin = 12;  // GPIO12 - NodemCU D6
int digitalInputPin = 5;  // GPIO5 - Relay module - optocoupled input
int digitalOutputPin = 4; // GPIO4 - Relay module - relay control

#ifndef AUTOCONNECT
char WiFi_ssid[] = "Your Fixed ESSID";
char WiFi_key[] = "Your Password";

#endif // AUTOCONNECT

//////////////////

void relay_set(int value);
void cmd_status(String &chat_id);

bool WiFi_ok = false;

bool input_status;
int relay_state;

WiFiClientSecure client;

void debug_log(String &msg, bool ln = true) {   
  String line("[");

  line += millis()/1000;
  line += "] ";
  line += msg;
  if (ln) Serial.println(line);
  else Serial.print(line);
}

void WiFi_setup() {
#ifdef AUTOCONNECT
  AutoConnectConfig portalConfig(MY_NAME, MY_NAME "Password2020!");
  portalConfig.ota = AC_OTA_BUILTIN;
  portal.config(portalConfig);
  if (portal.begin()) {
    Serial.println("connected:" + WiFi.SSID());
    Serial.println("IP:" + WiFi.localIP().toString());
  } else {
    Serial.println("connection failed:");
  }
#else // not AUTOCONNECT
  // Establecer el modo WiFi y desconectarse de un AP si fue Anteriormente conectada
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  Serial.println(WiFi_ssid);
  WiFi.begin(WiFi_ssid, WiFi_key);
#endif // AUTOCONNECT

  WiFi_ok = WiFi.status() == WL_CONNECTED;
}

void WiFi_loop() {
#ifdef AUTOCONNECT
  portal.handleClient();
#else // not AUTOCONNECT
  static long lastCheck = 0;
  bool next_ok;

  if (millis() - lastCheck > 500) {
    next_ok = WiFi.status() == WL_CONNECTED;

    if (!next_ok) {
      DPRINT(".");
    }

    if (next_ok && !WiFi_ok) {
      Serial.println("Connected!");
    }
    WiFi_ok = next_ok;
    lastCheck = millis();
  }
#endif // AUTOCONNECT
}

// Blinker
bool blinking = true;

void cmd_blink() {
  blinking = true;
  pinMode(1, OUTPUT);
}

void cmd_unblink() {
  blinking = false;
  Serial.begin(115200, SERIAL_8N1);
}

void blink_setup() {
  cmd_unblink();
  Serial.println("I'm " MY_NAME);
}

void blink_loop() {
  static int value = 0;

  if (!blinking) return;

  value++;
  if (value == 1000) value = -1000;
  analogWrite(1, abs(value));
  delay(1);
  Serial.println(value);
}

void cmd_polarity(String &chat_id) {
  polarity_inverted = !polarity_inverted;
  cmd_status(chat_id);
}

// Telegram Bot

UniversalTelegramBot bot(TelegramBotToken, client);

int Bot_mtbs_ms = 3000;
long Bot_nexttime = 0;
bool Bot_greeted = false;

// General

void send_message(String &chat_id, String &text) {
  String msg = F("*" MY_NAME "*: ");
  
  if (chat_id == "") chat_id = default_chat_id;
  
  msg += text;
  debug_log(msg);
  bot.sendMessage(chat_id, msg, "Markdown");
}

bool is_for_me(int message_index) {
  DPRINTLN(String("reply to: ") + bot.messages[message_index].reply_to_message_id + " text: " + bot.messages[message_index].reply_to_text);

  // Only accept answers to other messages
  // if (0 == bot.messages[message_index].reply_to_message_id) return false;

  // Only accept answers to my messages
  return bot.messages[message_index].reply_to_text.startsWith(MY_NAME ":");
}

// Greeter
void cmd_start(String &chat_id) {
  String welcome = "Hola, your `chat_id` is " + chat_id;

  default_chat_id = chat_id;
  send_message(chat_id, welcome);
  cmd_status(chat_id);

  /*
  DynamicJsonDocument doc(200);
  deserializeJson(doc, "}{\"ok\":true, \"nok\":false}");

  send_message(chat_id, String("ok: ")    + (bool)doc["ok"] +           " nok: "  + (bool)doc["mok"] +
                 " mokt: " + (bool)(doc["mok"] | true) + " mokf: " + (bool)(doc["mok"] | false)); 
  */
}

void cmd_help(String &chat_id) {
  String help = F("\n" GIT_VERSION "\n"
    "`status` shows all status\n"
    "`polarity` changes input polarity\n"
    "`start` registers who will receive alerts\n"
    "`ron` turns on Relay\n"
    "`roff` turns off Relay\n"
    "`ronoff` turns on Relay then off\n"
    "`roffon` turns off Relay then on\n"
    "`reset` resets the system\n");

  send_message(chat_id, help);
}

void cmd_status(String &chat_id) {
  String in_st  = input_status?"Ok":"ALARMA!";
  String rel_st = relay_state?"On":"Off";
  String pol = polarity_inverted?"-":"+";
  
  String msg = F("status: *");
  msg += in_st;
  msg += F("* relay: *");
  msg += rel_st;
  msg += F("* polarity: *");
  msg += pol;
  msg += F("*");
  
  send_message(chat_id, msg);
}

void cmd_sysinfo(String &chat_id) {

  String msg = F("\nIP: *");
  msg += WiFi.localIP().toString();
  msg += F("*\nESSID: *");
  msg += WiFi.SSID();
  msg += F("*\nSignal: *");
  msg += WiFi.RSSI();
  msg += F(" dBm*\nRAM: *");
  msg += ESP.getFreeHeap();
  msg += F("*\nuptime: *");
  msg += millis()/1000;
  msg += F("*\nversion: *" GIT_VERSION "*");
  // msg += F("*");
  
  send_message(chat_id, msg);
}

void cmd_keyboard(String &chat_id) {
  String keyboard = F("["
     "[{\"text\":\"sysinfo\",\"callback_data\":\"sysinfo\"},"
     "{\"text\":\"status\",\"callback_data\":\"status\"},"
     "{\"text\":\"polarity\",\"callback_data\":\"polarity\"}],"
     "[{\"text\":\"ron\",\"callback_data\":\"ron\"},"
     "{\"text\":\"roff\",\"callback_data\":\"roff\"}],"
     "[{\"text\":\"ronoff\",\"callback_data\":\"ronoff\"},"
     "{\"text\":\"roffon\",\"callback_data\":\"roffon\"}]"
     "]}"
  );
  bot.sendMessageWithInlineKeyboard(chat_id, F("*" MY_NAME "*: use the keyboard"), "Markdown", keyboard); //, false, true, false);
}

void cmd_relay_set(String &chat_id, int first_state, int second_state) {
   relay_set(first_state);
   if (second_state != -1) {
     delay(1000);
     relay_set(second_state);
   }
   cmd_status(chat_id);
}

void cmd_sent_file(int i) {
  DPRINTLN((String("Received document: ") + bot.messages[i].file_caption + " size: " + bot.messages[i].file_size));
  DPRINTLN((String("URL: ") + bot.messages[i].file_path));
  t_httpUpdate_return ret = ESPhttpUpdate.update(bot.messages[i].file_path);
  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("HTTP_UPDATE_NO_UPDATES");
      break;
    case HTTP_UPDATE_OK:
      Serial.println("HTTP_UPDATE_OK");
      break;
  }
}

void delay_next_poll() {
  Bot_nexttime += Bot_mtbs_ms;
}

void Bot_handleNewMessages(int numNewMessages) {
  static bool firstMsg = true;
  bool message_for_other_device = false;
  
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String cmd = bot.messages[i].text;

    cmd.toLowerCase();
    if (cmd[0] == '/') cmd.remove(0,1);

    debug_log("Received \"" + cmd + "\" from " + chat_id);

    // Global messages (acceptable for all devices at the same time, without any filtering)
    if (cmd == "start") {
      cmd_start(chat_id);
      message_for_other_device = true;
    }
    else if (cmd == "allstatus") {
      cmd_status(chat_id);
      message_for_other_device = true;
    }
    else if (cmd == "allsysinfo") {
      cmd_sysinfo(chat_id);
      message_for_other_device = true;
    }
    
    // Device commands (only acceptable if directed to a particular device)
    else if (is_for_me(i)) {
      if      (cmd == "status") cmd_status(chat_id);
      else if (cmd == "polarity") cmd_polarity(chat_id);
      else if (cmd == "ron") cmd_relay_set(chat_id, 1, -1);
      else if (cmd == "roff") cmd_relay_set(chat_id, 0, -1);
      else if (cmd == "ronoff") cmd_relay_set(chat_id, 1, 0);
      else if (cmd == "roffon") cmd_relay_set(chat_id, 0, 1);
      else if (cmd == "roffon") cmd_relay_set(chat_id, 0, 1);
      else if (cmd == "sysinfo") cmd_sysinfo(chat_id);
      else if (cmd == "keyboard") cmd_keyboard(chat_id);
      else if (cmd == "reset") {
        if (!firstMsg) ESP.reset();
      }
      else if (bot.messages[i].hasDocument) cmd_sent_file(i);
      else cmd_help(chat_id);
    } else {
      message_for_other_device = true;
    }
    
    firstMsg = false;
  }
  if (message_for_other_device) delay_next_poll();
}

void Bot_setup() {
  client.setInsecure();
  Bot_nexttime = millis() - 1;
  bot.longPoll = 1;
}

void Bot_first_time() {
  String commands = F("["
    "{\"command\":\"help\",  \"description\":\"Get bot usage help\"},"
    "{\"command\":\"reset\", \"description\":\"reset device\"},"
    "{\"command\":\"start\", \"description\":\"register with (all) devices as their user\"},"
    "{\"command\":\"allstatus\",\"description\":\"answer all devices current status\"},"
    "{\"command\":\"status\",\"description\":\"answer device current status\"},"
    "{\"command\":\"polarity\",\"description\":\"changes input polarity\"},"
    "{\"command\":\"ron\",\"description\":\"turn relay on\"},"
    "{\"command\":\"roff\",\"description\":\"turn relay off\"},"
    "{\"command\":\"ronoff\",\"description\":\"turn relay on then off\"},"
    "{\"command\":\"roffon\",\"description\":\"turn relay off then on\"},"
    "{\"command\":\"allsysinfo\",\"description\":\"answer all devices system info\"},"
    "{\"command\":\"sysinfo\",\"description\":\"answer device system info\"}"
  "]");

  bot.setMyCommands(commands);
  
  if (default_chat_id != "") {
    cmd_start(default_chat_id);
  }
}

void Bot_loop() {
  if (!WiFi_ok) return;
  if (!Bot_greeted) {
    Bot_greeted = true;
    Bot_first_time();
  }
  if (millis() > Bot_nexttime)  {
    Bot_nexttime = millis() + Bot_mtbs_ms;

    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    DPRINTLN((String("numNewMessages: ") + numNewMessages + " last: " + bot.last_message_received) + " mem: " + ESP.getFreeHeap());
    Bot_handleNewMessages(numNewMessages);
  }
}

///////////////////////

bool input_read() {
  return digitalRead(digitalInputPin) ^ polarity_inverted;
}

void input_setup() {
  pinMode(digitalInputPin, INPUT_PULLUP);
  input_status = input_read();
}

void input_loop() {
  bool new_status = input_read();
  
  if (input_status != new_status) {
      input_status = new_status;
#ifdef BOT_AND_WIFI
      cmd_status(default_chat_id);
#endif
  }
}

//// OTA

void OTA_setup() {
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(MY_NAME);
  ArduinoOTA.setPasswordHash("fff9a16a632c7daa86f7a4a8ce1929d6");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
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

void OTA_loop() {
  ArduinoOTA.handle();
}

/////////// Relay

void relay_setup() {
   pinMode(digitalOutputPin, OUTPUT);
   relay_set(0);
}

void relay_set(int value) {
   relay_state = value;
   digitalWrite(digitalOutputPin, value);
}

///////////////////////

void setup() {
  blink_setup();
#ifdef BOT_AND_WIFI
  WiFi_setup();
  OTA_setup();
  Bot_setup();
#endif
  input_setup();
  relay_setup();
}

void loop() {
#ifdef BOT_AND_WIFI
  WiFi_loop();
  OTA_loop();
  Bot_loop();
#endif
  // blink_loop();
  input_loop();
}
