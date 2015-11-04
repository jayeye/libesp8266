#ifndef _EEPROM_H_
#define _EEPROM_H_
// Mock EEPROM for tests.
#include <stdint.h>
#include <string.h>

extern uint8_t mock_eeprom_data[];
extern uint16_t mock_eeprom_size;

class EepromMock {
 public:
  EepromMock() {}
  ~EepromMock() {}
  void begin(size_t __attribute__((unused)) eeprom_size) {
  }
  void commit() {
  }
  void end() {
  }
  uint8_t* getDataPtr() {
    return mock_eeprom_data;
  }
};

extern EepromMock EEPROM;
#endif
