APP = p4lat
SRC = p4lat.c
PKGCONF ?= pkg-config
CFLAGS  += -O3 -g -Wall $(shell $(PKGCONF) --cflags libdpdk)
LDFLAGS += $(shell $(PKGCONF) --libs libdpdk)
$(APP): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(APP) $(LDFLAGS)
clean:
	rm -f $(APP)
