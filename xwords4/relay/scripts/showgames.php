<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01//EN">
<html>
<head></head>
<body>

<table border="3">

<?php
include "pwd.php";

$cols = array( "array_upper(nperdevice,1)", 
               "dead", 
               "room", 
               "lang", 
               "ntotal", 
               "nperdevice", 
               "nsent" );

$sql = "SELECT " . join(",",$cols) . " FROM games";
echo "<p>$sql</p>";

echo "<tr>";
foreach ( $cols as $index => $col ) {
    echo "<th>$col</th>";
}
echo "</tr>\n";

$db = pg_connect("host=localhost dbname=xwgames user=postgres password=$pwd");

if (!$db) {
    die( "Error in connection: " . pg_last_error() );
}

 // execute query
$result = pg_query($db, $sql);
if (!$result) {
    die( "Error in SQL query: " . pg_last_error() );
}

// iterate over result set
// print each row
while ( $row = pg_fetch_array($result) ) {
    echo "<tr>";
    foreach ( $cols as $index => $col ) {
        echo "<td>" . "$row[$index]" . "</td>";
    }
    echo "</tr>\n";
}

/* cleanup */
pg_free_result( $result );
pg_close( $db );

?>

</table>
</body>
</html>
