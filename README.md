usrcheat 1.0.0 (c) rofl0r
=========================

a tool to translate r4/dsone `usrcheat.dat` files into `cheats.xml` format
and vice versa.

the xml format allows one to easily edit the cheat format with a text editor
rather than a clumsy gui app, and it also allows automated updates/workflows.

instead of inventing my own text format, i opted to use the existing
xml format as used by other carts/tools (e.g. nitrohax), to make it more
useful, given that it was designed relatively simply and isn't too ugly
for being XML.

note that the xml parser is totally minimalistic, therefore the format
must be kept precisely as it is (i.e. you can't move e.g. a closing </name>
tag to another line - it needs to be on the same line than the opening tag,
just like the code emitted when converting a .dat to .xml.

the program was written with library usage in mind and it should be straight
forward to turn it into a reusable library.


usage
-----

usrcheat MODE FILEIN FILEOUT
MODE can be either of:
toxml - read FILEIN as usrcheat.dat format, write FILEOUT as xml
todat - read FILEIN as cheat.xml format, write FILEOUT as usrcheat.dat


build
-----

get a C compiler and run `make`.


dependencies
------------

none.


license
-------

GPL v3.


credits
-------

i've been looking at some prior art while mostly reverse engineering the
format.
the following 2 repos have been helpful:

https://github.com/xperia64/Jusrcheat
https://github.com/Epicpkmn11/NitroHax3DS

thanks also go to the gbatemp community, in particular DeadSkullzJr for
keeping the cheat databases maintained.
