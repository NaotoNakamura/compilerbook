#!/bin/bash
assert() {
  expected="$1"
  input="$2"

  ./9cc "$input" > tmp.s
  cc -o tmp tmp.s
  ./tmp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "$input => $actual"
  else
    echo "$input => $expected expected, but got $actual"
    exit 1
  fi
}

assert 0 '0;'
assert 10 '10;'
assert 30 '10+20;'
assert 20 '10+20-10;'
assert 41 '12 + 34 - 5;'

assert 7 '1+2*3;'
assert 15 '5*(9-6);'
assert 4 '(3+5)/2;'
assert 10 '-10+20;'

assert 0 '0==1;'
assert 1 '42==42;'
assert 1 '0!=1;'
assert 0 '42!=42;'
assert 1 '0<1;'
assert 0 '1<1;'
assert 0 '2<1;'
assert 1 '0<=1;'
assert 1 '1<=1;'
assert 0 '2<=1;'
assert 1 '1>0;'
assert 0 '1>1;'
assert 0 '1>2;'
assert 1 '1>=0;'
assert 1 '1>=1;'
assert 0 '1>=2;'

assert 1 '1+10<20;'

assert 3 "a = 3;"
assert 2 "a = 1; a * 2;"
assert 14 "a = 3;
b = 5 * 6 - 8;
a + b / 2;"
assert 3 "a = 3;
b = a;
b;"

assert 1 "a = b = 1;"
assert 1 "a = 1; b = a;"
assert 6 "foo = 3; bar = 3; foo + bar;"

assert 10 "return(10);"
assert 5 "return 5; return 8;"
assert 8 "returnx; return 8;"

assert 10 "if (1) 10;"
assert 15 "if(1) 15;"
assert 20 "a = 5; if (a == 5) 20;"

assert 20 "if (1 == 2) 10; else 20;"

assert 21 "if (1 == 2) 10; else a = 21;"

assert 4 "
i = 0;
while (i <= 3)
 i = i + 1;
 i;
"

assert 4 "for (i = 1; i <= 3; i = i + 1) i;"

echo OK
