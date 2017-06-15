This takes a data stream and converts the radix from x to y. Radices can be anything from 2 up to about 4294967296...

To compile:
  g++ -std=c++11 -Wno-multichar radixstream1.0.cpp -o radix

Example usage:
  cat radix | ./radix fa tx | less # This will let you view the hex dump of the binary itself
  echo "1001" | ./radix fb td # 9
  echo "a4" | ./radix fx tb # 10100100
  echo "a4" | ./radix fx td # 1004 'a' -> '10', '4' -> '04' as opposed to 164

NOTE: This is NOT directly compatible with most base conversion methods. It is a windowed stream converter and cannot use the whole 'number'.
Note: This can be rather easily modified to support practically any radix to infinity by using a big int library that returns the count of bits (i.e. log2) used and all other values as a big int type. Additionally, log2 should be created to support big int type with reasonable precision.
