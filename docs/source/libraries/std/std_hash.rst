``Hashing Library``
===================

.. code-block:: hexpat

    #include <std/hash.pat>

| This namespace contains networking functions
|

------------------------

Functions
---------

``std::hash::crc32(u128 address, u64 size, u32 init, u32 poly) -> u32``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Calculates the CRC32 hash of the given memory region.**

.. table::
    :align: left

    =========== =========================================================
    Parameter   Description
    =========== =========================================================
    ``address`` Start address
    ``size``    Region size
    ``init``    Initial CRC32 value
    ``poly``    CRC32 polynomial
    ``return``  Calculated CRC32 checksum
    =========== =========================================================