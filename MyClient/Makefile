# GNU Makefile for the MyClient FTP lab project
# Targets: all (default), clean, distclean, run, help

TARGET := client
CC := gcc
CFLAGS := -std=gnu11 -Wall -Wextra -O2 -g -I. -Icmd -Inet -Itransfer -Iui -Iutils -MMD -MP
LDFLAGS :=

# Collect source files from root and subdirectories
SRCS := $(wildcard *.c cmd/*.c net/*.c transfer/*.c ui/*.c utils/*.c)

OBJDIR := build
OBJS := $(patsubst %.c,$(OBJDIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all clean distclean run help

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

# compile rule: ensure output directory exists
$(OBJDIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

# Include dependency files if present
-include $(DEPS)

clean:
	rm -rf $(OBJDIR) *.d $(TARGET)

distclean: clean
	rm -f $(TARGET)

run: all
	./$(TARGET)

help:
	@echo "Makefile targets:"
	@echo "  make         build $(TARGET) (default)"
	@echo "  make clean   remove object files" 
	@echo "  make distclean remove objects and binary"
	@echo "  make run     build and run the program"
	@echo "  make help    show this message"
