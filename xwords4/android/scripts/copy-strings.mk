# -*- mode: Makefile -*-

SRCS = $(wildcard ./res_src/values-*/strings.xml)
TARGETS = $(SRCS:./res_src/value%/strings.xml=app/src/main/res/value%/strings.xml)

.PHONY: all clean

RES_SRC = ./res_src
RES_DEST = ./app/src/main/res

all: $(TARGETS)
	pwd
	echo $(SRCS)
	echo $(TARGETS)

# all: $(TARGETS)

$(RES_DEST)/value%/strings.xml: $(RES_SRC)/value%/strings.xml
	./scripts/copy-strings.py -f $<

clean:
	rm -f $(TARGETS)

# SRC = $(wildcard *.html)
# TAR = $(SRC:.html=.markdown)

# .PHONY: all clean

# all: $(TAR)

# %.markdown: %.html
#     pandoc -o $< $@

# clean:
#     rm -f $(TAR)
