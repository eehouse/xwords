<?php

// script to work around URLs with custom schemes not being clickable in
// Android's SMS app.  It runs on my server and SMS messages hold links to it
// that it then redirects to the passed-in scheme.

$scheme = "newxwgame";
$host = "10.0.2.2";
$lang = $_REQUEST["lang"];
$room = $_REQUEST["room"];
$np = $_REQUEST["np"];

print <<<EOF

<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0 Transitional//EN">
<html>
<head>
<title>Crosswords SMS redirect</title>
<meta http-equiv="REFRESH"
    content="0;url=$scheme://$host?room=$room&lang=$lang&np=$np">
</head>
    <body>
    <p>redirecting to Crosswords....</p>

    <p>If this fails because you don't have Crosswords installed <a
href="http://eehouse.org/xw4/android/Xwords_latest.apk">tap
here</a>.</p>

    </body>

    </html>

EOF;

/*     <p>If this fails because you don't have Crosswords installed <a */
/* href="https://market.android.com/search?q=pname:org.eehouse.android.xw4">tap */
/* here</a>.</p> */

?>
