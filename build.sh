#!/bin/sh
gcc -m64 -Og -g ttreader.cpp -o ttreader -lpthread -lstdc++ -lusb-1.0 -I include -L libs/linux libs/linux/libtimetag64.so
