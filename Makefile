APP = usrcheat
OBJS = usrcheat.o

prefix = /usr/local
bindir = $(prefix)/bin

-include config.mak

all: $(APP)

clean:
	rm -f $(APP) $(OBJS)

install: $(APP)
	install -Dm 755 $(APP) $(DESTDIR)$(bindir)/$(APP)

$(APP): $(OBJS)

.PHONY: all clean
