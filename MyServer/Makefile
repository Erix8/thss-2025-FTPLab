CC       := gcc
CFLAGS   := -Wall -Wextra -O2 -g -pthread -MMD -MP
CPPFLAGS := -Iserver -Iserver/config -Iserver/net -Iserver/conn -Iserver/cmd -Iserver/transfer -Iutils
LDFLAGS  := -pthread

BIN      := server

SRCS := \
    main.c \
    config/config.c \
    net/socket_utils.c \
    conn/client_conn.c \
    cmd/ftp_cmds.c \
    transfer/data_transfer.c \
    utils/utils.c

OBJDIR := build
OBJS   := $(SRCS:%.c=$(OBJDIR)/%.o)
DEPS   := $(OBJS:.o=.d)

.PHONY: all clean run
all: $(BIN)

$(BIN): $(OBJS)
	@echo "Linking $@"
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Pattern rule: build/server/xxx.o from server/xxx.c
$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(BIN)

run: $(BIN)
	./$(BIN) -port 21 -root /tmp

-include $(DEPS)