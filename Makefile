CXX ?= g++

CORE_PACKAGES := Imath
APP_PACKAGES := Imath glut glu gl libpng
CORE_CPPFLAGS := -I. $(shell pkg-config --cflags $(CORE_PACKAGES))
APP_CPPFLAGS := -I. $(shell pkg-config --cflags $(APP_PACKAGES))
CXXFLAGS ?= -O2 -g
CXXFLAGS += -std=c++17 -Wall -Wextra -Wpedantic
CORE_LDLIBS := $(shell pkg-config --libs $(CORE_PACKAGES))
APP_LDLIBS := $(shell pkg-config --libs $(APP_PACKAGES))

CORE_SOURCES := hexplanet.cpp puzzle.cpp camera.cpp bounded_adams_bashforth.cpp debug_log.cpp sequence_tracker.cpp
APP_SOURCES := hexp_main.cpp application.cpp renderer.cpp texture.cpp $(CORE_SOURCES)
TEST_SOURCES := tests/core_tests.cpp $(CORE_SOURCES)

.PHONY: all clean test check

all: hexp_main

hexp_main: $(APP_SOURCES) application.h renderer.h texture.h puzzle.h camera.h bounded_adams_bashforth.h hexplanet.h debug_log.h sequence_tracker.h
	$(CXX) $(APP_CPPFLAGS) $(CXXFLAGS) $(APP_SOURCES) -o $@ $(APP_LDLIBS)

hexpuzzle_core_tests: $(TEST_SOURCES) puzzle.h camera.h bounded_adams_bashforth.h hexplanet.h debug_log.h sequence_tracker.h
	$(CXX) $(CORE_CPPFLAGS) $(CXXFLAGS) $(TEST_SOURCES) -o $@ $(CORE_LDLIBS)

test: hexpuzzle_core_tests
	./hexpuzzle_core_tests

check: test hexp_main

clean:
	rm -f hexp_main hexpuzzle_core_tests *.o tests/*.o *.d tests/*.d
