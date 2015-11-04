#include <EEPROM.h>
#include <Env.h>

static void _hexdump(const char* buf, int buflen) {
  static const int kBufSize = 74;
  char line[kBufSize];
  for (int i = 0; i < buflen; i += 16) {
    int offset = 0;
    snprintf(&line[0], kBufSize, "%06x: ", i);  // 8 chars
    for (int j = 0; j < 16; ++j) {
      if ((i + j) < buflen) {
        snprintf(&line[8 + 3 * j], kBufSize - 8 - 3 * j, "%02x ", buf[i + j]);
      } else {
        snprintf(&line[8 + 3 * j], kBufSize - 8 - 3 * j, "   ");
      }
      line[56] = ' ';
    }
    for (int j = 0; j < 16 && (i + j) < buflen; ++j) {
      snprintf(&line[57 + j], kBufSize - 57 - j, "%c", isprint(buf[i + j]) ? buf[i + j] : '.');
    }
    line[73] = '\0';
    Serial.println(line);
  }
}

Env* ENV;
void setup() {
  ENV = new Env();
  Serial.begin(9600);
}

uint8_t bi = 0;
const uint8_t kMaxBi = 80;
char buf[kMaxBi];

int scan_serial() {
  if (Serial.available()) {
    unsigned int c = Serial.read();
    if (c == '\r') {
      buf[bi] = '\0';
      int retval = bi;
      bi = 0;
      return retval;
    } else {
      if (bi == kMaxBi - 2) {
        Serial.write('\a');
      } else if (c == '\b') {
        if (bi > 0) {
          --bi;
          Serial.print(F("\b \b"));
        }
      } else {
        buf[bi++] = c;
        Serial.write(c);
      }
    }
  }
  return -1;
}

void procbuffer() {
  int len;
  char* bp = buf;
  while (isspace(*bp)) {
    ++bp;
  }
  Serial.println();
  switch (*bp) {
  case '?':
    Serial.println(F("# Available commands:"));
    Serial.println(F("# ? - this help"));
    Serial.println(F("# # - comment"));
    Serial.println(F("# @ - clear"));
    Serial.println(F("# ~ - close and reopen ENV"));
    Serial.println(F("# ! - hexdump"));
    Serial.println(F("# !! - hexdump everything"));
    Serial.println(F("# -FOO - unset FOO"));
    Serial.println(F("# ?FOO - print FOO"));
    Serial.println(F("# FOO=bar - set FOO to bar"));
    break;
  case '\0':
  case '#':
    Serial.println();
    break;
  case '@':
    ENV->clear();
    Serial.println(F("# CLEARED"));
    break;
  case '~':
    delete ENV;
    ENV = new Env();
    Serial.println(F("# REOPENED"));
    break;
  case '!':
    ++bp;
    len = (*bp == '!') ? 4096 : ENV->bytes_used();
    _hexdump((const char*)EEPROM.getDataPtr(), len);
    break;
  case '-':
    ++bp;
    if (!ENV->unset(bp)) {
      Serial.println(F("# NONEXISTENT - CAN'T UNSET"));
    }
    break;
  default:
    char* eql = strchr(bp, '=');
    const char* val;
    if (eql) {
      *eql = '\0';
      val = ENV->set(bp, eql + 1);
    } else { 
      val = ENV->get(bp);
    }
    if (val) {
      Serial.print(bp);
      Serial.write('=');
      Serial.println(val);
    } else {
      Serial.println(F("# SYNTAX ERROR"));
    }
  }
}

void loop() {
  if (scan_serial() != -1) {
    procbuffer();
  }
}  
