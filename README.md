# Homepage #

[http://www.eecg.toronto.edu/~jayar/software/GENIE]

# Compilation #

## Visual Studio ##

The solution files are for version 15.0.
Just build the solution in the vc directory.

## GCC ##

Run make.

Requires a recent version that supports C++11. Currently depends on boost-regex, but only because glibc++ had insufficient support for std::regex at the time. If this has been fixed, edit regex.h to make it use C++11's regex instead of Boost's.

## Documentation ##

The Lua API docs are built with [Ldoc](https://github.com/stevedonovan/LDoc).