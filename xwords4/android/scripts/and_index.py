#!/usr/bin/python3

import os

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
       <style>
          div.ex {
             margin: 1em;
             margin-top: 3em;
             font-family:arial,helvetica;
          }
       </style>
  </head>
  <body>
"""

def printTail():
    return '</body></html>'

def printAndroid(appName, params, scheme):
    return """
<div class="ex">

<p>You just clicked a link in a {appName} invitation, but you should not be
seeing this page!</p>
<hr>
<p>One of three things went wrong:</p>
<ol>
    <li>You don't have {appName} installed</li>

    <div><b>What to do:</b> Install <a
    href="market://search?q=pname:org.eehouse.android.xw4">via Google's App
    Store</a> or <a href="https://eehouse.org/latest.apk">directly
    from the author's site</a> (sideloading)</div>

    <li>When you were asked to choose between a browser and {appName}
        you chose the browser</li>

    <div><b>What to do:</b> Click the invitation link again, and this time
    choose {appName}</div>

    <li>You're on <em>Android 11</em>, where there appears to be a new
    bug that prevents you being given a choice!</li>

    <div><b>What to do:</b> Tap <a href="{scheme}://?{params}">this link</a>
    to launch {appName}</div>
 </ol>

<hr>
<p>Have fun. And as always, <a href="mailto:xwords@eehouse.org">let
me know</a> if you have problems or suggestions.</p>
</div>
""".format(appName=appName, params=params, scheme=scheme)

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

    typ = os.path.basename(os.path.dirname(req.filename))
    if 'andd' == typ:
        appName = 'CrossDbg'
        scheme = 'newxwgamed'
    else:
        appName = 'CrossWords'
        scheme = 'newxwgame'

    agent = req.headers_in.get('User-Agent', None)
    if agent and 'Android' in agent:
        str += printAndroid(appName, req.args, scheme)
    else:
        str += printNonAndroid(agent)

    str += printTail()
    return str
