This file describes how to build dictionaries for the various versions
of Crosswords.

Short version:

For a Palm dictionary, type:

# make -f Makefile.BasEnglish TARGET_TYPE=PALM

which will create BasEnglish2to8.pdb.

For a Franklin or Wince or Linux dictionary, type  

# make -f Makefile.BasEnglish TARGET_TYPE=FRANK

which will create BasEnglish2to8.seb and BasEnglish2to8.xwd.saved.

The .seb file is for the eBookman, and is just a wrapper around an
.xwd file.  Unwrapped .xwd files are for Wince and Linux versions of
Crosswords.  Remove the .saved from the end of the filename.  It's
only there because I haven't figure out how to stop the build system
from deleting .xwd files after making .seb files out of them.



English is unusual in having multiple dictionaries.  In most language
directories there's only one, and so only one Makefile.  So you skip
the -f option to make.

The 2to8 part of the name is a convention meaning that only words from
2 to 8 letters long are included.  2to8 is the default, but you can
explicitly use a different target and the build system will adjust what
words are included.  For example 

# make -f Makefile.BasEnglish TARGET_TYPE=FRANK BasEnglish2to5.xwd

will produce an even smaller dictionary for Wince and Linux.

