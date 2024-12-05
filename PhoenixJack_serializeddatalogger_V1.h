#include "pico/platform/compiler.h"
#include "pico/rand.h"
/*
TARGET HARDWARE: RASPBERRY PI PICO W
AUTHOR: CHRIS MCCREARY
VERSION: 0.0.1
DATE: 2024-11-29

|GPIO|PIN#| FUNCTION | - |     FUNCTION |PIN#|GPIO|
| -- | -  | -------- | - |     -------- | -- | -- |
|  0 |  1 |          |   |         VBUS | 40 | -- |
|  1 |  2 |          |   |         VSYS | 39 | -- |
| -- |  3 | GND      |   |          GND | 38 | -- |
|  2 |  4 |          |   |       3V3_EN | 37 | -- |
|  3 |  5 |          |   |          3V3 | 36 | -- |
|  4 |  6 |          |   |     ADC_VREF | 35 | -- |
|  5 |  7 |          |   |              | 34 | 28 |
| -- |  8 | GND      |   |          GND | 33 | -- |
|  6 |  9 |          |   |              | 32 | 27 |
|  7 | 10 |          |   |              | 31 | 26 |
|  8 | 11 |          |   |        RESET | 30 | -- |
|  9 | 12 |          |   |              | 29 | 22 |
| -- | 13 | GND      |   |          GND | 28 | -- |
| 10 | 14 |          |   |              | 27 | 21 |
| 11 | 15 |          |   |              | 26 | 20 |
| 12 | 16 |          |   |    SPI0:MOSI | 25 | 19 |
| 13 | 17 |          |   |     SPI0:SCK | 24 | 18 |
| -- | 18 | GND      |   |          GND | 23 | -- |
| 14 | 19 |          |   |      SPI0:CS | 22 | 17 |
| 15 | 20 |          |   |    SPI0:MISO | 21 | 16 |

KEY NOTES:
For saving to SD card, filenames are limited (by the underlying library) to 8+3 in length.
To keep session names unique, a session is created with a random 16 bit / 2 byte / 4 hex character ID (0x0000-0xFFFF).
Session IDs will serve as the prefix for each session.
Each session will hold an index file (XXXXindx.txt) and timestamp file (XXXXtime.txt).
  The index file will hold any header info.
  The timestamp file will only be created once we have a valid epoch timestamp (called by mark_the_time()).
Each data file will start with the session ID, and be suffixed with a 16 bit sequence number, for a maximum of 65,535 data files per session.
Each data file has an arbitrary limit of 100Kb before it starts a new data file.
Assuming an average data entry size of ~160bytes, this should give us:
  ~640 entries per data file
  ~41,942,400 entries per session
Assuming a data entry once per second, this should give us:
  ~699,040 minutes / 11,650 hours / 485 days of runtime
Total size for 1 maximum session
  DATA FILES ~= 6,710,784,000 bytes / 6,399 MB / 6.4 GB
  INDEX FILE ~= 45 bytes + num_data_files (max 4095) * 23 = 94230 bytes / 92 MB
  TIME FILE ~= 54 bytes * number of time synchronizations
THEORETICALLY! Theoretically, I should be dating Jessica Alba.
*/

#include "hardware/address_mapped.h"
#define PIN_SD_MOSI PIN_SPI0_MOSI
#define PIN_SD_MISO PIN_SPI0_MISO
#define PIN_SD_SCK PIN_SPI0_SCK
#define PIN_SD_SS PIN_SPI0_SS

#include <SPI.h>
#include <RP2040_SD.h>  // https://github.com/khoih-prog/RP2040_SD

Sd2Card card;
SdVolume volume;
File root;
File myFile;

struct sd {
public:                                            // ERROR LEVELS
  enum SD_STATUS_CODE {                            // TODO: split to a LASTACTION and READINESS CODE
    SD_READY,                                      //
    SD_INITIALIZED,                                //
    SD_CANNOT_INITIALIZE,                          //
    SD_NOT_FORMATTED,                              //
    SD_FULL,                                       //
    SD_INDEX_FILE_WRITTEN,                         //
    SD_INDEX_UPDATED,                              //
    SD_COULD_NOT_OPEN_DATA_FILE,                   //
    SD_COULD_NOT_CREATE_NEW_DATA_FILE,             //
    SD_INDEX_NOT_UPDATED,                          //
    SD_OPENED_FOR_WRITE,                           //
    SD_UNABLE_OPEN_FOR_WRITE,                      //
    SD_UNABLE_CLOSE_FILE,                          //
    SD_FILE_NOT_EXIST,                             //
    SD_END_OF_LIST                                 //
  };                                               //
  SD_STATUS_CODE current_status = SD_READY;        // give outside functions a known attribute to check; TODO: change to private and use isReady() as a getter func
  bool isReady();                                  // checks that current_status is SD_READY or SD_INITIALIZED
private:                                           // CARD & SESSION SETUP
                                                   // CARD INFO
  uint32_t _volume_size_kb = 0;                    // total capacity in KB
  uint32_t _volume_used_kb = 0;                    // amount of used space in KB
  uint32_t _volume_free_kb = 0;                    // amount of free space in KB
  uint16_t _directory_count = 0;                   //
  uint16_t _file_count = 0;                        //
  bool _start_up();                                // returns false if unable to reach SD reader OR volume isn't formatted as FAT; populates _volume_size_kb AND calls _get_card_space() to get used/free space & file/dir count
  bool _initialize();                              //
  bool _mount();                                   //
  float _get_card_size_GB();                       // simple conversion: KB to GB
  bool _get_card_space();                          // gets used/free space & file/dir count
  void _get_used_size(File dir);                   // recursively calls itself as it works through directories
  bool _doesFileExist(const char* fileNameInput);  // internal handler to see if a file is present
                                                   // SIZE INFO
  const uint32_t _file_size_max_bytes = 102400;    // arbitrary value of 100Kb (see notes above)
  uint8_t _column_labels_length = 0;               //
  uint32_t _file_size_data_bytes = 0;              // where we'll track the current data file size in case we need to roll over
                                                   // SESSION INFO
  uint16_t _session_id = 0;                        // this will be cast to hex and serve as the base of all filenames for this session
  uint32_t _curr_filenum = 0;                      // the 16bit session ID + file number
  char _column_labels[256];                        // comma separated column labels for when we start a new file
                                                   // FILE NAME INFO
  char _file_name_index[13];                       //
  char _file_name_time[13];                        //
  char _file_name_data[13];                        //
  uint32_t _bytes_used_this_session = 0;           //
                                                   // TIME INFO
  bool time_NOT_saved = true;                      //
  uint32_t _last_time_sync = 0;                    // millis() of the last time we wrote to the time file
  uint32_t _boot_time_sec = 0;                     // calculated time of when we started; derived from a manually fed epoch time in seconds
  int16_t _boot_drift_sec = 0;                     // for checking if the latest timesync seems off
  void _update_curr_file_name();                   //
  void _initialize_index_file();                   //
  void _update_index_file();                       //
  void _start_new_file();                          //
public:
  bool init();  // initializes the SD card
  void start_new_session();
  void start_new_session(String user_labels);
  bool isCurrFileFull(uint8_t new_data);
  bool mark_the_time(uint32_t timestamp);
  bool write(String* data_to_save);
  bool write(String data_to_save);
  uint32_t get_curr_file_num();
  float get_volume_size_GB();
  uint32_t get_free_size_KB();
} sd;

bool sd::_start_up() {
  if (_initialize()) {
    if (_mount()) { _get_card_space(); }
  }
  return isReady();
};
bool sd::_initialize() {
  if (SD.begin(PIN_SD_SS)) { current_status = SD_INITIALIZED; }
  if (card.init(SPI_HALF_SPEED, PIN_SD_SS)) {
    current_status = SD_INITIALIZED;
  } else {
    current_status = SD_CANNOT_INITIALIZE;
  }
  return isReady();
};
bool sd::_mount() {
  if (volume.init(card)) {
    _volume_size_kb = (volume.blocksPerCluster() * volume.clusterCount() / 2);
    current_status = SD_READY;
    return true;
  } else {
    current_status = SD_NOT_FORMATTED;
    return false;
  }
};
float sd::_get_card_size_GB() {  // KB to GB
  return (float)_volume_size_kb / 1048576;
};
bool sd::_get_card_space() {
  if (_volume_size_kb > 0) {
    _volume_used_kb = 0;
    _directory_count = 0;
    _file_count = 0;
    root = SD.open("/");
    _get_used_size(root);
    _volume_free_kb = _volume_size_kb - _volume_used_kb;
    return true;
  } else {
    return false;
  }
};
void sd::_get_used_size(File dir) {
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) { break; }  // no more files
    if (entry.isDirectory()) {
      _directory_count++;
      _get_used_size(entry);
    } else {
      _file_count++;
      _volume_used_kb += entry.size();
    }
    entry.close();
  }
};
bool sd::_doesFileExist(const char* fileNameInput) {
  return SD.exists(fileNameInput);
};
bool sd::isReady() {
  return (current_status == SD_READY) || (current_status == SD_INITIALIZED);
};
bool sd::init() {
  return _start_up();
};
bool sd::isCurrFileFull(uint8_t new_data = 0) {
  return (_file_size_data_bytes + new_data > _file_size_max_bytes);
};
void sd::_update_curr_file_name() {  // simply refreshes the file name char array based on file number; DOES NOT change file number
  sprintf(_file_name_data, "%08lx.CSV", _curr_filenum);
};
void sd::_initialize_index_file() {
  myFile = SD.open(_file_name_index, FILE_WRITE);  // try to open it initially
  if (myFile) {
    current_status = SD_INDEX_FILE_WRITTEN;
    myFile.print("File rollover: ");            // 15 chars
    myFile.print(_file_size_max_bytes / 1024);  // max 7 chars
    myFile.println(" Kb");                      // 3 chars + NL
    myFile.println("ASSOCIATED FILES: ");       // 18 chars + NL
    myFile.close();
    _bytes_used_this_session += 45;
    _update_index_file();
    current_status = SD_READY;
  } else {
    current_status = SD_UNABLE_OPEN_FOR_WRITE;
  }
};
void sd::_update_index_file() {
  _update_curr_file_name();
  myFile = SD.open(_file_name_index, FILE_WRITE);
  if (myFile) {
    char file_print_buffer[23];                      // 8 + 1 + 13 + 1 = 23
    sprintf(file_print_buffer, "%08lx ", millis());  // mark the system uptime of when we entered this filename
    strcat(file_print_buffer, _file_name_data);      // join them
    myFile.println(file_print_buffer);               // include the latest file name
    myFile.close();
    _bytes_used_this_session += 23;
    //current_status = SD_INDEX_UPDATED; // set the flag
    current_status = SD_READY;  // set the flag
  } else {
    current_status = SD_INDEX_NOT_UPDATED;
  }
};
void sd::start_new_session() {
  _bytes_used_this_session = 0;                             // initialize the count; this will be updated anytime we write to a file (faster than rerunning _get_card_space())
  _session_id = (uint16_t)get_rand_32();                    // get a random 4 character ID
  _curr_filenum = (uint32_t)_session_id << 16;              // left shift 16 bits, leaving the last 16 bits for sequence numbers
  sprintf(_file_name_index, "%04lxindx.txt", _session_id);  // set the index file name; we'll need to keep this around
  _initialize_index_file();                                 //
  sprintf(_file_name_time, "%04lxtime.txt", _session_id);   // set the time reference file, but we won't create it until we get a time sync message
  _start_new_file();
};
void sd::start_new_session(String user_labels) {
  _column_labels_length = MIN(sizeof(_column_labels) - 1, user_labels.length());
  if (_column_labels_length > 3) {
    strncpy(_column_labels, user_labels.c_str(), _column_labels_length);
  } else {
    _column_labels_length = 0;
  }
  _column_labels[_column_labels_length] = '\0';
  start_new_session();
};
void sd::_start_new_file() {
  _update_curr_file_name();                                   // simply refreshes the file name char array based on file number; DOES NOT change file number
  _update_index_file();                                       // add the starttime and new filename to the index file
  myFile = SD.open(_file_name_data, FILE_WRITE);              // attempt to open the newest data file
  if (myFile) {                                               // if we could open it,
    if (_column_labels_length > 0) {                          // if column labels were set
      myFile.println(_column_labels);                         // include it
      _file_size_data_bytes = _column_labels_length + 1;      // track it
      _bytes_used_this_session += _column_labels_length + 1;  // and this too
    }
    myFile.close();             // close it
    current_status = SD_READY;  // update status
  } else {
    current_status = SD_COULD_NOT_CREATE_NEW_DATA_FILE;
  }
};
bool sd::mark_the_time(uint32_t timestamp) {
  uint32_t marktime = millis();
  if (marktime - _last_time_sync > 30000) {
    uint32_t _uptime_sec = marktime / 1000;
    if (_uptime_sec < timestamp) { _boot_time_sec = timestamp - _uptime_sec; }
    myFile = SD.open(_file_name_time, FILE_WRITE);  // try to open it initially
    if (myFile) {
      char time_buffer[54];
      sprintf(time_buffer, "EPOCH:%08lx - UPTIME:%08lx = BOOTTIME:%08lx", timestamp, _uptime_sec, _boot_time_sec);
      time_buffer[53] = '\0';
      myFile.println(time_buffer);
      myFile.close();
      _bytes_used_this_session += 54;
      time_NOT_saved = false;
      current_status = SD_READY;
      _last_time_sync = marktime;
    } else {
      current_status = SD_UNABLE_OPEN_FOR_WRITE;
    }
  } else {
    // too soon, do nothing
  }
  return isReady();
}
bool sd::write(String* data_to_save) {
  if (isReady()) {
    uint8_t new_data_size = data_to_save->length();
    if (isCurrFileFull(new_data_size)) {
      _curr_filenum++;    // advance the file counter
      _start_new_file();  // this will create a new file with column headers AND update the index file
    }
    myFile = SD.open(_file_name_data, FILE_WRITE);
    if (myFile) {
      for (uint8_t i = 0; i < new_data_size; i++) {  // for simplicity, we'll strip out non-printable characters & NL / CRs
        char c = data_to_save->charAt(i);
        if (isPrintable(c) && c != (char)10 && c != (char)13) {
          myFile.write(c);
          _file_size_data_bytes++;
          _bytes_used_this_session++;
        }
      }
      myFile.write((char)10);
      _file_size_data_bytes++;
      _bytes_used_this_session++;
      myFile.close();
      current_status = SD_READY;
    } else {
      current_status = SD_COULD_NOT_OPEN_DATA_FILE;
    }
  }
  return isReady();
};
bool sd::write(String data_to_save) {
  return write(data_to_save);
};
uint32_t sd::get_curr_file_num() {
  return _curr_filenum;
};
float sd::get_volume_size_GB() {
  return (float)_volume_size_kb / 1048576;
};
uint32_t sd::get_free_size_KB() {
  return _volume_free_kb;
};