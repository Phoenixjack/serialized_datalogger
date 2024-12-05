#include "PhoenixJack_serializeddatalogger_V1.h"

void setup() {
  Serial.begin(9600);
  while (!Serial) { ; }
  delay(1000);
  Serial.println("\n\n\n\n\nSD TEST");
  if (sd.init()) {
    Serial.println("Card ready.");
    Serial.println("USAGE:");
    Serial.println("  Sxxxx... where x is comma separated column label string");
    Serial.println("  Tyyyy... where y is the current epoch time in seconds");
    Serial.println("  Type anything else and I'll save it to file\n\n");
  }
}

void loop() {
  if (Serial.available()) {
    String rcvString = Serial.readString();
    if (rcvString.startsWith("S")) {
      rcvString = rcvString.substring(1);
      Serial.println("Starting new session!");
      sd.start_new_session(rcvString);
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