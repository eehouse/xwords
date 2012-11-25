<!-- -*- mode: sgml; -*- -->
<?php

function langToString( $code ) {
    switch ( $code ) {
    case 1: return "English";
    case 2: return "French";
    case 3: return "German";
    case 4: return "Turkish";
    case 5: return "Arabic";
    case 6: return "Spanish";
    case 7: return "Swedish";
    case 8: return "Polish";
    case 9: return "Danish";
    case 0xA: return "Italian";
    case 0xB: return "Dutch";
    case 0xC: return "Catalan";
    case 0xD: return "Portuguese";

    case 0XF: return "Russian";
    case 0x11: return "Czech";
    case 0x12: return "Greek";
    case 0x13: return "Slovak";
    default:
        return "<unknown>";
    }
}

$g_androidStrings = array( "android", );

$scheme = "newxwgame";
$host = "10.0.2.2";
$lang = $_REQUEST["lang"];
$room = $_REQUEST["room"];
$np = $_REQUEST["np"];
$id = $_REQUEST["id"];
$wl = $_REQUEST["wl"];

$agent = $_SERVER['HTTP_USER_AGENT'];
$onAndroid = false;
for ( $ii = 0; $ii < count($g_androidStrings) && !$onAndroid; ++$ii ) {
    $needle = $g_androidStrings[$ii];
    $onAndroid = 0 != stripos( $agent, $needle );
}
$onFire = 0 != stripos( $agent, 'silk' );

$localurl = "$scheme://$host?room=$room&lang=$lang&np=$np";
if ( $id != "" ) {
    $localurl .= "&id=$id";
}
if ( $wl != "" ) {
    $localurl .= "&wl=$wl";
}

if ( $onAndroid || $onFire ) {
print <<<EOF

<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0 Transitional//EN">
<html>
<head>
<link rel="stylesheet" type="text/css" href="/xw4mobile.css" />
<title>Crosswords Invite redirect</title>
</head>
    <body>

    <div align="center">
    <img src="./icon48x48.png"/>

    <h1><a href="$localurl">Tap this link to launch Crosswords with
      your new game.</a>
    </h1>
    <p>If this fails it&apos;s probably because you don&apos;t have a new enough
      version of Crosswords installed.
    </p>

    <img src="./icon48x48.png"/>
    </div>
    </body>

    </html>

EOF;

} else if ( $onFire ) {
    $langString = langToString($lang);
    $langText = "Make sure the language chosen is $langString";
    if ( '' != $wl ) {
        $langText .= " and the wordlist is $wl.";
    }
    $langText .= " If you don't have a[n] $langString wordlist installed you'll need to do that first.";
print <<<EOF
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0 Transitional//EN">
<html>
<head>
<title>Crosswords Invite redirect</title>
</head>
<body>

<p>It appears you&apos;re running on a Kindle Fire, whose non-standard (from
an Android perspective) OS doesn't support the custom schemes on which
Crosswords invitations depend.  If you want to accept this invitation
you'll need to do it the manual way:

<ol>
<li>Open Crosswords, and navigate to the main Games List screen</li>
<li>Choose &quot;Add game&quot;, either from the menu or the button at the bottom.</li>
<li>Under &quot;New Networked game&quot;, choose &quot;Configure first&quot;.</li>
<li>$langText</li>
<li>As the room name, enter &quot;$room&quot;.</li>
<li>Make sure the total number of players shown is $np and that only one of them is not an &quot;Off-device player&quot;.</li>
<li>Now tap the &quot;Play game&quot; button at the bottom (above the keyboard). Your new game should open and connect.</li>
</ol></p>
<p>I&apos;m sorry this is so complicated.  I&apos;m trying to find a
workaround for this limitation in the Kindle Fire's operating system
but for now this is all I can offer.</p>

<p>(Just in case Amazon&apos;s fixed the
problem, <a href="$localurl">here is the link</a> that should open
your new game.)</p>
</body>
</html>
EOF;
} else { 
$subject = "Android device not identified"; 

$body = htmlentities("My browser is running on an android device but"
. " says its user agent is: \"$agent\".  Please fix your script to recognize"
. " this as an Android browser.");

print <<<EOF
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0 Transitional//EN">
<html>
<head>
<title>Crosswords Invite redirect</title>
</head>
<body>
  <div align="center">
    <img src="./icon48x48.png"/>
  </div>
  <p>This page is meant to be viewed on a browser on your Android
  device.  Please open the email that sent you here on that device and
  revisit this link to complete the invitation process.
  </p>

  <p>(If you <em>are</em> viewing this on an Android device, you've
  found a bug!  Please <a href="mailto:
  xwords@eehouse.org?subject=$subject&body=$body">email me</a> (and be
  sure to leave the user agent string in the email body.) 
  </p>

</body>
</html>

EOF;
}

?>
