![C++](https://github.com/MartinErhardt/RoaringRegex/actions/workflows/c-cpp.yml/badge.svg)
![License](https://img.shields.io/static/v1?label=License&message=MPL-2.0&color=blue)

# RoaringRegex
Regex Engine utilizing SIMD and Roaring-Bitmaps

This is largely a worse version of [RE2](https://github.com/google/re2), the concept of which is outlined by Russ Cox in his classic blog post
["Regular Expression Matching Can Be Simple And Fast"](https://swtch.com/~rsc/regexp/regexp1.html). 
Using this NFA-based approach we can only support regular expressions without backreferences, which are specified in the POSIX standard as 
[extended regular expressions](https://pubs.opengroup.org/onlinepubs/009696699/basedefs/xbd_chap09.html), because regular expressions with backreferences no longer describe regular languages in general.
If you're wondering: Yes extended regex is actually less expressive than basic regex in POSIX.

The set of reachable states in the nondeterministic finite automaton after processing a prefix of the input, 
which is a subset of the whole state space, can be represented by a integer if the total number of states does not exceed the word size.
Otherwise SIMD will handle up to 256 states.
This allows to perform operations like unions or intersections of sets in a single instuction 
and it also brings the additional benefit of being very light on cache usage.
For state spaces larger than 256 states (think of the regex a{1,300} and it's minimal DFA) 
RoaringRegex comes with the additional quirk of utilizing Roaring Bitmaps for such sets.
[Roaring Bitmap](https://github.com/RoaringBitmap/CRoaring)s are a set implementation, which determines dynamically if a set is so "sparse", 
that it is more efficient to represent it as a list as opposed to a Bitset.
## Use
Just type
```
$ make
$ ./test_regex # test program to test the library
<type string to match here>
<type regular expresseion here>
# Now it will print the corresponding NFA
# and all lazy matches
```
## Features
Currently implemented features include
 - Character escapes                      \
 - Anchors                                $
 - Recursive processing of subexpressions ()
 - Wildcard                               .
 - Simple bracket expressions             []
 - Concatenation                          {}
 - Union of two regular languages         |
 - Kleeene operators for repeating        *+?

This project is still largely work in progress. Some of the current shortfalls:
 - No proper wchar support
 - Messy headers
 - No doxygen
 - No tests
 - No performance profiling
 - Greedy iterater not greedy (not possible in O(n)?)
 - Costly vtable lookup required by purely virtual function, that exists to abstract set type -> use C++20 concepts?
 - No tweaking of constants in RoaringC: BitSets only used for >4000 entries
