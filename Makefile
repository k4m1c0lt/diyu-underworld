# 大闹阴曹 · 归山 —— GCC / MinGW 构建
CC      ?= gcc
CFLAGS  ?= -O2 -std=c99 -Wall
LDFLAGS ?=

ifeq ($(OS),Windows_NT)
    TARGET = diyu.exe
    LDLIBS = -luser32 -lkernel32
else
    TARGET = diyu
    LDLIBS = -lm
endif

.PHONY: all clean run selftest

all: $(TARGET)

$(TARGET): diyu.c
	$(CC) $(CFLAGS) diyu.c -o $(TARGET) $(LDFLAGS) $(LDLIBS)

run: $(TARGET)
	./$(TARGET)

selftest: $(TARGET)
	./$(TARGET) --sim 20000
	./$(TARGET) --sim 3000 --stage 3
	./$(TARGET) --sim 3000 --god

clean:
	rm -f $(TARGET) diyu *.o
