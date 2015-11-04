#include "Env.h"

#include <EEPROM.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>

// The "environment" is stored as key=value pairs separated by a NUL byte.
// Two NUL bytes signify the end of the environment.
// It is good form to zero-fill the rest of the "eeprom" with NULs anyway.
//

uint16_t Env::kEepromSize = 4096;

inline bool isenvname(int c) {
  return isalnum(c) || c == '_';
}

// * Start scanning at &data_ptr_[start].
// * If a proper foo=bar entry is found starting at @start,
//   return the location of the = in @equals and the location of
//   the nul in @nul. Return true. The next scan should start at nul+1.
// * If the value is missing, then nul == equals + 1. Return true.
// * If we are at the double-null, then nul == start. Return true.
// * If a malformed entry is found, simply locate (and return) the next nul.
//   Return false.
bool Env::_scanner(uint16_t start,   // IN
                  uint16_t* equals, // OUT
                  uint16_t* nul) {  // OUT
  enum State {
    START_OF_PAIR,
    READING_KEY,
    READING_VALUE,
    SKIPPING,
  } state;

  state = START_OF_PAIR;
  for (uint16_t offset = start; offset < kEepromSize - 1; ++offset) {
    uint8_t c = data_ptr_[offset];
    switch (state) {
    case START_OF_PAIR:
      if (c == 0) {
        *nul = start;
        return true;
      } else if (isenvname(c)) {
        state = READING_KEY;
      } else {
        state = SKIPPING;
      }
      break;
    case READING_KEY:
      if (c == 0) {
        *nul = offset;
        return false;
      } else if (c == '=') {
        *equals = offset;
        state = READING_VALUE;
      } else if (!isenvname(c)) {
        state = SKIPPING;
      }
      break;
    case READING_VALUE:
      if (c == 0) {
        *nul = offset;
        return true;
      } else if (!isprint(c)) {
        state = SKIPPING;
      }
      break;
    case SKIPPING:
      if (c == 0) {
        *nul = offset;
        return false;
      }
      break;
    default:
      return false;
    }
  }
  return false;
}

Env::Env() {
  EEPROM.begin(kEepromSize);
  data_ptr_ = EEPROM.getDataPtr();
}

Env::~Env() {
  EEPROM.commit();
  EEPROM.end();
}

void Env::_commit() {
  EEPROM.commit();
  EEPROM.getDataPtr(); // mark as dirty
}

void Env::clear() {
  memset(data_ptr_, 0, kEepromSize);
  _commit();
}

const char* Env::get(const char* key) {
  return _get(key, nullptr);
}

bool Env::unset(const char* key) {
  return _unset(key, true);
}

bool Env::_unset(const char* key, bool commit) {
  uint16_t key_len = strlen(key);
  if (key_len == 0) {
    return false;
  }
  uint16_t key_pos;
  char* val = _get(key, &key_pos);
  if (val == nullptr) {
    return false;
  }
  //
  //             k    ev       nf         zx
  //             |    ||       ||         ||
  //  ..........0KKKKK=VVVVVVVV0.........00xxxx
  //             ^              ^^^^^^^^^^^ move these -.
  //             '----here------------------------------'
  //
  //  The following points are of interest. Their values are the offset
  //  from the beginning of the EEPROM area.
  //
  //  k: (== key_pos) where the key starts.
  //  e: (== key_pos + key_len) location of the = sign
  //  v: (== key_pos + key_len + 1) where the value starts
  //  n: (== key_pos + key_len + 1 + val_len) location of the NUL
  //  f: (== key_pos + key_len + 1 + val_len + 1) first byte to be moved back
  //  z: (== _find_double_zero()) the second of the two terminating NULs
  //  x: (== _find_double_zero() + 1) first unused byte.
  //
  //  We want to take the bytes from f to z (inclusive) and move them to k.
  //  destination is @k
  //  source is @f
  //  number of bytes is @z - @f + 1
  //
  //  A single entry is a special case:
  //
  //                 z
  //  k    ev       nf
  //  |    ||       ||
  //  KKKKK=VVVVVVVV00xxxxxxxx
  //
  //  Here, k=0, f=z; we would thus be moving just one byte (the second zero)
  //  to the zeroth byte, leaving the first byte with its original value, resulting
  //  in invalid image.  Identify and special-case this situation.
  uint16_t f_pos = key_pos + key_len + 1 + strlen(val) + 1;
  uint16_t z_pos = _find_double_zero();
  if (key_pos == 0 && f_pos == z_pos) {
    data_ptr_[0] = data_ptr_[1] = 0;
  } else {
    memmove(&data_ptr_[key_pos], &data_ptr_[f_pos], z_pos + 1 - f_pos);
  }
  if (commit) {
    _commit();
  }
  return true;
}

const char* Env::set(const char* key, const char* value) {
  uint16_t key_len = strlen(key);
  if (key_len == 0) {
    return nullptr;
  }
  uint16_t val_len = 0;
  if (value != nullptr) {
    val_len = strlen(value);
  }
  _unset(key, false);
  uint16_t free_space_start = _find_double_zero();
  if (free_space_start == 0) {
    return nullptr;
  }
  if (free_space_start == 1) {
    // Empty env, special-case.
    free_space_start = 0;
  }
  memcpy(&data_ptr_[free_space_start], key, key_len);
  data_ptr_[free_space_start + key_len] = '=';
  memcpy(&data_ptr_[free_space_start + key_len + 1], value, val_len);
  data_ptr_[free_space_start + key_len + 1 + val_len] = '\0';
  data_ptr_[free_space_start + key_len + 1 + val_len + 1] = '\0';
  _commit();
  return (char*)&data_ptr_[free_space_start + key_len + 1];
}

uint16_t Env::_find_double_zero() {
  for (uint16_t i = 0; i < kEepromSize - 1; ++i) {
    if (data_ptr_[i] == 0 && data_ptr_[i + 1] == 0) {
      return i + 1;
    }
  }
  return 0;
}

// _get actually skips over bad entries, no reason to fail.
char* Env::_get(const char* key, uint16_t* keypos) {
  int keylen = strlen(key);
  if (keylen == 0 || strchr(key, '=')) {
    return nullptr;
  }
  uint16_t offset = 0;
  uint16_t equals;
  uint16_t nul;
  while (offset + keylen + 2 < kEepromSize) {
    equals = 0xffff;
    nul = 0xffff;
    if (!_scanner(offset, &equals, &nul)) {
      if (equals != 0xffff) {
        if (keypos != nullptr) {
          *keypos = 0xffff;
        }
        return nullptr;
      }
    } else {
      if (offset == nul) {
        if (keypos != nullptr) {
          //        *keypos = nul + 1;
        }
        return nullptr;
      }
      if ((equals - offset) == keylen &&
          memcmp(key, &data_ptr_[offset], keylen) == 0) {
        if (keypos != nullptr) {
          *keypos = offset;
        }
        return (char *)&data_ptr_[equals + 1];
      }
    }
    offset = nul + 1;
  }
  if (keypos != nullptr) {
    //    *keypos = 0xfffe;
  }
  return nullptr;
}

uint16_t Env::bytes_used() {
  return _find_double_zero() + 1;
}

void Env::_sizer(uint16_t* pairs, uint16_t* bads, uint16_t* bytes) {
  *pairs = 0;
  *bads = 0;
  for (*bytes = 0; *bytes <= kEepromSize;) {
    uint16_t equals;
    uint16_t nul;
    if (!_scanner(*bytes, &equals, &nul)) {
      ++*bads;
    } else if (nul == *bytes) {
      return;
    } else {
      ++*pairs;
    }
    *bytes = nul + 1;
  }
  return;
}

uint16_t Env::size() {
  uint16_t pairs;
  uint16_t bads;
  uint16_t bytes;
  _sizer(&pairs, &bads, &bytes);
  return pairs;
}

bool Env::is_valid_env() {
  uint16_t pairs;
  uint16_t bads;
  uint16_t bytes;
  _sizer(&pairs, &bads, &bytes);
  return bads == 0;
}
