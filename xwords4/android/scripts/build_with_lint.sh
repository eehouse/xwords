#!/bin/sh

# invoke the build process with javac lint features enabled

ant "-Djava.compilerargs=-Xlint:unchecked -Xlint:deprecation" clean debug
