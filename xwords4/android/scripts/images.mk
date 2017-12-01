# -*- mode: Makefile -*-

IMG_SRC = ./img_src
IMG_DEST = app/src/main/res
PARAMS = -transparent white -negate

# $(IMG_DEST)/drawable/%_gen.png:
# 	pwd
# 	touch $@

# $(IMG_DEST)/drawable/%_gen.png: $(IMG_SRC)/%.svg
# 	convert -extent 48x48 $< $@

$(IMG_DEST)/drawable-xhdpi/%__gen.png: $(IMG_SRC)/%.svg
	convert $(PARAMS) -scale 64x64 $< $@

$(IMG_DEST)/drawable-mdpi/%__gen.png: $(IMG_SRC)/%.svg
	convert $(PARAMS) -scale 32x32 $< $@

$(IMG_DEST)/drawable-hdpi/%__gen.png: $(IMG_SRC)/%.svg
	convert $(PARAMS) -scale 48x48 $< $@

# Build have-chat badge using R.color.dull_green
$(IMG_DEST)/drawable/green_chat__gen.png: $(IMG_DEST)/drawable/stat_notify_chat.png
	convert -fill '#00AF00' -colorize 50% $< $@
