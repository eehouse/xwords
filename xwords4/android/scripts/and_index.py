#!/usr/bin/python3

try:
    from mod_python import apache
    apacheAvailable = True
except ImportError:
    apacheAvailable = False

def printHead():
    return """<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0 Transitional//EN">
<html>
  <head>
    <link rel="stylesheet" type="text/css" href="/xw4mobile.css" />
    <title>CrossWords Invite redirect</title>
  </head>
  <body>
"""

def printTail():
    return '</body></html>'

def printAndroid():
    return """
<div>

<p>You should not be seeing this page.</p>

<p>One of two things went wrong:
    <div>&bull; You don't have CrossWords installed</div>
    <div>OR</div>
    <div>&bull; When you clicked on a link and were asked to choose between a
        browser and CrossWords you chose the browser.</div>
</p>

<p>In the first case, install the latest CrossWords, either <a
href="market://search?q=pname:org.eehouse.android.xw4">via the Google
Play store</a> or <a href="https://eehouse.org/latest.apk">directly
from the author's site</a> (sideloading). After the install is
finished go back to the invite email (or text) and tap the link
again.</p>

<p>Otherwise, hit your browser&apos;s back button, click the
invitation link again, and this time choose CrossWords to handle
it.</p>

<p>(If you get tired of having to having to make that choice, Android
will allow you to make CrossWords the default.  If you do that
CrossWords will be launched for all URLs that start with
"https://eehouse.org/and/" -- not all URLs of any type.)</p>

<p>Have fun. And as always, <a href="mailto:xwords@eehouse.org">let
me know</a> if you have problems or suggestions.</p>
</div>
"""

def printNonAndroid(agent):
    subject = 'Android device not identified'
    body = 'My browser is running on an android device but says its' \
           + ' user agent is: \"{agent}\".  Please fix your website' \
           + ' to recognize this as an Android browser'.format(agent=agent)
    fmt = """
<div>
  <p>This page is meant to be viewed on an Android device.</p>
  <hr>
    <p>(If you <em>are</em> viewing this on an Android device,
      you&apos;ve found a bug!  Please <a href="mailto:
      xwords@eehouse.org?subject={subject}&body={body}\n\n">email me</a>.
    </p>
</div>
"""
    return fmt.format(subject=subject, body=body)


def index(req):
    str = printHead()
    agent = req.headers_in.get('User-Agent', None)
    if agent and 'Android' in agent:
        str += printAndroid()
    else:
        str += printNonAndroid(agent)
    str += printTail()
    return str
