<!-- -*- mode: sgml; -*- -->
<?php

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

$localurl = "$scheme://$host?room=$room&lang=$lang&np=$np";
if ( $id != "" ) {
    $localurl .= "&id=$id";
}
if ( $wl != "" ) {
    $localurl .= "&wl=$wl";
}

if ( $onAndroid ) {
print <<<EOF

<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0 Transitional//EN">
<html>
<head>
<link rel="stylesheet" type="text/css" href="/xw4mobile.css" />
<title>Crosswords Invite redirect</title>
</head>
    <body>

    <div align="center">
    <img src="./icon48x48.png">

    <h1><a href="$localurl">Tap this link to launch Crosswords with
      your new game.</a>
    </h1>
    <p>If this fails it&apos;s probably because you don&apos;t have a new enough
      version of Crosswords installed.
    </p>

    <img src="./icon48x48.png">
    </div>
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
    <img src="./icon48x48.png">
  </div>
  <p>This page is meant to be viewed on a browser on your Android
  device.  Please open the email that sent you here on that device to
  complete the invitation process.
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
