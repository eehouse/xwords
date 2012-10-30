#!/usr/bin/python

from mod_python import apache

def a(req):
    return """
<html>
<head>
<link rel="stylesheet" type="text/css" href="/xw4mobile.css" />
<meta http-equiv="Content-Type" content="text/html; charset=iso-8859-15">
<title>What gibberish is my friend sending me?</title>
</head>

<body>
  <h3>Gibberish Text explained</h3>

  <p>You probably reached this page because a friend sent you a text
  message with this URL and a bunch of gibberish.</p>

  <p>Well, that gibberish was meant for an Android app called 
    <a href="http://xwords.sf.net/android.php">Crosswords</a>, and your
    friend hoped you had it installed and would want to play.  If you
    like,

    <span style="color:red">
      AND YOU HAVE AN UNLIMITED TEXT MESSAGING PLAN, 
    </span>

    you can download it via the button below.  It's free (costs you neither
    money nor time with annoying advertising) and open-source -- and
    besides, your friend is recommending it. :-)</p>

  <div>
    <form method="get" action="http://eehouse.org/market_redir.php">
      <input type="submit" value="Install Crosswords via Google Play"/>
    </form>
  </div>

  <p>(If you don't have access to the Google Play store from your
    device you can
    instead <a href="http://eehouse.org/sms_latest.apk">download
    Crosswords directly</a>.)
  </p>

  <p style="color:red"> Do not play Crosswords via SMS unless you
    have an unlimited Text Messaging plan with your cellular carrier.
    When communicating via SMS Crosswords can send and receive 50-100
    messages in the course of a single game. If each one costs you 25 or
    50 cents -- well, do the math.</p>

  <p>If you don&apos;t have an unlimited texting plan, you can
    still <a href="http://eehouse.org/market_redir.php">install
    Crosswords</a> and play with your friend over the internet. Just
    don't enable the SMS feature. (And don't worry: that's impossible
    to do accidentally.)</p>

</body> </html>
"""
