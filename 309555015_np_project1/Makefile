# the compiler: gcc for C program, define as g++ for C++
CC = g++

# compiler flags:
#  -g    adds debugging information to the executable file
#  -Wall turns on most, but not all, compiler warnings
CFLAGS  = -g -Wall

  # the build target executable:
TARGET = npshell

all: $(TARGET)

$(TARGET): shell.cpp
	$(CC) $(CFLAGS) shell.cpp -o $(TARGET)

clean:
	$(RM) $(TARGET)