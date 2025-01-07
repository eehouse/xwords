# -*- mode: Makefile; -*-

src/%/res/values/strings.xml: src/main/res/values/strings.xml
	@mkdir -p $(shell dirname $@)
	@sed \
		-e "s,CrossWords,$(APPNAME),g" \
		< $< > $@

src/%/res/values/tmpstrings.xml: src/main/res/values/tmpstrings.xml
	@mkdir -p $(shell dirname $@)
	@sed \
		-e "s,CrossWords,$(APPNAME),g" \
		< $< > $@
