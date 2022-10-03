``Decoding Library`` :version:`1.24.0`
======================================

.. code-block:: hexpat

    #include <hex/dec.pat>

| This namespace contains various special decoder functions.
|

------------------------

Functions
---------

``hex::dec::demangle(str mangled_name) -> str``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Demangles a Itanium, MSVC, Rust or DLang mangled name.**

.. table::
    :align: left

    ================ ==============================================================
    Parameter        Description
    ================ ==============================================================
    ``mangled_name`` Mangled name
    ``return``       Demangled name
    ================ ==============================================================