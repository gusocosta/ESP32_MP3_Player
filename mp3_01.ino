#include <Arduino.h>
#include "bitmaps.h"
#include "read_sd.h"
#include <Wire.h>
#include <driver/uart.h>
#include <WiFi.h>

#include "AudioFileSourceSD.h"
#include "AudioFileSourceID3.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"
#include "AudioFileSourceBuffer.h"

#include <GyverOLED.h>
#include "OneButton.h"

#include "SPI.h"
#include "driver/gpio.h"

#define pot 0
const int BUTTON_ENTER = 8;
const int BUTTON_DOWN = 1;
const int BUTTON_UP = 2;
OneButton btn_enter(BUTTON_ENTER, true);
OneButton btn_down(BUTTON_DOWN, true);
OneButton btn_up(BUTTON_UP, true);

//Oled Display
#define I2C_SDA 10
#define I2C_SCL 9

//DAC
#define I2S_BCK 20
#define I2S_LRCK 21
#define I2S_DIN 3

//SD Module
#define PIN_MISO 5
#define PIN_MOSI 6
#define PIN_SCK 4
#define PIN_CS 7

//Choose your screen protocol
//GyverOLED<SSH1106_128x64> oled;
GyverOLED<SSD1306_128x64> oled;

AudioGeneratorMP3 *mp3 = nullptr;
AudioFileSourceSD *file = nullptr;
AudioFileSourceID3 *id3 = nullptr;
AudioOutputI2S *out = nullptr;
AudioFileSourceBuffer *buff = nullptr;

bool update_draw = true;
int vol = 25;  
uint8_t btn_timer = 0;
uint8_t btn_timer_max = 20;
uint8_t last_btn = 0;
state_struct state = state_load;

String currentFolderPath = "/";
int menu_scroll = 0;
int sel_index = 0;
int total_items = 0;
const int max_lines = 7;
bool can_check_btn = true;
int clickedItemIndex;

String track_title = "Carregando...";
String track_artist = "Desconhecido";
uint32_t track_duration = 0;
uint32_t track_current = 0;
float track_progress = 0.0;  
uint32_t start_time = 0;     
bool is_paused = false;
uint32_t pause_start_time = 0;

void check_pot(){
  const int NUM_SAMPLES = 16; 
  static int readings[NUM_SAMPLES] = {0};
  static int reading_index = 0;
  static long readings_sum = 0;
  static int last_stable_vol = -1;

  readings_sum -= readings[reading_index];                
  readings[reading_index] = analogRead(pot);              
  readings_sum += readings[reading_index];                
  reading_index = (reading_index + 1) % NUM_SAMPLES;      

  int average_raw = readings_sum / NUM_SAMPLES;
  int act_vol = map(average_raw, 100, 4000, 0, 100);
  if (act_vol < 0) act_vol = 0;
  if (act_vol > 100) act_vol = 100;

  if (abs(act_vol - last_stable_vol) >= 4) {
    vol = act_vol;
    last_stable_vol = act_vol;
    
    if (out) {
      out->SetGain((float)vol / 100.0);
    }
    
    update_draw = true; 
    
    Serial.print("[POT] Volume changed to: ");
    Serial.print(vol);
    Serial.println("%");
  }
}

void check_enter() {
  if (can_check_btn == true) {
    switch (state) {
      case state_dir:
        can_check_btn = false;
        clickedItemIndex = menu_scroll + sel_index;
        handleMenuSelection(currentFolderPath.c_str(), clickedItemIndex);
        update_draw = true;
      break;
      
      case state_player:
        player_toggle_pause();
        update_draw = true;
      break;
    }
    check_pot();
  }
}

void check_double_enter() {
  if (can_check_btn == true) {
    switch (state) {
      case state_dir:
        state = state_player;
        break;
      case state_player:

        state = state_dir;
        break;
    }
    player_toggle_pause();
    update_draw = true;
  }
}

void check_down() {
  if (can_check_btn == true) {
    switch (state) {
      case state_dir:
        if (sel_index < max_lines - 1 && menu_scroll + sel_index < total_items - 1) {
          sel_index += 1;
        } else {
          if (menu_scroll + sel_index < total_items - 1) {
            menu_scroll += 1;
          }
        }
        update_draw = true;
      break;
      case state_player:
        skip_track(-1);
      break;
    }
  }
}

void check_up() {
  if (can_check_btn == true) {
    switch (state) {
      case state_dir:
        if (sel_index > 0) {
          sel_index -= 1;
        } else {
          if (menu_scroll > 0) {
            menu_scroll--;
          }
        }
        update_draw = true;
      break;
      case state_player:
        skip_track(1);
      break;
    }
  }
}

// ============================================================================
// ================================== Audio ===================================
// ============================================================================

void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string) {
  String t = String(type);
  String v = String(string);
  
  v.trim(); 

  if (v.length() == 0) return; 

  Serial.print("[ID3] Tag encontrada -> ");
  Serial.print(t);
  Serial.print(": ");
  Serial.println(v);

  if (t.equalsIgnoreCase("TITLE")) {
    track_title = v;
    update_draw = true;
  } else if (t.equalsIgnoreCase("ARTIST") || t.equalsIgnoreCase("ALBUM ARTIST") || t.equalsIgnoreCase("PERFORMER")) {
    track_artist = v;
    update_draw = true;
  }
}

void player_init() {
  out = new AudioOutputI2S();
  out->SetPinout(I2S_BCK, I2S_LRCK, I2S_DIN);
  out->SetGain((float)vol / 100.0); 
}

void player_toggle_pause() {
  if (mp3 && mp3->isRunning()) {
    if (state == state_dir){
      is_paused = true;
    }
    else if (state == state_player){
      is_paused = !is_paused;
    }
    if (is_paused) {
      pause_start_time = millis();
      if (out) out->SetGain(0.0); 
      
    } else {
      start_time += (millis() - pause_start_time);
      if (out) out->SetGain((float)vol / 100.0);
    }
    
    update_draw = true;
  }
}

void player_start(const char *folderPath, const String &fileName) {
  if (out) out->SetGain(0.0);

  String fullPath = String(folderPath);
  if (!fullPath.endsWith("/")) fullPath += "/";
  fullPath += fileName;

  track_title = fileName;
  track_artist = "Desconhecido";
  track_current = 0;
  track_progress = 0.0;
  start_time = millis();
  is_paused = false;
  update_draw = true;

  Serial.print("[PLAYER] Iniciando: ");
  Serial.println(fullPath);

  if (mp3) {
    if (mp3->isRunning()) mp3->stop();
    delete mp3;
    mp3 = nullptr;
  }
  if (id3) {
    delete id3;
    id3 = nullptr;
  }
  if (buff) {
    delete buff;
    buff = nullptr;
  }
  if (file) {
    delete file;
    file = nullptr;
  }

  file = new AudioFileSourceSD(fullPath.c_str());
  buff = new AudioFileSourceBuffer(file, 8192);
  id3 = new AudioFileSourceID3(buff);
  id3->RegisterMetadataCB(MDCallback, (void *)"ID3TAG");

  mp3 = new AudioGeneratorMP3();
  mp3->begin(id3, out);
  if (out) out->SetGain((float)vol / 100.0);
}

void player_loop() {
  if (mp3 && mp3->isRunning()) {

    if (!mp3->loop()) {
      mp3->stop();
      skip_track(1);
    } else {
      uint32_t current_sec = (millis() - start_time) / 1000;
      float current_progress = 0.0;
      if (id3 && id3->getSize() > 0) {
        current_progress = (float)id3->getPos() / (float)id3->getSize();
      }

      static uint32_t last_sec = 999999;
      if (current_sec != last_sec) {
        track_current = current_sec;
        track_progress = current_progress;
        last_sec = current_sec;
        update_draw = true;
      }
    }
  }
}

// ============================================================================
// =================================== Draw ===================================
// ============================================================================

int center_text(const char *text) {
  return (127 - (int)strlen(text) * 6) / 2;
}

String formatTime(uint32_t seconds) {
  if (seconds == 0) return "00:00";
  uint32_t minutes = seconds / 60;
  uint32_t secs = seconds % 60;

  String timeStr = "";
  if (minutes < 10) timeStr += "0";
  timeStr += String(minutes) + ":";
  if (secs < 10) timeStr += "0";
  timeStr += String(secs);

  return timeStr;
}

void draw() {
  oled.clear();

  if (state == state_load) {
    oled.setCursor(25, 4);
    oled.print("Carregando...");
  }

  if (state == state_player) {
    oled.setScale(1);
    oled.setCursorXY(3, 2);
    oled.print("vol");
    int volbarWidth = 93;                                    // 117 - 24
    int volprogressX = 24 + (int)((vol * volbarWidth)/100);
    oled.rect(22,3, 119, 8, 2);
    oled.rect(24,5, volprogressX, 6, 1);


    oled.setCursor(center_text(track_artist.c_str()), 2);
    oled.print(track_artist);
    oled.setCursor(center_text(track_title.c_str()), 3);
    oled.print(track_title);

    oled.drawBitmap(23, 36, spr_back, 16, 16);
    if (is_paused) oled.drawBitmap(55, 36, spr_pause, 16, 16);
    else oled.drawBitmap(55, 36, spr_play, 16, 16);
    oled.drawBitmap(87, 36, spr_forw, 16, 16);

    oled.line(8, 56, 119, 56);
    oled.line(8, 54, 8, 58);
    oled.line(119, 54, 119, 58);

    int barWidth = 111;
    int progressX = 8 + (int)(track_progress * barWidth);

    oled.circle(progressX, 56, 4, 1);
    oled.circle(progressX, 56, 3, 0);
    oled.rect(0, 0, 127, 63, 2);
  } else if (state == state_dir) {
    displayFolderMenu(currentFolderPath.c_str(), menu_scroll, max_lines);
  }
  oled.update();
}

void setup() {
  Serial.begin(115200);
  delay(10);
  WiFi.mode(WIFI_OFF);
  btStop();
  setCpuFrequencyMhz(160);
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
  SD.begin(PIN_CS, SPI, 16000000);

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(800000);
  uart_driver_delete(UART_NUM_0);

  player_init();
  oled.init();
  oled.clear();

  btn_enter.attachClick(check_enter);
  btn_enter.attachLongPressStart(check_double_enter);
  btn_down.attachClick(check_down);
  btn_down.attachLongPressStart(check_down);
  btn_down.attachDuringLongPress(check_down);
  btn_up.attachClick(check_up);
  btn_up.attachLongPressStart(check_up);
  btn_up.attachDuringLongPress(check_up);

  btn_enter.setLongPressIntervalMs(600);
  btn_down.setLongPressIntervalMs(200);
  btn_up.setLongPressIntervalMs(200);

  gen_folder_signature(currentFolderPath.c_str());
  state = state_dir;
  draw();
}

void loop() {
  btn_down.tick();
  btn_up.tick();
  btn_enter.tick();
  check_pot();
  
  player_loop();

  if (update_draw) {
    draw();
    update_draw = false;
  }
}
