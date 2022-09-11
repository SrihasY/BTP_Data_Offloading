#!/bin/bash
g++ programs/vrate5.cpp
parallel -j4 :::: cmds/vrate5/cmds0.txt
parallel -j4 :::: cmds/vrate5/cmds1.txt
parallel -j4 :::: cmds/vrate5/cmds2.txt
parallel -j4 :::: cmds/vrate5/cmds3.txt

g++ programs/vrate5_rsu4.cpp
parallel -j4 :::: cmds/vrate5_rsu4/cmds0.txt
parallel -j4 :::: cmds/vrate5_rsu4/cmds1.txt
parallel -j4 :::: cmds/vrate5_rsu4/cmds2.txt
parallel -j4 :::: cmds/vrate5_rsu4/cmds3.txt

g++ programs/vrate10.cpp
parallel -j4 :::: cmds/vrate10/cmds0.txt
parallel -j4 :::: cmds/vrate10/cmds1.txt
parallel -j4 :::: cmds/vrate10/cmds2.txt
parallel -j4 :::: cmds/vrate10/cmds3.txt

g++ programs/vrate10_rsu4.cpp
parallel -j4 :::: cmds/vrate10_rsu4/cmds0.txt
parallel -j4 :::: cmds/vrate10_rsu4/cmds1.txt
parallel -j4 :::: cmds/vrate10_rsu4/cmds2.txt
parallel -j4 :::: cmds/vrate10_rsu4/cmds3.txt