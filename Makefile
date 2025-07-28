CC = gcc
CFLAGS = -O1 -g -Ilib/cJSON
LDFLAGS =
LDLIBS = -lpthread -lcurl
SCDOC = scdoc
PANDOC = pandoc

# Version can be overridden at build time
ifdef VERSION
CFLAGS += -DMINICODER_VERSION=\"$(VERSION)\"
endif

OBJS = main.o util.o model.o agent.o execute.o spinner.o gc.o string.o agent_commands.o
LIB_OBJS = lib/cJSON/cJSON.o
MAN_PAGES = doc/minicoder.1 doc/minicoder-model-config.5
WEB_PAGES = www/minicoder.1.html www/minicoder-model-config.5.html

all: minicoder

minicoder: $(OBJS) $(LIB_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

man: $(MAN_PAGES)

doc/%.1: doc/%.1.scdoc
	$(SCDOC) < $< > $@

doc/%.5: doc/%.5.scdoc
	$(SCDOC) < $< > $@

web: $(WEB_PAGES)


www/minicoder.1.html: doc/minicoder.1 www/style.css
	./support/man2html.sh doc/minicoder.1 $@

www/minicoder-model-config.5.html: doc/minicoder-model-config.5 www/style.css
	./support/man2html.sh doc/minicoder-model-config.5 $@

install: minicoder $(MAN_PAGES)
	install -Dm755 minicoder $(DESTDIR)$(PREFIX)/bin/minicoder
	install -Dm644 doc/minicoder.1 $(DESTDIR)$(PREFIX)/share/man/man1/assist.1
	install -Dm644 doc/minicoder-model-config.5 $(DESTDIR)$(PREFIX)/share/man/man5/minicoder-model-config.5

clean:
	rm -f minicoder $(OBJS) $(LIB_OBJS) $(MAN_PAGES) $(WEB_PAGES)

.PHONY: all clean install man web