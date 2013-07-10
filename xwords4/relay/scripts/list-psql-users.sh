#!/bin/sh

# Show pids of processes holding onto the xwgames DB in some way

# from http://newstrib.com/main.asp?SectionID=2&SubSectionID=27&ArticleID=26068

echo "select pg_class.relname,pg_locks.* from pg_class,pg_locks where pg_class.relfilenode=pg_locks.relation;" | psql xwgames
