
: sr 44100 ;
: dur 5 sr * ;

0 0 20 sr * line
0 1 dur line 1 0 dur line join 440 2 dur * osc * 3 sr * schedule
0 1 dur line 1 0 dur line join 440 2 dur * osc * 5 sr * schedule
0 1 dur line 1 0 dur line join 440 2 dur * osc * 7 sr * schedule
0 1 dur line 1 0 dur line join 440 2 dur * osc * 8 sr * schedule

dump
