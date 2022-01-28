# -*-mode: Makefile; -*-
# Copyright 2002-2020 by Eric House (xwords@eehouse.org).  All rights
# reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

XWLANG=Hungarian
LANGCODE=hu_HU
ENC = UTF-8
TARGET_TYPE ?= WINCE

DICTNOTE = "Derived from szotar/alap/ in https://github.com/laszlonemeth/magyarispell.git"

# Needs only to be defined
ALLOWS_DUPLICATES = TRUE

LANG_SPECIAL_INFO = \
		"CS Cs cS cs" /dev/null /dev/null \
		"GY Gy gY gy" /dev/null /dev/null \
		"LY Ly lY ly" /dev/null /dev/null \
		"NY Ny nY ny" /dev/null /dev/null \
		"SZ Sz sZ sz" /dev/null /dev/null \
		"TY Ty tY ty" /dev/null /dev/null \
		"ZS Zs zS zs" /dev/null /dev/null \

include ../Makefile.langcommon

SRC_COMMIT = 39ee7f3f8631b953d44ed6f12cfe8ae7193fbf13
GIT_DIR = ./magyarispell/szotar/alap/
SRC = $(GIT_DIR)/fonev.1 \
	$(GIT_DIR)/fonev_a.1 \
	$(GIT_DIR)/fonev_ae.1 \
	$(GIT_DIR)/fonev_igemorfo.1 \
	$(GIT_DIR)/fonev_ing.1 \
	$(GIT_DIR)/fonev_jaje.1 \
	$(GIT_DIR)/fonev_jajeae.1 \
	$(GIT_DIR)/fonev_kulon.1 \
	$(GIT_DIR)/fonev_magas.1 \
	$(GIT_DIR)/fonev_mely.1 \
	$(GIT_DIR)/fonev_morfo.2 \
	$(GIT_DIR)/fonev_morfo2.2 \
	$(GIT_DIR)/fonev_oe.1 \
	$(GIT_DIR)/fonev_osszetett.1 \
	$(GIT_DIR)/fonev_vege.1 \
	$(GIT_DIR)/fonev_y_i.1 \
	$(GIT_DIR)/fonev_y_j.1 \
	$(GIT_DIR)/ige_alanyi.1 \
	$(GIT_DIR)/ige_ikes_kiv.1 \
	$(GIT_DIR)/ige_koto.1 \
	$(GIT_DIR)/ige_mely.1 \
	$(GIT_DIR)/ige_morfo.1 \
	$(GIT_DIR)/ige_osszetett.1 \
	$(GIT_DIR)/ige_targy.1 \
	$(GIT_DIR)/ige_tat_kiv.1 \
	$(GIT_DIR)/kotojeles_osszetett.1 \
	$(GIT_DIR)/melleknev-igemorfo.1 \
	$(GIT_DIR)/melleknev-szam.1 \
	$(GIT_DIR)/melleknev.1 \
	$(GIT_DIR)/melleknev_a.1 \
	$(GIT_DIR)/melleknev_ae.1 \
	$(GIT_DIR)/melleknev_ae.1 \
	$(GIT_DIR)/melleknev_igemorfo.1 \
	$(GIT_DIR)/melleknev_ing.1 \
	$(GIT_DIR)/melleknev_jaje.1 \
	$(GIT_DIR)/melleknev_jajeae.1 \
	$(GIT_DIR)/melleknev_mely.1 \
	$(GIT_DIR)/melleknev_nyelv.1 \
	$(GIT_DIR)/melleknev_oe.1 \
	$(GIT_DIR)/melleknev_osszetett.1 \
	$(GIT_DIR)/ragozatlan.2 \



TMP_LIST = tmp_wordlist.txt

PHONY: GIT_TREE

GIT_TREE:
	if [ ! -d $(GIT_DIR) ]; then \
		mkdir -p $(GIT_DIR); \
		git clone https://github.com/laszlonemeth/magyarispell.git $(GIT_DIR); \
	fi
	(cd $(GIT_DIR) && git checkout $(SRC_COMMIT))

$(SRC) : GIT_TREE

$(TMP_LIST): $(SRC)
	cat $^ |\
		sed -e '/^-/d' -e '/^[A-ZÁÉÍÓÖŐÚÜŰ]/d' \
			-e 's/\/.*$$//' -e 's/#.*$$//' -e 's/\[.*$$//' \
			-e 's/\s.*$$//' -e '/_/d' -e '/-/d' |\
		grep -v '^$$' | sort -u > $@

# Filtering wordlist: When a word can be spelled with either a
# double-letter tile or two single-letter tiles, it's in the list with
# both spellings. That's what the longer sed expressions are doing
# (emitting two words)
$(XWLANG)Main.dict.gz: $(TMP_LIST)
	cat $< \
	| tr -d '\r' \
	| grep -v '[1-7]' \
	| sed 's/[[:lower:]]*/\U&/' \
	| sed -e 's,^\(.*\)CS\(.*\)$$,\11\2\n\1CS\2,g' \
	| sed -e 's,GY,2,g' \
	| sed -e 's,LY,3,g' \
	| sed -e 's,NY,4,g' \
	| sed -e 's,^\(.*\)SZ\(.*\)$$,\15\2\n\1SZ\2,g' \
	| sed -e 's,TY,6,g' \
	| sed -e 's,^\(.*\)ZS\(.*\)$$,\17\2\n\1ZS\2,g' \
	| grep '^[1-7AÁBCDEÉFGHIÍJKLMNOÓÖŐPRSTUÚÜŰVZ]\{2,15\}$$' \
	| tr '1234567' '\001\002\003\004\005\006\007' \
	| gzip -c > $@

# Everything but creating of the Main.dict file is inherited from the
# "parent" Makefile.langcommon in the parent directory.

clean: clean_common
	rm -f $(XWLANG)Main.dict.gz *.bin $(XWLANG)*.pdb $(XWLANG)*.seb $(TMP_LIST)
