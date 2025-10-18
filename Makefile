CC = gcc
LIBS = -lcurl -lcjson

TARGET = speed_test
SRC = speed_test_Steponas_Vaisnora.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
