#!/usr/bin/gnuplot -c
# data.csv "xeon" "numbers.png" "cycles" "block size"
set title @ARG2
set terminal pngcairo transparent enhanced linewidth 2 font 'Helvetica,15' size 1000, 600 background rgb 'white'
set xtics nomirror rotate by -45 ##scale 0 offset 4 #
set ytics nomirror
set grid ytics
set grid xtics
set ylabel "Latency in ".ARG4
set xlabel "Number of blocks"
set datafile separator ","
set output ARG3
set key autotitle columnheader
set key outside
set key title ARG5
set tics font ",12"
set xtics 256
set yrange [0:]
stats ARG1 nooutput
print "STATS_columns: ", STATS_columns
plot for [i=2:STATS_columns] ARG1 u 1:i with lines

