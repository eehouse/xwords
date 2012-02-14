# -*- mode: Makefile; -*-

$(DEST_PATH)/%.java : $(SRC_PATH)/%.java
	@sed \
		-e "s,\(package org.eehouse.android.\)xw4\(.*\);,\1$(VARIANT)\2;," \
		-e "s,\(import org.eehouse.android.\)xw4\(.*\);,\1$(VARIANT)\2;," \
		< $< > $@

$(DEST_PATH)/%.png : $(SRC_PATH)/%.png
	@cp $< $@

$(DEST_PATH)/%.xml : $(SRC_PATH)/%.xml
	@sed \
		-e "s,\(^.*org.eehouse.android.\)xw4\(.*$$\),\1$(VARIANT)\2," \
		< $< > $@

$(DEST_PATH)/%.h : $(SRC_PATH)/%.h
	sed \
		-e "s,\(^.*org/eehouse/android/\)xw4\(.*$$\),\1$(VARIANT)\2," \
		< $< > $@

$(DEST_PATH)/% : $(SRC_PATH)/%
	@cp $< $@
