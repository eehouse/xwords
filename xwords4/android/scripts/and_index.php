<?php

$g_androidStrings = array( "android", );
$g_apk = 'XWords4-release_android_beta_55-39-gbffb231.apk';

function printHead() {
    print <<<EOF

<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0 Transitional//EN">
<html>
  <head>
    <link rel="stylesheet" type="text/css" href="/xw4mobile.css" />
    <title>Crosswords Invite redirect</title>
  </head>
  <body>
    <div class="center">
      <img class="center" src="../icon48x48.png"/>
    </div>
EOF;
}

function printTail() {
print <<<EOF
</body>
</html>
EOF;
}

function printNonAndroid($agent) {
    $subject = "Android device not identified"; 

    $body = htmlentities("My browser is running on an android device but"
                         . " says its user agent is: \"$agent\"."
                         . " Please fix your website to recognize"
                         . " this as an Android browser.");
    print <<<EOF
<div class="center">
  <p>This page is meant to be viewed on an Android device.</p>
  <hr>
    <p>(If you <em>are</em> viewing this on an Android device,
      you&apos;ve found a bug!  Please <a href="mailto:
      xwords@eehouse.org?subject=$subject&body=$body">email me</a>
      (and be sure to leave the user agent string in the email body.)
    </p>
</div>

EOF;
}

function printAndroid() {
print <<<EOF
<div>
<p>You&apos;ll have come here after clicking a link in an email or
  text inviting you to a Crosswords game. But you should not be seeing
  this page.</p>

<p>If you got this page on your device, it means either
  <ul>
    <li>The copy of Crosswords you have is NOT beta 56 or newer (dating from about Dec. 1, 2012).</li>
    <li> OR </li>
    <li> that your copy of Crosswords is new enough <em>BUT</em> that
      when you clicked on the link and were asked to choose between a
      browser and Crosswords you chose the browser.</li>
</ul></p>

<p>In the first case, install the latest Crosswords,
either <a href="market://search?q=pname:org.eehouse.android.xw4">via
the Google Play store</a> or
(sideloading) <a href="https://sourceforge.net/projects/xwords/files/xwords_Android/4.4%20beta%2056/XWords4-release_android_beta_56.apk/download">via
Sourceforge.net</a>. After the install is finished go back to the
invite email (or text) and tap the link again.</p>

<p>In the second case, hit your browser&apos;s back button, click the
link in your invite email (or text) again, and this time let
Crosswords handle it.</p>

<p>(If you get tired of having to having to make that choice, Android
will allow you to make Crosswords the default.  If you do that
Crosswords will be given control of all URLs that start with
"http://eehouse.org/and/" -- not all URLs of any type.)</p>

<p>Have fun.  And as always, <a href="mailto:xwords@eehouse.org">let
me know</a> if you have problems or suggestions.</p>
</div>
<div class="center">
  <img class="center" src="../icon48x48.png"/>
</div>
EOF;
}


/**********************************************************************
 * Main()
 **********************************************************************/
$agent = $_SERVER['HTTP_USER_AGENT'];
$onAndroid = false;
for ( $ii = 0; $ii < count($g_androidStrings) && !$onAndroid; ++$ii ) {
    $needle = $g_androidStrings[$ii];
    $onAndroid = false !== stripos( $agent, $needle );
}
$onFire = false !== stripos( $agent, 'silk' );

printHead();
if ( /*true || */ $onFire || $onAndroid ) {
    printAndroid();
} else {
    printNonAndroid($agent);
}
printTail();


?>
