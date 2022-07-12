Variable Placement
==================

In order for the runtime to start decoding data, variables need to be placed
somewhere in the binary data. To do this the variable placement syntax is used:

.. code-block:: hexpat

    u32 myPlacedVariable @ 0x110;

This creates a new unsigned 32 bit variable named ``myPlacedVariable`` and place it at address ``0x100``.

The runtime will now treat the 4 bytes starting at offset ``0x100`` as a u32 and decodes the bytes at this address accordingly.

.. image:: assets/placement/hex.png
  :width: 100%
  :alt: Placement Highlighing

.. image:: assets/placement/data.png
  :width: 100%
  :alt: Placement Decoding

Placing variables isn't limited to just built-in types. All types, even custom ones like structs, enums, unions, etc, can be placed.


Global variables :version:`1.11.0`
----------------------------------

Sometimes it's necessary to store data globally while the pattern is running. For this global variables can be used.
The syntax is the same as with placed variables but are missing the `@` placement instruction at the end.

.. code-block:: hexpat

    u32 globalVariable;


Calculated pointers :version:`1.15.0`
-------------------------------------

The same placement syntax may also be used inside of structs to specify where patterns are supposed to be placed in memory.
These variables do not contribute to the overall size of the struct they are placed within.