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
          <p><em>UNDER CONSTRUCTION -- certain assumptions will go away before the next version ships</em></p>

          <img src="../icon48x48.png"/>
EOF;
}

function printTail() {
print <<<EOF
</body>
</html>
EOF;
}

function printNonAndroid() {
    $subject = "Android device not identified"; 

    $body = htmlentities("My browser is running on an android device but"
                         . " says its user agent is: \"$agent\".  Please fix your script to recognize"
                         . " this as an Android browser.");
    print <<<EOF
        <p>Please open the link that sent you here on an Android device.</p>

        <p>(If you <em>are</em> viewing this on an Android device, you&apos;ve
            found a bug!  Please <a href="mailto:
            xwords@eehouse.org?subject=$subject&body=$body">email me</a> (and be
            sure to leave the user agent string in the email body.) 
       </p>

EOF;
}

function printAndroid() {
print <<<EOF
<p>Hello Kati, Chris, Mana, Deb, and maybe Brynn,</p>

<p>You&apos;ll have come here after clicking a link in an invite
  email.  But you should not be seeing this page.</p>

<p>If you got this page on your device, it means either
  <ul>
    <li> you haven&apos;t installed a copy of Crosswords built after Nov. 25</li>
    <li> OR </li>
    <li> that you have, and when you clicked on the link and were asked to
      choose between a browser and Crosswords you chose the browser.</li>
</ul></p>

<p>In the first
case, <a href="http://eehouse.org/xw4/android/$g_apk">download
and install the latest Crosswords</a>. Then go back to the invite
email (or text) and tap the link again.</p>

<p>In the second, hit your back button, click the link in your invite
email (or text) again, and this time choose Crosswords.</p>

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
    printNonAndroid();
}
printTail();


?>
