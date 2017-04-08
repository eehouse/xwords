# -*- mode: Makefile; -*-

src/xw4dbg/res/values/strings.xml: src/main/res/values/strings.xml
	@mkdir -p $(shell dirname $@)
	@sed \
		-e "s,CrossWords,$(APPNAME),g" \
		< $< > $@
