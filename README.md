# Multiplayer Front-End Client Shared Library

---
CPS2008 *Operating Systems and Systems Programming 2*

Assignment for the Academic year 2020/1

Xandru Mifsud (0173498M), B.Sc. (Hons) Mathematics and Computer Science

---

This repository defines the shared library that should be used for a front-end implementation, in order to communicate
with our [game server implementation](https://github.com/xmif1/CPS2008_Tetris_Server). For an example front-end implementation,
kindly see our [multiplayer tetris](https://github.com/xmif1/CPS2008_Tetris_FrontEnd) variant.

The shared library in particular exposes a (primitve) API, which when used along with
the appropriately defined mutexes, allows for the easy implementation of a thread-safe
front-end. To this extent, the API exposes a number of thread-safe functions, as well as thread-safe getters
and setters for global defined shared variables.

One major design decision made is to delegate thread-management to the front-end implementation, i.e. in particular we do
not at any point spawn, cancel etc threads in any of the shared library functions. The idea is to make debugging easier.
The rationale behind this, as well as to other thread-management considerations made, is outlined in the introductory
chapter of the assignment report.

## Requirements

This program is intended to run on Linux platforms, in particular on Debian-based systems such as Ubuntu and
Lubuntu, on which we have tested the implementation with relative success.

For installation, ```cmake``` version 3.17+ is required.

## Installation Instructions

Clone the repository, and ```cd``` into the project directory. Then run:

1. ```cmake .```
2. ```sudo make install```
3. ```sudo ldconfig```

where the last two instructions install the shared library on your system, ready for use in 
particular by any front-end implementation.

## Known Issues

The library in particular supports the setting up of a P2P network between clients joining
the same game session. There are a number of known issues during P2P setup which can occur, albeit they seem to be sporadic. 

These issues are outlined in detail in the *Testing* and the *Limitations and Future Improvements* chapters of the
assignment report. In summary:

1. Joining a new game session shortly after the completion of another game sessoion may result in a socket binding error,
   if the port allocated by the server to the client on which to accept P2P connections is the same as the previous game
   session.
   
2. A client may send a ```P2P_READY``` message to the server, but it may never receive a ```START_GAME``` message, resulting
   in an indefinite wait. We suspect this issue to lie within the server implementation and how we keep track of the number
   of ```P2P_READY``` messages received across multiple threads, using ```pthread_cond_wait``` and ```pthread_cond_broadcast```.
   Indeed, we believe that some mutex lock (that of the ```game_session``` struct) is not being released as expected,
   causing a deadlock in the server such that it never sends a ```START_GAME``` message.