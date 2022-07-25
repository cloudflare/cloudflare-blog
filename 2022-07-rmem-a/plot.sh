#!/usr/bin/gnuplot -c
set title @ARG2
set terminal pngcairo transparent enhanced linewidth 2 font 'Helvetica,15' size 1000, 600 background rgb 'white'
set xtics nomirror rotate by -45 ##scale 0 offset 4 #
set ytics nomirror
set grid ytics
set grid xtics
set ylabel "Bytes"
set datafile separator ","
set output ARG3
set key autotitle columnheader
set key outside
set termoption enhanced
set key title @ARG4
set tics font ",12"
set y2range [0.2:16]

stats  ARG1 using 7:7 nooutput
a=STATS_max_y
print "STATS_max_y: ", a
stats  ARG1 using 8:8 nooutput
b=STATS_max_y
print "STATS_max_y: ", b
c=a > b ? a : b

stats ARG1 nooutput
print "STATS_columns: ", STATS_columns

#set multiplot layout 2,1

maxx= STATS_max_x #> 500 ? 500 : STATS_max_x

set xrange [0:maxx]

set lmargin at screen 0.15
set rmargin at screen 0.70

set format y '%.1s%cB'

set style fill transparent solid 0.5 noborder

delta_v(x) = ( vD = x - old_v, old_v = x, vD)
old_v = NaN

#unset xlabel
set ytics a/4
set yrange [0:c*1.15]
plot \
    for [i=7:STATS_columns] ARG1 u 1:i with lines , \
    ARG1 u 1:5 with lines lt 1 dt 3 lc rgb 'black',\
    ARG1 u 1:6 with lines lt 1 dt 2 lc rgb 'red'

    # ARG1 u 1:(delta_v($7) < 1 ? NaN : 1):(0):(c) with vectors nohead noborder lc  rgb "0xe00000FF" lw 3  notitle





# set xlabel "Packets"
# unset format y
# unset title
# set key title ""
# unset ylabel
# unset ytics 
# set yrange [0:2]
# plot \
#   ARG1 u 1:($2 > 0 ? 1 : 0) with line lc rgb "pink",  \
#   ARG1 u 1:($3 > 0 ? $3 : NaN) with points pt 6 ps 2 lc rgb "red", \
#   ARG1 u 1:($4 > 0 ? $4 : NaN) with points pt 2 lc rgb "red"
# #  ARG1 u 1:($5 > 8000 ? 8 : $5/1000) with lines
# unset multiplot
