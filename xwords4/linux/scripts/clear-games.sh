#!/bin/sh

echo "delete from pairs where key like 'games/%';" | sqlite3 xwgames.sqldb
echo "delete from pairs where key like 'groups/%';" | sqlite3 xwgames.sqldb
echo "delete from pairs where key like 'gmgr/state';" | sqlite3 xwgames.sqldb
