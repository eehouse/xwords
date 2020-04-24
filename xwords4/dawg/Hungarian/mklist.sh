#!/bin/sh

set -e -u

# from: https://github.com/laszlonemeth/magyarispell.git
DIR=/home/eehouse/dev/git/magyarispell/szotar/alap

cat ${DIR}/fonev.1 ${DIR}/melleknev.1 ${DIR}/ige_alanyi.1 ${DIR}/ige_targy.1 ${DIR}/ragozatlan.2 |\
	sed -e 's/#.*$//' -e 's/\[.*$//' -e 's/ .*$//' |\
	grep -v '^$' |\
	sort -u > hungarian_wordlist.txt
