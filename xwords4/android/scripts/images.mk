# -*- mode: Makefile -*-

IMG_SRC = ./img_src
IMG_DEST = res

# $(IMG_DEST)/drawable/%_gen.png:
# 	pwd
# 	touch $@

# $(IMG_DEST)/drawable/%_gen.png: $(IMG_SRC)/%.svg
# 	convert -extent 48x48 $< $@

$(IMG_DEST)/drawable-xhdpi/%__gen.png: $(IMG_SRC)/%.svg
	convert -negate -scale 64x64 $< $@

$(IMG_DEST)/drawable-mdpi/%__gen.png: $(IMG_SRC)/%.svg
	convert -negate -scale 32x32 $< $@

$(IMG_DEST)/drawable-hdpi/%__gen.png: $(IMG_SRC)/%.svg
	convert -negate -scale 48x48 $< $@
