# CrossWords
An open source implementation of the rules of Scrabble(tm) for handhelds

# What I'm working on (updated March 2026)

While I'm doing incremental tweaks and bug-fixes on the main branch, my focus is the gameref branch, a significant rewrite that moves
a lot of functionality from Android into common (cross-platform) code. This makes the app much easier to improve and maintain and test,
and should also significantly reduce the effort required to produce, say, an iOS version. The gameref branch is nearing feature-equivalence with
the main branch, but is still months away from release.

Other priorities:
* Get back on the Google Play Store
* Finish support of Duplicate-mode play

# CrossDeb
CrossDeb is a separate Android app built from the same source as
CrossWords. Separate in that it can be installed alongside
CrossWords. It's rebuilt periodically whenever a significant change is
made to the main branch here on GitHub. And if installed, it will
notice and offer to install the newest build. Installing CrossDeb is
the best way to keep up with development and to test translations done
through Weblate.

Get CrossDeb at https://eehouse.org/dbg.apk (Tap the link on your
Android phone to install. You may be prompted to enable side-loading,
or to confirm that side-loading from eehouse.org is ok.)

# CrogrDbg
CrogrDbg is a test build of the gameref branch. I'm now finishing the
changes needed to allow it to run and be upgraded separately like CrossDeb.
Soon it will be available at https://eehouse.org/grdbg.apk
