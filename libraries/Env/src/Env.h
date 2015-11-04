#ifndef _ENV_H_
#define _ENV_H_

// The ESP8266 uses 4096 bytes of flash as a simulated EEPROM.
// This library imposes an environment-like structure of key=value
// pairs, stored as such, pairs separated by a NUL byte.  Two NUL
// bytes signify the end of the environment.  It is good form to
// zero-fill the rest of the "eeprom" with NULs anyway.

// DO NOT USE EEPROM.h directly with this code, or the Universe may explode.

// Found in esp8266/hardware/esp8266/1.6.5-947-g39819f0/libraries/EEPROM/
#include <EEPROM.h>
#ifndef ARDUINO
#include <gtest/gtest_prod.h>
#endif
#include <stdint.h>
#include <stddef.h>

// TODO(ji): Make this an STL-like container, with proper iterators.

class Env {
public:
  Env();
  ~Env();
  void clear();
  bool is_valid_env();
  uint16_t bytes_used();
  uint16_t size();
  const char* get(const char* key);
  const char* set(const char* key, const char* value);
  bool unset(const char* key);

private:
  static uint16_t kEepromSize;
  uint8_t* data_ptr_;

  void _commit();
  uint16_t _find_double_zero();     // the second NUL
  char* _get(const char* key, uint16_t* keypos);
  bool _scanner(uint16_t start, uint16_t* equals, uint16_t* nul);
  void _sizer(uint16_t* pairs, uint16_t* bads, uint16_t* bytes);
  bool _unset(const char* key, bool commit);

#ifndef ARDUINO
  FRIEND_TEST(EnvTest, Scanner);
  FRIEND_TEST(EnvTest, Getter);
#endif
};
#endif
