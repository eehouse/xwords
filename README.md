# CrossWords
An open source implementation of the rules of Scrabble(tm) for handhelds

# What I'm working on (updated Dec 2025)

## I'm rewriting a significant portion of the codebase to move functionality from Android into the cross-platform layer, which should make it much easier to support new platforms (e.g. iOS).

## *Next release*: will be based on the gameref branch. Should be a bit faster, but functionally the same as the current release.
## *Status*: in development

After that:
* Get back on the Google Play Store
* Finish support of Duplicate-mode play

# CrossDeb
CrossDeb is a separate Android app built from the same source as
CrossWords. Separate in that it can be installed alongside
CrossWords. If installed, it will notice and offer to install newer
builds as I push them. Installing CrossDeb is the best way to keep up
with development and to test translations done through Weblate.

Get CrossDeb at https://eehouse.org/dbg.apk (Tap the link on your
Android phone to install. You may be prompted to enable side-loading,
or to confirm that side-loading from eehouse.org is ok.)
