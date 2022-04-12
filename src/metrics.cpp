#include <ESP8266WiFi.h>

void metrics_print(const char *header) {
    Serial.println();

    if (header) {
        Serial.println(header);
    }

    Serial.print("Free heap: ");
    Serial.println(ESP.getFreeHeap());
    Serial.print("Heap fragmentation: ");
    Serial.println(ESP.getHeapFragmentation());
    Serial.print("Max free block size: ");
    Serial.println(ESP.getMaxFreeBlockSize());
}