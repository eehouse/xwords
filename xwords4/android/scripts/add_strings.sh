#!/bin/sh

LOCALES=values

check_add () {
    STRING=$1
    for VALUES in $LOCALES; do
	    PAT="<string name=\"$STRING\">"
        if grep -q "$PAT" res/values/common_rsrc.xml; then
            :
        elif [ ! -f "res/$VALUES/strings.xml" ]; then
            echo "error: res/$VALUES/strings.xml not found" 1>&2
        elif ! grep -q "$PAT" res/$VALUES/strings.xml; then
            echo "<string name=\"$STRING\">$STRING</string>"
        fi
    done
}

usage() {
    echo "usage: $0 [--locale <locale>] [--all]" 1>&2
}

BASE=$(dirname $0)
cd $BASE/../XWords4

while [ -n "$1" ]; do
    case $1 in
        --all)
            for DIR in $(ls -d res/values-*); do
                DIR=$(basename $DIR)
                LOCALES="$LOCALES $DIR"
            done
            ;;
        --locale)
            [ -n "$2" ] || usage
            LOCALES="values-$2"
            shift
            ;;
        *)
            usage
            ;;
    esac
    shift
done

echo "trying $LOCALES" 1>&2

for XML_FILE in $(find res/layout -name '*.xml'); do
    for STRING in $(grep 'android:text=' $XML_FILE | sed 's,^.*"@string/\(.*\)".*$,\1,'); do
        check_add $STRING
    done
done

for XML_FILE in $(find res/menu -name '*.xml'); do
    for STRING in $(grep 'android:title=' $XML_FILE | sed 's,^.*"@string/\(.*\)".*$,\1,'); do
        check_add $STRING
    done
done

for XML_FILE in $(find res/xml -name '*.xml'); do
    for STRING in $(grep 'android:.*="@string/' $XML_FILE | sed 's,^.*"@string/\(.*\)".*$,\1,'); do
        check_add $STRING
    done
done

for JAVA_FILE in $(find src -name '*.java'); do
    for STRING in $(grep -E 'R\.string\.' $JAVA_FILE | sed 's/^.*R\.string\.\([a-z_]*\).*$/\1/'); do
        check_add $STRING
    done
done

