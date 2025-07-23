CC = gcc
CFLAGS = -O1 -Ilib/tgc -Ilib/cJSON
LDFLAGS =
LDLIBS = -lpthread -lcurl
SCDOC = scdoc
PANDOC = pandoc

OBJS = main.o util.o model.o agent.o shell.o
LIB_OBJS = lib/tgc/tgc.o lib/cJSON/cJSON.o
MAN_PAGES = doc/assist.1 doc/assist-model-config.5
WEB_PAGES = www/assist.1.html www/assist-model-config.5.html

all: assist

assist: $(OBJS) $(LIB_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

man: $(MAN_PAGES)

doc/%.1: doc/%.1.scdoc
	$(SCDOC) < $< > $@

doc/%.5: doc/%.5.scdoc
	$(SCDOC) < $< > $@

web: $(WEB_PAGES)


www/assist.1.html: doc/assist.1 www/style.css
	@mkdir -p www
	mandoc -T html -O style=style.css doc/assist.1 > $@.tmp && \
	sed -e 's|<b>assist-model-config</b>(5)|<a href="assist-model-config.5.html"><b>assist-model-config</b>(5)</a>|g' \
	    -e 's|<b>assist</b>(1)|<a href="assist.1.html"><b>assist</b>(1)</a>|g' $@.tmp > $@ && \
	rm -f $@.tmp || \
	(man2html < doc/assist.1 | sed 's|<HTML>|<!DOCTYPE html><html><head><link rel="stylesheet" href="style.css"><title>assist(1)</title></head><body class="markdown-body">|' | sed 's|</HTML>|</body></html>|' > $@)

www/assist-model-config.5.html: doc/assist-model-config.5 www/style.css
	@mkdir -p www
	mandoc -T html -O style=style.css doc/assist-model-config.5 > $@.tmp && \
	sed -e 's|<b>assist</b>(1)|<a href="assist.1.html"><b>assist</b>(1)</a>|g' \
	    -e 's|<b>assist-model-config</b>(5)|<a href="assist-model-config.5.html"><b>assist-model-config</b>(5)</a>|g' \
	    -e 's|<br/>||g' $@.tmp | \
	perl -0pe 's|(<pre>[^<]*</pre>)|my $$pre=$$1; $$pre=~s/\n\n/\n/g; $$pre|ge' > $@ && \
	rm -f $@.tmp || \
	(man2html < doc/assist-model-config.5 | sed 's|<HTML>|<!DOCTYPE html><html><head><link rel="stylesheet" href="style.css"><title>assist-model-config(5)</title></head><body class="markdown-body">|' | sed 's|</HTML>|</body></html>|' > $@)

install: assist $(MAN_PAGES)
	install -Dm755 assist $(DESTDIR)$(PREFIX)/bin/assist
	install -Dm644 doc/assist.1 $(DESTDIR)$(PREFIX)/share/man/man1/assist.1
	install -Dm644 doc/assist-model-config.5 $(DESTDIR)$(PREFIX)/share/man/man5/assist-model-config.5

clean:
	rm -f assist $(OBJS) $(LIB_OBJS) $(MAN_PAGES) $(WEB_PAGES)

.PHONY: all clean install man web