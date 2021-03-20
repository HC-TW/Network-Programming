# the compiler: gcc for C program, define as g++ for C++
CC = g++

# compiler flags:
#  -g    adds debugging information to the executable file
#  -Wall turns on most, but not all, compiler warnings
CFLAGS  = -g -Wall

  # the build target executable:
TARGET1 = np_simple
TARGET2 = np_single_proc

all: $(TARGET1) $(TARGET2)

$(TARGET1): np_simple.cpp
	$(CC) $(CFLAGS) np_simple.cpp -o $(TARGET1)

$(TARGET2): np_single_proc.cpp
	$(CC) $(CFLAGS) np_single_proc.cpp -o $(TARGET2)

clean:
	$(RM) $(TARGET1)
	$(RM) $(TARGET2)