#!/bin/bash

# use this on the output of tkindex, all the files that are like this:
# docs/b2/d4/b2d4f313-5352-4253-8dde-cb0ad3d52f49 is not a file we can deal with PDF

for a in $(cat $1)
do
	mkdir -p improvdocs/$(echo $a | cut -f2,3 -d/)
	ocrmypdf --redo-ocr -l nld  $a $(echo $a | sed s/docs/improvdocs/)
done
