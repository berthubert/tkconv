#!/bin/sh

while true
do
  cp -r /workdir/html /app/html
  cp -r /workdir/partials /app/partials
  cp -r /workdir/build /app/build
  cp /workdir/tk.xslt /app/tk.xslt
  cp /workdir/tk-div.xslt /app/tk-div.xslt
  tkgetxml
  tkconv
  sqlite3 tk.sqlite3 < /workdir/maak-indexen || true
  tkpull
  oppull
  oppull xml
  tkparse
  tkindex
  tkindex --days=14 --tkindex tkindex-small.sqlite3
  echo sleeping
  sleep 60
done
