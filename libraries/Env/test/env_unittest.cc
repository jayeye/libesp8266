#include "Env.h"
#include "EEPROM.h"
#include <gtest/gtest.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <vector>

using namespace std;

const uint16_t kEepromSize = 4096;
uint8_t mock_eeprom_data[kEepromSize];
const char* mock_eeprom = (const char*)mock_eeprom_data;
uint16_t mock_eeprom_size = kEepromSize;
EepromMock EEPROM;

__attribute__((unused)) static void _hexdump(const uint8_t* buf, int buflen) {
  for (int i = 0; i < buflen; i += 16) {
    fprintf(stderr, "%06x: ", i);  // 8 chars
    for (int j = 0; j < 16; ++j) {
      if ((i + j) < buflen) {
        fprintf(stderr, "%02x ", buf[i + j]);
      } else {
        fprintf(stderr, "   ");
      }
    }
    fprintf(stderr, " ");
    for (int j = 0; j < 16; ++j) {
      if ((i + j) < buflen) {
        fprintf(stderr, "%c", isprint(buf[i + j]) ? buf[i + j] : '.');
      }
    }
    fprintf(stderr, "\n");
  }
}

class EnvTest : public testing::Test {
protected:
  virtual void SetUp() {
    memset(mock_eeprom_data, 254, mock_eeprom_size);
  }

  static void set_env(uint8_t* ee, string s, uint16_t* offset) {
    for (auto c : s) {
      if (c == '$') {
        c = '\0';
      }
      ee[*offset] = c;
      ++*offset;
      EXPECT_LT(*offset, kEepromSize);
    }
  }

  static uint16_t make_env(uint8_t* ee, vector<string> envs) {
    uint16_t offset = 0;
    for (auto s : envs) {
      set_env(ee, s, &offset);
      ee[offset] = 0;
      ++offset;
      EXPECT_LT(offset, kEepromSize);
    }
    ee[offset] = 0;
    if (offset == 0) {
      ee[1] = 0;
      return 2;
    }
    return offset + 1;
    //_hexdump(mock_eeprom_data, 64);
  }

  static uint16_t make_env(vector<string> envs) {
    return make_env(mock_eeprom_data, envs);
  }

  static bool equal_env(vector<string> envs) {
    unique_ptr<uint8_t> temp_env(new uint8_t[kEepromSize]);
    uint16_t temp_size = make_env(temp_env.get(), envs);
    return memcmp(mock_eeprom_data, temp_env.get(), temp_size) == 0;
  }

  static inline void EXPECT_EQENV(vector<string> envs) {
    EXPECT_TRUE(equal_env(envs));
  }
  Env e_;
};
    
TEST_F(EnvTest, MakeEnvWorks) {
  EXPECT_EQ(2, make_env({}));
  EXPECT_EQ(0, mock_eeprom_data[0]);
  EXPECT_EQ(0, mock_eeprom_data[1]);
  EXPECT_TRUE(e_.is_valid_env());

  EXPECT_EQ(9, make_env({"FOO=bar"}));
  EXPECT_TRUE(e_.is_valid_env());

  EXPECT_EQ(17, make_env({"FOO", "FOO$$", "BAR", "$"}));
  EXPECT_EQ(0, memcmp(mock_eeprom_data,
                      "FOO\0FOO\0\0\0BAR\0\0\0\0\376\376", 19));
  EXPECT_FALSE(e_.is_valid_env());
}

TEST_F(EnvTest, MakeEnvExpectWorks) {
  EXPECT_EQ(2, make_env({}));
  EXPECT_EQENV({});

  EXPECT_EQ(9, make_env({"FOO=bar"}));
  EXPECT_EQENV({"FOO=bar"});
}

TEST_F(EnvTest, IsValidEnv) {
  uint16_t offset = 0;
  set_env(mock_eeprom_data, "$$xx", &offset);
  EXPECT_TRUE(e_.is_valid_env());

  offset = 0;
  set_env(mock_eeprom_data, "FOO=bar$FOO=barbar$BAR=$XYZZY=xyzzy$$", &offset);
  EXPECT_TRUE(e_.is_valid_env());

  offset = 0;
  set_env(mock_eeprom_data, "FOO=bar$FOO=barbar$BAR$XYZZY=xyzzy$$", &offset);
  EXPECT_FALSE(e_.is_valid_env());
}

TEST_F(EnvTest, SizeAndBytes) {
  make_env({});
  EXPECT_EQ(0, e_.size());
  EXPECT_EQ(2, e_.bytes_used());
  EXPECT_TRUE(e_.is_valid_env());

  make_env({"FOO=bar"});
  EXPECT_EQ(1, e_.size());
  EXPECT_EQ(9, e_.bytes_used());  
  EXPECT_TRUE(e_.is_valid_env());

  make_env({"^^BAD&&", "ANOTHER", "=bad"});
  EXPECT_EQ(0, e_.size());
  EXPECT_EQ(22, e_.bytes_used());
  EXPECT_FALSE(e_.is_valid_env());

  make_env({"BAD", "FOO=bar", "XXX="});
  EXPECT_EQ(2, e_.size());
  EXPECT_EQ(18, e_.bytes_used());
  EXPECT_FALSE(e_.is_valid_env());
}

TEST_F(EnvTest, Getter) {
  uint16_t offset = 0;
  set_env(mock_eeprom_data, "FOO=bar$FLAG=$NOEQ$;BADNAME=xx$BADVALUE=\01$$", &offset);
  // names at  0,  8, 
  // =s    at  3, 12, 
  // vals  at  4, 13,
  // NULs  at  7, 13, 18, 30, 41, 42
  ASSERT_EQ(43, offset);
  uint16_t key_pos;
  EXPECT_EQ((char*)&e_.data_ptr_[4], e_._get("FOO", &key_pos));
  EXPECT_EQ(0, key_pos);
  EXPECT_EQ((char*)&e_.data_ptr_[13], e_._get("FLAG", &key_pos));
  EXPECT_EQ(8, key_pos);
  EXPECT_EQ(nullptr, e_._get("BADVALUE", &key_pos));
  EXPECT_EQ(0xffff, key_pos);
  EXPECT_EQ(nullptr, e_._get("NONEXISTENT", &key_pos));
  EXPECT_EQ(0xffff, key_pos);
}

TEST_F(EnvTest, Scanner) {
  uint16_t start;
  uint16_t equals;
  uint16_t nul;
  
  uint16_t offset = 0;
  set_env(mock_eeprom_data, "FOO=bar$FLAG=$NOEQ$;BADNAME=xx$BADVALUE=\01$$x", &offset);
  ASSERT_EQ(44, offset);
  // =s   at  3, 12, 
  // NULs at  7, 13, 18, 30, 41, 42

  EXPECT_EQ(42, e_._find_double_zero());

  start = 0;
  equals = 0xdead;
  nul = 0xbeef;
  EXPECT_TRUE(e_._scanner(start, &equals, &nul));
  EXPECT_EQ(3, equals);
  EXPECT_EQ(7, nul);
  EXPECT_EQ("FOO=bar", string(mock_eeprom));

  start = nul + 1;
  equals = 0xdead;
  nul = 0xbeef;
  EXPECT_TRUE(e_._scanner(start, &equals, &nul));
  EXPECT_EQ(12, equals);
  EXPECT_EQ(13, nul);
  EXPECT_EQ("FLAG=", string(mock_eeprom + start));

  start = nul + 1;
  equals = 0xdead;
  nul = 0xbeef;
  EXPECT_FALSE(e_._scanner(start, &equals, &nul));
  EXPECT_EQ(0xdead, equals);
  EXPECT_EQ(18, nul);
}

TEST_F(EnvTest, GetEmptyEnv) {
  make_env({});
  EXPECT_EQ(nullptr, e_.get("FOO"));
}

TEST_F(EnvTest, GetSingletonEnv) {
  make_env({"FOO=foo"});
  EXPECT_STREQ("foo", e_.get("FOO"));
}

TEST_F(EnvTest, GetOrderMatters) {
  make_env({"FOO=foo", "FOO=bar"});
  EXPECT_STREQ("foo", e_.get("FOO"));
}

TEST_F(EnvTest, GetExactNameMatters1) {
  make_env({"FOO=foo", "FOO=bar", "FOOBAR=foobar"});
  EXPECT_STREQ("foo", e_.get("FOO"));
}

TEST_F(EnvTest, GetExactNameMatters2) {
  make_env({"FOOBAR=foobar", "FOO=foo", "FOO=bar"});
  EXPECT_STREQ("foo", e_.get("FOO"));
}

TEST_F(EnvTest, GetNullValue) {
  make_env({"FOO="});
  EXPECT_STREQ("", e_.get("FOO"));
}

TEST_F(EnvTest, GetNameOnlyBad) {
  make_env({"BAR=bar", "FOO"});
  EXPECT_EQ(nullptr, e_.get("FOO"));
}

TEST_F(EnvTest, GetSkipBadEntries) {
  make_env({"BAR=bar", "FUBAR", "FUBAR=42", "FOO=foo"});
  EXPECT_STREQ("foo", e_.get("FOO"));
}

TEST_F(EnvTest, UnsetEmpty) {
  make_env({});
  EXPECT_FALSE(e_.unset("FOO"));
}

TEST_F(EnvTest, UnsetFirst) {
  make_env({"FOO=foofoo", "BARBAR=bar"});
  EXPECT_TRUE(e_.unset("FOO"));
  EXPECT_EQENV({"BARBAR=bar"});
}

TEST_F(EnvTest, UnsetLast) {
  make_env({"FOO=foofoo", "BARBAR=bar"});
  EXPECT_TRUE(e_.unset("BARBAR"));
  EXPECT_EQENV({"FOO=foofoo"});
}

TEST_F(EnvTest, UnsetSingleton) {
  make_env({"FOO=foo"});
  EXPECT_TRUE(e_.unset("FOO"));
  EXPECT_EQ(0, mock_eeprom_data[0]);
  EXPECT_EQ(0, mock_eeprom_data[1]);
}

TEST_F(EnvTest, UnsetFlag) {
  make_env({"BAR=foo", "FLAG=", "LAST=last"});
  EXPECT_TRUE(e_.unset("FLAG"));
  EXPECT_EQENV({"BAR=foo", "LAST=last"});
}

TEST_F(EnvTest, SetWhenEmpty) {
  make_env({});
  EXPECT_STREQ("bar", e_.set("FOO", "bar"));
  EXPECT_EQENV({"FOO=bar"});
}

TEST_F(EnvTest, SetWhenNotEmpty) {
  make_env({"FOO=bar", "FOOBAR=foobar"});
  EXPECT_STREQ("fo", e_.set("FO", "fo"));
  EXPECT_EQENV({"FOO=bar", "FOOBAR=foobar", "FO=fo"});
}

TEST_F(EnvTest, SetEmptyVal) {
  make_env({"FOO=bar", "FOOBAR=foobar"});
  EXPECT_STREQ("", e_.set("EMPTY", nullptr));
  EXPECT_EQENV({"FOO=bar", "FOOBAR=foobar", "EMPTY="});
}

TEST_F(EnvTest, SetWhenBadStuffExists) {
  make_env({"^^BAD&&", "ANOTHER", "=bad"});
  EXPECT_STREQ("bar", e_.set("FOO", "bar"));
  EXPECT_EQENV({"^^BAD&&", "ANOTHER", "=bad", "FOO=bar"});
}  

TEST_F(EnvTest, SetWithReplacementShorter) {
  make_env({"FOO=bar", "FOOBAR=foobar"});
  EXPECT_STREQ("b", e_.set("FOO", "b"));
  EXPECT_EQENV({"FOOBAR=foobar", "FOO=b"});
}

TEST_F(EnvTest, SetWithReplacementSameSize) {
  make_env({"FOO=bar", "FOOBAR=foobar"});
  EXPECT_STREQ("oom", e_.set("FOO", "oom"));
  EXPECT_EQENV({"FOOBAR=foobar", "FOO=oom"});
}

TEST_F(EnvTest, SetWithReplacementLonger) {
  make_env({"FOO=bar", "FOOBAR=foobar"});
  EXPECT_STREQ("barbar", e_.set("FOO", "barbar"));
  EXPECT_EQENV({"FOOBAR=foobar", "FOO=barbar"});
}

TEST_F(EnvTest, SetWithReplacementMix) {
  make_env({"A=al", "BB=bee", "CCC=see", "DDDD=deltas"});
  EXPECT_STREQ("alpha", e_.set("A", "alpha"));
  EXPECT_EQENV({"BB=bee", "CCC=see", "DDDD=deltas", "A=alpha"});

  EXPECT_STREQ("cxaxa", e_.set("CCC", "cxaxa"));
  EXPECT_EQENV({"BB=bee", "DDDD=deltas", "A=alpha", "CCC=cxaxa"});

  EXPECT_STREQ("cbgb", e_.set("CCC", "cbgb"));
  EXPECT_EQENV({"BB=bee", "DDDD=deltas", "A=alpha", "CCC=cbgb"});
}
