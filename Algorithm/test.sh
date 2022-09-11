#!/bin/bash
g++ period.cpp
#25 rsus
printf "%s\n%s\n%s\n" "vehicledata/250vehicle" "rsucreation/25rsu.txt" "10" | /usr/bin/time ./a.out >> test.txt 2>&1
