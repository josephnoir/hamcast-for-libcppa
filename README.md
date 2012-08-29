hamcast-for-libcppa
===================

Libcppa is a C++11 actor model implementation. This module enables the actors to communicate via multicast using hamcast, which is a scalable multicast implementation.


Dependencies
------------

To use hamcast-for-libcppa you need libcppa, hamcast and boost.

* __libcppa__: https://github.com/Neverlord/libcppa
* __hamcast__: http://hamcast.realmv6.org/developers
* __boost__: http://www.boost.org/


Compiler
--------

* GCC >= 4.7


Get the source:
--------------

    git clone git://github.com/josephnoir/hamcast-for-libcppa.git
    cd hamcast-for-libcppa


Build the library
-----------------

    ./configure --with-gcc=<your-gcc4.7-compiler>
    make


Operating Systems
-----------------

Tested on OSX, but should work on Linux too.


Usage
-----

Besides including the header you need to add the hamcast group module to libcppa:

    group::add_module(make_hamcast_group_module());

