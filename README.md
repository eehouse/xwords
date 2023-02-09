# CrossWords
An open source implementation of the rules of Scrabble(tm) for handhelds

# What I'm working on (updated 9 Feb 2023)

## *Next release*: reduce the number of mqtt messages when both sender and receiver are up-to-date. 
## *Status*: in testing

After that:
* Get back on the Google Play Store
* Finish support of Duplicate-mode play
* Enable rematch of three- and four-device games

# CrossDbg
CrossDbg is a separate Android app built from the same source as
CrossWords. Separate in that it can be installed alongside
CrossWords. It's rebuilt automatically (on TravisCI, thanks to their
support for free software) every time a change is made to
android_branch here on GitHub. And if installed, it will notice and
offer to install a newer build. Installing CrossDbg is the best way to
keep up with development and to test translations done through
Weblate.

Get CrossDbg at https://eehouse.org/dbg.apk (Tap the link on your
Android phone to install. You may be prompted to enable side-loading,
or to confirm that side-loading from eehouse.org is ok.)
