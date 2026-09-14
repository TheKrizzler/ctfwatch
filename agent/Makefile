CC = gcc

CFLAGS = -Wall -Wextra -DLOG_USE_COLOR -fPIE \
         -Iinclude \
         -Ivendor/log \
         -Ivendor/cjson
LDFLAGS = -lcurl -lz -pthread -pie

SRC = src/main.c \
      src/protocol.c \
      src/profile.c \
      src/docker.c \
      src/event.c \
      src/queue.c \
      src/forwarder.c \
      src/compress.c \
      src/server.c \
      src/config.c \
      vendor/log/log.c \
      vendor/cjson/cJSON.c

TARGET = build/ctfwatch-agent

$(TARGET): $(SRC)
	mkdir -p build
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -rf build

.PHONY: clean