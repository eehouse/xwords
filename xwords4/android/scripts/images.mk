# -*- mode: Makefile -*-

IMG_SRC = ./img_src
IMG_DEST = app/src/main/res
PARAMS = -transparent white -negate

SRC_SVGS = \
	archive.svg \
	clear_all.svg \
	content_copy.svg \
	content_discard.svg \
	content_new_net.svg \
	content_new_solo.svg \
	dict.svg \
	download.svg \
	email.svg \
	new_group.svg \
	prefs.svg \
	relabel.svg \
	rematch.svg \
	reset.svg \
	save.svg \
	search.svg \
	select_all.svg \
	send.svg \
	trade.svg \
	untrade.svg \
	check_circle.svg \

XHDPI_IMGS:=$(foreach img,$(SRC_SVGS:.svg=__gen.png),$(IMG_DEST)/drawable-xhdpi/$(img))
MDPI_IMGS:=$(foreach img,$(SRC_SVGS:.svg=__gen.png),$(IMG_DEST)/drawable-mdpi/$(img))
HDPI_IMGS:=$(foreach img,$(SRC_SVGS:.svg=__gen.png),$(IMG_DEST)/drawable-hdpi/$(img))

all: $(XHDPI_IMGS) $(MDPI_IMGS) $(HDPI_IMGS)

clean:
	rm -f $(XHDPI_IMGS) $(MDPI_IMGS) $(HDPI_IMGS)

$(IMG_DEST)/drawable-xhdpi/%__gen.png: $(IMG_SRC)/%.svg
	convert $(PARAMS) -scale 64x64 $< $@

$(IMG_DEST)/drawable-mdpi/%__gen.png: $(IMG_SRC)/%.svg
	convert $(PARAMS) -scale 32x32 $< $@

$(IMG_DEST)/drawable-hdpi/%__gen.png: $(IMG_SRC)/%.svg
	convert $(PARAMS) -scale 48x48 $< $@

# Build have-chat badge using R.color.dull_green
$(IMG_DEST)/drawable/green_chat__gen.png: $(IMG_DEST)/drawable/stat_notify_chat.png
	convert -fill '#00AF00' -colorize 50% $< $@
