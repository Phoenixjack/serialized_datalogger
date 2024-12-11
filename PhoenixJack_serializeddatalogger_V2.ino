#include "PhoenixJack_serializeddatalogger_V2.h"


void setup() {
  Serial.begin(9600);
  while (!Serial) { ; }
  delay(1000);
  print_CLS();
  Serial.println("SD TEST");
  Serial.println("COMPILED: " __DATE__ " " __TIME__);
  if (sd.init()) {
    Serial.println("***** CARD READY *****");
  }
}

void loop() {
  if (Serial.available()) {
    String rcvString = Serial.readString();
    if (rcvString.startsWith("S")) {
      rcvString = rcvString.substring(1);
      Serial.print("Starting new session! ");
      sd.start_new_session(rcvString);
      Serial.println(sd.get_curr_file_name());
    } else if (rcvString.startsWith("D")) {
      Serial.println("*** LISTING ALL DISK CONTENTS ***");
      sd.printAll();
    } else if (rcvString.startsWith("R")) {
      if (rcvString.length() > 2) {
        rcvString = rcvString.substring(1);
        Serial.println("Attempting to open " + rcvString);
        //Serial.println(rcvString);
        if (!sd.read_this_file(rcvString)) {
          Serial.println("***** FILE NOT FOUND *****");
        }
      } else {
        sd.readcurrent();
      }
    } else if (rcvString.startsWith("T")) {
      rcvString = rcvString.substring(1);
      char* endptr;
      unsigned long value = strtoul(rcvString.c_str(), &endptr, 10);
      Serial.print("I see value: ");
      Serial.println(value);
      if (sd.mark_the_time(value)) { Serial.println("SAVED IT!"); }
    } else {
      rcvString = String(millis(), HEX) + "," + rcvString;
      if (sd.write(&rcvString)) {
        Serial.print("written to file [ ");
        Serial.print(rcvString);
        Serial.println(" ] ");
        Serial.flush();
        delay(100);
      } else {
        Serial.print("Failed to write to file [ ");
        Serial.print(sd.current_status);
        Serial.println(" ]");
      }
    }
  }
}
void print_CLS() {
  Serial.println("\n\n\n\n\n");
}

void print_menu() {
  Serial.println("USAGE:");
  Serial.println("  D....... list all files on the card");
  Serial.println("  Rxxxx... where x is a filename OR leave blank for the current file");
  Serial.println("  Sxxxx... where x is comma separated column label string");
  Serial.println("  Tyyyy... where y is the current epoch time in seconds");
  Serial.println("  Type anything else and I'll save it to file\n\n");
}

String get_user_input(uint32_t wait_time) {
  uint32_t timeout = millis() + wait_time;
  String rcvBuffer = "";
  return rcvBuffer;
}