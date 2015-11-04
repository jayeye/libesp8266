USER_DIR=../src/

TESTS=env_unittest

CPPFLAGS += -I. -I$(USER_DIR)
CXXFLAGS += -std=c++11 -g -Wall -Wextra -pthread

all: env_unittest

Env.o: $(USER_DIR)/Env.cpp $(USER_DIR)/Env.h EEPROM.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $(USER_DIR)/Env.cpp

env_unittest.o: Env.o env_unittest.cc EEPROM.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c env_unittest.cc

env_unittest: env_unittest.o Env.o
	$(CXX) $(CPPFLAGS) $(CXXFLAGS)  $^ -o $@ -lpthread -lgtest_main  -lgtest

clean:
	rm -f *.o env_unittest
