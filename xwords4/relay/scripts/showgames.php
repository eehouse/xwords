<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01//EN">
 <html>
<head></head>
<body>

<?php

$pwd = "put pwd here";

$db = pg_connect("host=localhost dbname=xwgames user=postgres password=$pwd");

if (!$db) {
    die( "Error in connection: " . pg_last_error() );
}

 // execute query
$sql = "SELECT room,lang FROM games";
$result = pg_query($db, $sql);
if (!$result) {
    die("Error in SQL query: " . pg_last_error());
}

// iterate over result set
// print each row
while ( $row = pg_fetch_array($result) ) {
    echo "<p>room: " . $row[0] . "</p>";
    echo "<p>lang: " . $row[1] . "</p>";
}

// free memory
pg_free_result( $result );

// close connection
pg_close( $db );

?>

</body>
</html>
