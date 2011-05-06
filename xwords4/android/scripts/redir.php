<?php

// script to work around URLs with custom schemes not being clickable in
// Android's SMS app.  It runs on my server and SMS messages hold links to it
// that it then redirects to the passed-in scheme.

$scheme = "newxwgame";
$host = "10.0.2.2";
$lang = $_REQUEST["lang"];
$room = $_REQUEST["room"];

print <<<EOF

<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0 Transitional//EN">
<html>
<head>
<title>Crosswords SMS redirect</title>
<meta http-equiv="REFRESH" 
    content="0;url=$scheme://$host?room=$room&lang=$lang">
</head>
    <body>
    redirecting to Crosswords....
    </body>                                                                                                                                                                                     
    </html>                                                                                                                                                                                     

EOF;
?>
