APPS = usrcheat r4crc

prefix = /usr/local
bindir = $(prefix)/bin

-include config.mak

all: $(APPS)

clean:
	rm -f $(APPS) *.o

install: $(APPS)
	install -Dm 755 usrcheat $(DESTDIR)$(bindir)/usrcheat
	install -Dm 755 r4crc $(DESTDIR)$(bindir)/r4crc

usrcheat: usrcheat.o
r4crc: r4crc.o

.PHONY: all clean install
