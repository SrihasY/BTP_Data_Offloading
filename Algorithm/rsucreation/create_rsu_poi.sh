#!/bin/bash
if [ $# -eq 0 ]
  then
    echo "Filename not supplied."
    exit
fi
rsu=0
printf "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" > rsu.poi.xml
printf "<additional xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:noNamespaceSchemaLocation=\"http://sumo.dlr.de/xsd/additional_file.xsd\">" > rsu.poi.xml
while read x y; do
    printf "\t<poi id=\"rsu_%s\" color=\"blue\" x=\"%s\" y=\"%s\"/> \n" "$rsu" "$x" "$y" >> rsu.poi.xml
    ((rsu=rsu+1))
done < $1
printf "</additional>" >> rsu.poi.xml