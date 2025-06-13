CC = gcc
TARGET = client
LINK_OPTION = -lws2_32
$(TARGET): testjsonclient.c
	$(CC) $< -o $@ $(LINK_OPTION)

clean:
	rm -f $(TARGET)

.PHONY: all clean
