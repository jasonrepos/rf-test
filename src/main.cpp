// See github page : https://github.com/schreibfaul1/ESP32-audioI2S
// See https://how2electronics.com/esp32-music-audio-mp3-player/
 
#include <Arduino.h>
// #include "WiFi.h"
#include "Audio.h"
#include "SD.h"
#include "FS.h"
#include "RCSwitch.h"
// #include "Credentials.h"

// SPI - SD Card pins
#define SD_CS          5
#define SPI_MOSI      23    
#define SPI_MISO      19
#define SPI_SCK       18

// RF receiver
#define RF_RX         16
RCSwitch rfreceiver = RCSwitch();

// I2S - MAX98357A pins
#define I2S_DOUT      22
#define I2S_BCLK      14
#define I2S_LRC       15

Audio audio;
String file_list_butC[100]; // Will only allow 100 songs! This is a fixed size array.  Also see wavlist below.
int file_num_butC = 0;
int file_index_butC = 0;
int random_index_butC = 0;
String file_list_butD[100]; // Will only allow 100 songs! This is a fixed size array.  Also see wavlist below.
int file_num_butD = 0;
int file_index_butD = 0;
int random_index_butD = 0;
String filename;
int timeleft = 0;

// Diag onboard LED
#define ONBOARD_LED  2

struct Music_info
{
   String name;
   int length;
   int runtime;
   int volume;
   int timeleft;
   int status;
   int mute_volume;
} music_info = {"", 0, 0, 0, 0, 0};

const int Pin_next = 34;

int get_music_list(fs::FS &fs, const char *dirname, uint8_t levels, String wavlist[100])
{
    Serial.printf("Listing directory: %s\n", dirname);
    int i = 0;
 
    File root = fs.open(dirname);
    if (!root)
    {
        Serial.println("Failed to open directory");
        return i;
    }
    if (!root.isDirectory())
    {
        Serial.println("Not a directory");
        return i;
    }

    File file = root.openNextFile();
    while (file)
    {
        if (file.isDirectory())
        {
        }
        else
        {
            String temp = file.name();
            if (temp.endsWith(".wav"))
            {
                wavlist[i] = temp;
                i++;
            }
            else if (temp.endsWith(".mp3"))
            {
                wavlist[i] = temp;
                i++;
            }
        }
        file = root.openNextFile();
    }
    return i;
}

void open_new_song(String filename)
{
    music_info.name = filename.substring(1, filename.indexOf("."));
    audio.connecttoFS(SD, filename.c_str());
    music_info.runtime = audio.getAudioCurrentTime();
    music_info.length = audio.getAudioFileDuration();
    music_info.volume = audio.getVolume();
    //music_info.status = 1;
    Serial.println("**********start a new sound************");
    Serial.println(music_info.name);
}

void print_song_time()
{
    music_info.runtime = audio.getAudioCurrentTime();
    music_info.length = audio.getAudioFileDuration();
    music_info.timeleft = music_info.length - music_info.runtime;
    music_info.volume = audio.getVolume();
    Serial.println(music_info.runtime);
    Serial.println(music_info.length);
    Serial.println(music_info.timeleft);
    Serial.println("");
}

int song_time_left()
{
    timeleft = audio.getAudioFileDuration() - audio.getAudioCurrentTime();
    return timeleft;
}

uint button_time = 0;
int alarm_on = 1;
int no_repeat_delay = 1000;
int code_rec_time = 0;
uint run_time = 0;

// Define remote button codes
int buttonA = 7485602;
int buttonB = 7485608;
int buttonC = 7485601;
int buttonD = 7485604;
int lastpressed = 0;

// Create RTOS queue
struct audioMessage{
    uint8_t     cmd;
    const char* txt1;
    const char* txt2;
    const char* txt3;
    uint32_t    value1;
    uint32_t    value2;
    uint32_t    ret;
} audioTxMessage, audioRxMessage;

enum : uint8_t { NEXT_SONG, GET_VOLUME, CONNECTTOHOST, CONNECTTOFS, STOPSONG, SETTONE, INBUFF_FILLED, INBUFF_FREE,
                 ISRUNNING};

QueueHandle_t audioQueue = NULL;

void CreateQueues(){
    audioQueue = xQueueCreate(10, sizeof(struct audioMessage));
}

// Task: audioTask
void audioTask(void *parameters) {
  Serial.println("Executing audioTask");
  CreateQueues();

  struct audioMessage audioRxTaskMessage;

  // Audio(I2S) setup
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(14); // 0...21
  // Start a song
  open_new_song("/buttonC/" + file_list_butC[4]);
  //audio.connecttohost("http://192.168.1.141/xmas.mp3"); //  128k mp3
  // Loop forever
  while (1) {
    if(xQueueReceive(audioQueue, &audioRxTaskMessage, 1) == pdPASS) {
      Serial.println("Message received!");
      if(audioRxTaskMessage.cmd == NEXT_SONG){
        Serial.println(audioRxTaskMessage.txt1);
        audio.connecttoFS(SD, audioRxTaskMessage.txt1);
        //open_new_song("/buttonC/" + file_list_butC[13]);
      } else {
        Serial.println("Message was not matched!");
      }
    }
    audio.loop();
  }
}

void rfTask(void *parameters) {
  // audio.loop();
  rfreceiver.enableReceive(RF_RX);
  int alarm_on = 0;
  struct audioMessage audioTxTaskMessage;
  // Loop forever
  while (1) {

    // if (millis() - run_time > 1000)
    //   Serial.println("RFTask is running");
    //   {
    //       run_time = millis();
    //       // print_song_time();
    //       if ((lastpressed == buttonC) && (song_time_left() <= 0))
    //       {
    //           Serial.println("The song has finished, playing next track!");
    //           if (file_index_butC < file_num_butC - 1)
    //               file_index_butC++;
    //           else
    //               file_index_butC = 0;
    //           //open_new_song("/buttonC/" + file_list_butC[file_index_butC]);
    //       }
    //   }
      //if (millis() - button_time > 700) {
          //Button logic
          // if (digitalRead(Pin_next) == 1)
          if (rfreceiver.available()) {
              Serial.println("rfreceiver available");
              Serial.println(rfreceiver.getReceivedValue());
              // Button A logic
              if ((rfreceiver.getReceivedValue() == buttonA) && ((millis() - code_rec_time) > no_repeat_delay)) {
                  Serial.println(millis() - code_rec_time);
                  Serial.println(alarm_on);
                  digitalWrite(ONBOARD_LED,HIGH);
                  code_rec_time = millis();
                  if (alarm_on == 0) {
                      //open_new_song("/buttonA/on.mp3");
                      alarm_on = 1;
                      Serial.println(alarm_on);
                      filename = "/buttonA/on.mp3";
                      audioTxTaskMessage.cmd = NEXT_SONG;
                      audioTxTaskMessage.txt1 = filename.c_str();
                      xQueueSend(audioQueue, (void *)&audioTxTaskMessage, 10);
                  } else {
                      //open_new_song("/buttonA/off.mp3");
                      alarm_on = 0;
                      Serial.println(alarm_on);
                      filename = "/buttonA/off.mp3";
                      audioTxTaskMessage.cmd = NEXT_SONG;
                      audioTxTaskMessage.txt1 = filename.c_str();
                      xQueueSend(audioQueue, (void *)&audioTxTaskMessage, 10);
                  }
                  // print_song_time();
                  button_time = millis();
                  digitalWrite(ONBOARD_LED,LOW);
                  lastpressed = rfreceiver.getReceivedValue();
              }
              // Button B Logic (7485608)
              // Button C logic
              if ((rfreceiver.getReceivedValue() == buttonC) && ((millis() - code_rec_time) > no_repeat_delay)) {
                  Serial.println(millis() - code_rec_time);
                  code_rec_time = millis();
                  Serial.println("Pin_next");
                  // Play one song after the next
                  // if (file_index_butC < file_num_butC - 1)
                  //     file_index_butC++;
                  // else
                  //     file_index_butC = 0;
                  // open_new_song("/buttonC/" + file_list_butC[file_index_butC]);
                  // Randomise songs
                  random_index_butC = random(0, file_num_butC);
                  filename = "/buttonC/" + file_list_butC[random_index_butC];
                  audioTxTaskMessage.cmd = NEXT_SONG;
                  audioTxTaskMessage.txt1 = filename.c_str();
                  xQueueSend(audioQueue, (void *)&audioTxTaskMessage, 10);
                  //open_new_song("/buttonC/" + file_list_butC[random_index_butC]);

                  // print_song_time();
                  button_time = millis();
                  lastpressed = rfreceiver.getReceivedValue();
              }
              // Button D Logic (7485604)
              if ((rfreceiver.getReceivedValue() == buttonD) && ((millis() - code_rec_time) > no_repeat_delay)) {
                  Serial.println(millis() - code_rec_time);
                  code_rec_time = millis();
                  Serial.println("Pin_next");
                  // Play one song after the next
                  // if (file_index_butC < file_num_butC - 1)
                  //     file_index_butC++;
                  // else
                  //     file_index_butC = 0;
                  // open_new_song("/buttonC/" + file_list_butC[file_index_butC]);
                  // Randomise songs
                  random_index_butD = random(0, file_num_butD);
                  //open_new_song("/buttonD/" + file_list_butD[random_index_butD]);

                  // print_song_time();
                  button_time = millis();
                  lastpressed = rfreceiver.getReceivedValue();
              }

              rfreceiver.resetAvailable();
          }
     // }
  }
}   

void setup() {

    // Serial setup
    Serial.begin(115200);

    // SD card(SPI) setup
    pinMode(SD_CS, OUTPUT);      
    digitalWrite(SD_CS, HIGH);
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    if (!SD.begin(SD_CS, SPI))
    {
        Serial.println("Card Mount Failed");
    } else {
        Serial.println("Card Mounted Successfully");
    }

    // Read list of songs from SD card - buttonC
    file_num_butC = get_music_list(SD, "/buttonC", 0, file_list_butC);
    Serial.print("Music file count:");
    Serial.println(file_num_butC);
    Serial.println("All music:");
    for (int i = 0; i < file_num_butC; i++)
    {
        Serial.println(file_list_butC[i]);
    }

    // Read list of songs from SD card - buttonD
    file_num_butD = get_music_list(SD, "/buttonD", 0, file_list_butD);
    Serial.print("Music file count:");
    Serial.println(file_num_butD);
    Serial.println("All music:");
    for (int i = 0; i < file_num_butD; i++)
    {
        Serial.println(file_list_butD[i]);
    }

    // RF setup
    // rfreceiver.enableReceive(RF_RX);
    // int alarm_on = 0;

    //// WiFi setup
    // WiFi.disconnect();
    // WiFi.mode(WIFI_STA);
    // WiFi.begin(WIFI_SSID, WIFI_PW);
    // while (WiFi.status() != WL_CONNECTED) delay(1500);

    // Setup IO pins - currently using for testing before RF
    pinMode(Pin_next, INPUT);

    // Audio(I2S) setup
    // audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    // audio.setVolume(14); // 0...21
    //audio.connecttohost("http://mp3.ffh.de/radioffh/hqlivestream.mp3"); //  128k mp3
    //audio.connecttoFS(SD, "/buttonC/test.mp3");

    // FreeRTOS tasks
    // Start Serial receive task
    Serial.println("Starting audioTask...");
    xTaskCreatePinnedToCore(audioTask,
                            "Audio Task",
                            7000,
                            NULL,
                            3,
                            NULL,
                            1);

    Serial.println("Starting rfTask...");
    xTaskCreatePinnedToCore(rfTask,
                            "RF Task",
                            1024,
                            NULL,
                            0,
                            NULL,
                            0);

    // Diagnose with onboard LED
    pinMode(ONBOARD_LED,OUTPUT);

}

void loop()
{
  // if (millis() - run_time > 1000)
  //     {
  //         run_time = millis();
  //         print_song_time();
  //         if ((lastpressed == buttonC) && (song_time_left() <= 0))
  //         {
  //             Serial.println("The song has finished, playing next track!");
  //             if (file_index_butC < file_num_butC - 1)
  //                 file_index_butC++;
  //             else
  //                 file_index_butC = 0;
  //             open_new_song("/buttonC/" + file_list_butC[file_index_butC]);
  //         }
  //     }
  //     if (millis() - button_time > 700) {
  //         Button logic
  //         if (digitalRead(Pin_next) == 1)
  //         if (rfreceiver.available()) {
  //             Serial.println("rfreceiver available");
  //             Serial.println(rfreceiver.getReceivedValue());
  //             Button A logic
  //             if ((rfreceiver.getReceivedValue() == buttonA) && ((millis() - code_rec_time) > no_repeat_delay)) {
  //                 Serial.println(millis() - code_rec_time);
  //                 Serial.println(alarm_on);
  //                 digitalWrite(ONBOARD_LED,HIGH);
  //                 code_rec_time = millis();
  //                 if (alarm_on == 0) {
  //                     open_new_song("/buttonA/on.mp3");
  //                     alarm_on = 1;
  //                     Serial.println(alarm_on);
  //                 } else {
  //                     open_new_song("/buttonA/off.mp3");
  //                     alarm_on = 0;
  //                     Serial.println(alarm_on);
  //                 }
  //                 print_song_time();
  //                 button_time = millis();
  //                 digitalWrite(ONBOARD_LED,LOW);
  //                 lastpressed = rfreceiver.getReceivedValue();
  //             }
  //             Button B Logic (7485608)
  //             Button C logic
  //             if ((rfreceiver.getReceivedValue() == buttonC) && ((millis() - code_rec_time) > no_repeat_delay)) {
  //                 Serial.println(millis() - code_rec_time);
  //                 code_rec_time = millis();
  //                 Serial.println("Pin_next");
  //                 Play one song after the next
  //                 if (file_index_butC < file_num_butC - 1)
  //                     file_index_butC++;
  //                 else
  //                     file_index_butC = 0;
  //                 open_new_song("/buttonC/" + file_list_butC[file_index_butC]);
  //                 Randomise songs
  //                 random_index_butC = random(0, file_num_butC);
  //                 open_new_song("/buttonC/" + file_list_butC[random_index_butC]);

  //                 print_song_time();
  //                 button_time = millis();
  //                 lastpressed = rfreceiver.getReceivedValue();
  //             }
  //             Button D Logic (7485604)
  //             if ((rfreceiver.getReceivedValue() == buttonD) && ((millis() - code_rec_time) > no_repeat_delay)) {
  //                 Serial.println(millis() - code_rec_time);
  //                 code_rec_time = millis();
  //                 Serial.println("Pin_next");
  //                 Play one song after the next
  //                 if (file_index_butC < file_num_butC - 1)
  //                     file_index_butC++;
  //                 else
  //                     file_index_butC = 0;
  //                 open_new_song("/buttonC/" + file_list_butC[file_index_butC]);
  //                 Randomise songs
  //                 random_index_butD = random(0, file_num_butD);
  //                 open_new_song("/buttonD/" + file_list_butD[random_index_butD]);

  //                 print_song_time();
  //                 button_time = millis();
  //                 lastpressed = rfreceiver.getReceivedValue();
  //             }

  //             rfreceiver.resetAvailable();
  //         }
  //     }  
}


// optional
// void audio_info(const char *info){
//     Serial.print("info        "); Serial.println(info);
// }
// void audio_id3data(const char *info){  //id3 metadata
//     Serial.print("id3data     ");Serial.println(info);
// }
// void audio_eof_mp3(const char *info){  //end of file
//     Serial.print("eof_mp3     ");Serial.println(info);
// }
// void audio_showstation(const char *info){
//     Serial.print("station     ");Serial.println(info);
// }
// void audio_showstreamtitle(const char *info){
//     Serial.print("streamtitle ");Serial.println(info);
// }
// void audio_bitrate(const char *info){
//     Serial.print("bitrate     ");Serial.println(info);
// }
// void audio_commercial(const char *info){  //duration in sec
//     Serial.print("commercial  ");Serial.println(info);
// }
// void audio_icyurl(const char *info){  //homepage
//     Serial.print("icyurl      ");Serial.println(info);
// }
// void audio_lasthost(const char *info){  //stream URL played
//     Serial.print("lasthost    ");Serial.println(info);
// }
// void audio_eof_speech(const char *info){
//     Serial.print("eof_speech  ");Serial.println(info);
// }