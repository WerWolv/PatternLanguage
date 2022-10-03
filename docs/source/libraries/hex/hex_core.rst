``Core Library`` :version:`1.24.0`
==================================

.. code-block:: hexpat

    #include <hex/core.pat>

| This namespace contains various core intrinsics to interact directly with the ImHex Hex Editor
|

------------------------

Types
-----

``Selection``
^^^^^^^^^^^^^

**Current hex editor selection**

- ``bool valid``: If the selection value is valid or not
- ``u64 address``: Start address of the selection
- ``u64 size``: Size of the selection

------------------------

Functions
---------

``hex::core::get_selection() -> hex::core::Selection``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Returns the region that is currently selected in the hex editor view.**

.. table::
    :align: left

    =============== ==============================================================
    Parameter       Description
    =============== ==============================================================
    ``return``      ``Selection`` struct. Only use data if ``valid`` flag is true!
    =============== ==============================================================