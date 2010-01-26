#!/bin/sh

check_add () {
    STRING=$1
	PAT="<string name=\"$STRING\">.*</string>"
    if ! grep -q "$PAT" res/values/strings.xml; then
        echo "<string name=\"$STRING\">$STRING</string>"
    fi
}


BASE=$(dirname $0)
cd $BASE/../XWords4

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

for JAVA_FILE in $(find src -name '*.java'); do
    for STRING in $(grep -E 'R\.string\.' $JAVA_FILE | sed 's/^.*R\.string\.\([a-z_]*\).*$/\1/'); do
        check_add $STRING
    done
done

