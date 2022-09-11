#!/bin/bash
g++ period.cpp
#25 rsus
printf "%s\n%s\n%s\n" "vehicledata/250vehicle" "rsucreation/25rsu.txt" "10" | /usr/bin/time ./a.out >> results/test0.txt 2>&1
printf "%s\n%s\n%s\n" "vehicledata/500vehicle" "rsucreation/25rsu.txt" "10" | /usr/bin/time ./a.out >> results/test1.txt 2>&1
printf "%s\n%s\n%s\n" "vehicledata/750vehicle" "rsucreation/25rsu.txt" "10" | /usr/bin/time ./a.out >> results/test2.txt 2>&1
#25 rsus period 25
printf "%s\n%s\n%s\n" "vehicledata/250vehicle" "rsucreation/25rsu.txt" "25" | /usr/bin/time ./a.out >> results/test4.txt 2>&1
printf "%s\n%s\n%s\n" "vehicledata/500vehicle" "rsucreation/25rsu.txt" "25" | /usr/bin/time ./a.out >> results/test5.txt 2>&1
printf "%s\n%s\n%s\n" "vehicledata/750vehicle" "rsucreation/25rsu.txt" "25" | /usr/bin/time ./a.out >> results/test6.txt 2>&1

#50rsus
printf "%s\n%s\n%s\n" "vehicledata/250vehicle" "rsucreation/50rsu.txt" "10" | /usr/bin/time ./a.out >> results/test.txt 2>&1
printf "%s\n%s\n%s\n" "vehicledata/500vehicle" "rsucreation/50rsu.txt" "10" | /usr/bin/time ./a.out >> results/test.txt 2>&1
printf "%s\n%s\n%s\n" "vehicledata/750vehicle" "rsucreation/50rsu.txt" "10" | /usr/bin/time ./a.out >> results/test.txt 2>&1
#50rsus period 25
printf "%s\n%s\n%s\n" "vehicledata/250vehicle" "rsucreation/50rsu.txt" "25" | /usr/bin/time ./a.out >> results/test.txt 2>&1
printf "%s\n%s\n%s\n" "vehicledata/500vehicle" "rsucreation/50rsu.txt" "25" | /usr/bin/time ./a.out >> results/test.txt 2>&1
printf "%s\n%s\n%s\n" "vehicledata/750vehicle" "rsucreation/50rsu.txt" "25" | /usr/bin/time ./a.out >> results/test.txt 2>&1

#75rsus
printf "%s\n%s\n%s\n" "vehicledata/250vehicle" "rsucreation/75rsu.txt" "10" | /usr/bin/time ./a.out >> results/test.txt 2>&1
printf "%s\n%s\n%s\n" "vehicledata/500vehicle" "rsucreation/75rsu.txt" "10" | /usr/bin/time ./a.out >> results/test.txt 2>&1
printf "%s\n%s\n%s\n" "vehicledata/750vehicle" "rsucreation/75rsu.txt" "10" | /usr/bin/time ./a.out >> results/test.txt 2>&1
#75rsus period 25
printf "%s\n%s\n%s\n" "vehicledata/250vehicle" "rsucreation/75rsu.txt" "25" | /usr/bin/time ./a.out >> results/test.txt 2>&1
printf "%s\n%s\n%s\n" "vehicledata/500vehicle" "rsucreation/75rsu.txt" "25" | /usr/bin/time ./a.out >> results/test.txt 2>&1
printf "%s\n%s\n%s\n" "vehicledata/750vehicle" "rsucreation/75rsu.txt" "25" | /usr/bin/time ./a.out >> results/test.txt 2>&1