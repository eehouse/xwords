<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01//EN">
<html>
<head></head>
<body>

<?php
include "pwd.php";

class Column {
    public $colname;
    private $isArray;
    private $converter;

    function __construct( $name, $converter, $isArray ) {
        $this->colname = $name;
        $this->isArray = $isArray;
        $this->converter = $converter;
    }

    function printTD( $row, $nrows, $colData ) {
        if ( !$this->isArray && 0 < $row ) {
            /* do nothing */
        } else if ( 0 == $row && !$this->isArray ) {
            $td = call_user_func( $this->converter, $colData );
            echo "<td rowspan=$nrows>$td</td>";
        } else {
            $td = call_user_func( $this->converter, 
                                  $this->nth_elem( $colData, $row ) );
            echo "<td>$td</td>";
        }
    }

    function nth_elem( $arr, $nn ) {
        $arr = explode(',', trim($arr, '{}'));
        return $arr[$nn];
    }
} /* class Column */

function identity( $str ) {
    return $str;
}

function int_to_lang( $str ) {
    $lang = "unknown";
    switch ( $str ) {
    case 1: $lang = "English"; break;
    case 2: $lang = "French"; break;
    case 3: $lang = "German"; break;
    case 4: $lang = "Turkish"; break;
    case 5: $lang = "Arabic"; break;
    case 6: $lang = "Spanish"; break;
    case 7: $lang = "Swedish"; break;
    case 8: $lang = "Polish"; break;
    case 9: $lang = "Danish"; break;
    case 10: $lang = "Italian"; break;
    case 11: $lang = "Dutch"; break;
    case 12: $lang = "Catalan"; break;
    case 13: $lang = "Portuguese"; break;
    case 15: $lang = "Russian"; break;
    case 17: $lang = "Czech"; break;
    case 18: $lang = "Greek"; break;
    case 19: $lang = "Slovak"; break;
    }
    return $lang;
}

function ip_to_host($ip) {
    if ( 0 < strlen( $ip ) ) {
        $ip = gethostbyaddr($ip);
    }
    return $ip;
}

function strip_quotes($str) {
    return trim( $str, '"');
}

$cols = array( new Column("dead", "identity", false ), 
               new Column("room", "identity", false ), 
               new Column("lang", "int_to_lang", false ), 
               new Column("ntotal", "identity", false ), 
               new Column("nperdevice", "identity", true ), 
               new Column("ack", "identity", true ), 
               new Column("nsent", "identity", false ),
               new Column("addrs", "ip_to_host", true ), 
               new Column("mtimes", "strip_quotes", true ), 
               );

$colnames = array();
foreach ( $cols as $index => $col ) {
    $colnames[] = $col->colname;
}
$sql = "SELECT " . join( ",", $colnames ) . 
    ", array_upper(nperdevice,1)" .
    " FROM games" .
    " WHERE NOT -NTOTAL = sum_array(nperdevice) LIMIT 200;";
echo "<p>$sql</p>";

echo "\n<table border=\"3\">\n";

echo "<tr>";
/* index has no header */
echo "<th>&nbsp;</th>";
foreach ( $cols as $index => $col ) {
    echo "<th>$col->colname</th>";
}
echo "</tr>\n";

$db = pg_connect("host=localhost dbname=xwgames user=postgres password=$pwd");

if (!$db) {
    die( "Error in connection: " . pg_last_error() );
}

 // execute query
$result = pg_query( $db, $sql );
if ( !$result ) {
    die( "Error in SQL query: " . pg_last_error() );
}

// iterate over result set
// print each row
$count = 0;
$countIndex = count($cols);
// error_log( "countIndex = " . $countIndex );
while ( $row = pg_fetch_array($result) ) {
    $nrows = $row[$countIndex];
    for ( $devIndex = 0; $devIndex < $nrows; ++$devIndex ) {
        echo "<tr>";
        if ( 0 == $devIndex ) {
            echo "<td rowspan=$nrows>$count</td>";
        }
        foreach ( $cols as $index => $col ) {
            $col->printTD( $devIndex, $nrows, $row[$index] );
        }
        echo "</tr>\n";
    }
    ++$count;
}

/* cleanup */
pg_free_result( $result );
pg_close( $db );

echo "</table>\n";

?>

</body>
</html>
