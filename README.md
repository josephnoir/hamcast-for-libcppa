hamcast-for-libcppa
===================

Libcppa is a C++11 actor model implementation. Hamcast is a scalable multicast implementation. This module allows using hamcast for communication between actors.


Dependencies
------------

To use hamcast-for-libcppa you need libcppa, hamcast and boost.

* __libcppa__: https://github.com/Neverlord/libcppa
* __hamcast__: http://hamcast.realmv6.org/developers
* __boost__: http://www.boost.org/


Compiler
--------

I recommand using gcc 4.7. Meaning you need to compile boost, libcppa and hamcast with gcc 4.7 too.


Get the source:
--------------

    git clone git://github.com/josephnoir/hamcast-for-libcppa.git
    cd hamcast-for-libcppa


Build the library
-----------------

    ./configure --with-gcc=<your gcc4.7 compiler>
    make


Operating Systems
-----------------

Tested on OSX, but should work on Linux too.


Usage
-----

Besides adding the header you need to use the

    group::add_module(group::add_module(make_hamcast_group_module());

method provided by libcppa.

