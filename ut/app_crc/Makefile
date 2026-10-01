TARGET = app_crc.bin

CC = gcc

CDIR=../common/
OBJDIR=./obj/

CFLAGS = -DPLATFORM_NIX -c -g -Wall -I$(CDIR)
INSTALL_DIR = ..

OBJS = $(OBJDIR)main.o $(OBJDIR)ihex.o

all:  $(OBJDIR) $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -static $(PFLFLAGS) $(OBJS) $(LDFLAGS) -g -o $(TARGET)


install:
	strip $(TARGET)
	cp $(TARGET) ${INSTALL_DIR}

clean:
	rm -f $(OBJDIR)*.o
	rm -f $(TARGET)


# depend on the shared flash-layout header: APP_INFO_PAGE_SIZE / APP_START_ADDR
# live there, and a stale tool silently produces a wrong app_info.size.
$(OBJDIR)main.o:     main.[ch] $(CDIR)app.h
	$(CXX) $(CFLAGS) main.c -o $(OBJDIR)main.o

$(OBJDIR)ihex.o:     $(CDIR)ihex.[ch]
	$(CXX) $(CFLAGS) $(CDIR)ihex.c -o $(OBJDIR)ihex.o

$(OBJDIR):
	mkdir -p $(OBJDIR)

.PHONY: all clean install

