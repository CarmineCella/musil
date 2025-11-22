# variables and command substitution
set x 5
set y 10
puts sum=[add $x $y]

# if
if [sub $y $x] { puts y> x }

# while + proc
proc fact {n} {
    set r 1;
    while $n {
        set r [mul $r $n];
        set n [sub $n 1];
    };
    puts result=$r
}

fact 5