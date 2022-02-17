# SpiTest

SpiTest is a small utility to test spi request with write and read without releasing the /CS line of
the spibus between the write and the read.

Licence
=======
GPL-3.0

Used Libraries:
===============
Boost

Building:
=========

Installing requirements:
========================
- apt install libboost-all-dev cmake g++

Building:
=========
- git clone https://github.com/schorsch1976/SpiTest.git
- cd SpiTest
- mkdir build
- cd build
- cmake ..
- make

Just start it by ./src/SpiTest
