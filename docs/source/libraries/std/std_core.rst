``Core Library`` :version:`1.22.0`
==================================

.. code-block:: hexpat

    #include <std/core.pat>

| This namespace contains various core intrinsics to interact directly with the current evaluation runtime
|

------------------------

Types
-----

``BitfieldOrder``
^^^^^^^^^^^^^^^^^

**Bitfield order**

- ``LeftToRight``: Left to right bitfield field ordering
- ``RightToLeft``: Right to left bitfield field ordering

------------------------

Functions
---------

``std::core::has_attribute(ref auto pattern, str attribute) -> bool``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Checks if a given pattern has a given attribute.**

.. table::
    :align: left

    =============== =========================================================
    Parameter       Description
    =============== =========================================================
    ``pattern``     Pattern to check
    ``attribute``   Attribute to look for
    ``return``      Whether or not the pattern has this attribute
    =============== =========================================================

------------------------

``std::core::get_attribute_value(ref auto pattern, str attribute) -> str``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Returns the value of a given attribute that a given pattern has.**

.. table::
    :align: left

    =============== ============================================================================
    Parameter       Description
    =============== ============================================================================
    ``pattern``     Pattern to check
    ``attribute``   Attribute to look for
    ``return``      The value of this attribute or "" if the pattern doesn't have this attribute
    =============== ============================================================================

------------------------

``std::core::set_endian(std::mem::Endian endian)``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Changes the global default endianess to the given value.**
**This function works similar to the `#pragma endian` pragma but can be used multiple times and is scriptable.**

.. table::
    :align: left

    =============== ============================================================================
    Parameter       Description
    =============== ============================================================================
    ``endian``      Value to set the default endian to
    =============== ============================================================================

------------------------

``std::core::get_endian() -> std::mem::Endian``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Returns the global default endianess.**

.. table::
    :align: left

    =============== ============================================================================
    Parameter       Description
    =============== ============================================================================
    ``return``      Current default endian value
    =============== ============================================================================

------------------------

``std::core::set_bitfield_order(std::core::BitfieldOrder order)``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Changes the global default bitfield order to the given value.**
**This function works similar to the `#pragma bitfield_order` pragma but can be used multiple times and is scriptable.**

.. table::
    :align: left

    =============== ============================================================================
    Parameter       Description
    =============== ============================================================================
    ``order``       Value to set the default bitfield order to
    =============== ============================================================================

------------------------

``std::core::get_bitfield_order() -> std::core::BitfieldOrder``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Returns the global default bitfield order.**

.. table::
    :align: left

    =============== ============================================================================
    Parameter       Description
    =============== ============================================================================
    ``return``      Current default bitfield order value
    =============== ============================================================================

------------------------

``std::core::array_index() -> u128``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**When used inside of an array declaration, returns the currently processed array index.**

.. table::
    :align: left

    =============== ============================================================================
    Parameter       Description
    =============== ============================================================================
    ``return``      Currently processed array index
    =============== ============================================================================

------------------------

``std::core::member_count(ref auto pattern) -> u128``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Returns the number of members in a struct, union or bitfield or the number of entries in a array.**

.. table::
    :align: left

    =============== ============================================================================
    Parameter       Description
    =============== ============================================================================
    ``pattern``     Pattern to check entries of
    ``return``      Number of members in the pattern
    =============== ============================================================================

------------------------

``std::core::has_member(ref auto pattern, str name) -> bool``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Returns true if the given pattern has a member with the given name.**

.. table::
    :align: left

    =============== ============================================================================
    Parameter       Description
    =============== ============================================================================
    ``pattern``     Pattern to check entries of
    ``name``        Name of the member to look for
    ``return``      Whether or not this pattern has a member with the given name
    =============== ============================================================================

------------------------

``std::core::formatted_value(ref auto pattern) -> str``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Gets the formatted value of a given pattern.**
**Respects custom formatter functions.**

.. table::
    :align: left

    =============== ============================================================================
    Parameter       Description
    =============== ============================================================================
    ``pattern``     Pattern to get formatted value of
    ``return``      Formatted value
    =============== ============================================================================
