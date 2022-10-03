``Networking Library`` :version:`1.24.0`
========================================

.. code-block:: hexpat

    #include <hex/http.pat>

| This namespace contains various networking functions.
|

.. warning::

    These functions are considered dangerous and require the user to confirm that they really want to run this pattern.

------------------------

Functions
---------

``hex::http::get(str url) -> str``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Issues a ``GET`` request to the passed URL and returns the received data as a string**

.. table::
    :align: left

    =========== =========================================================
    Parameter   Description
    =========== =========================================================
    ``url``     URL to make request to
    ``return``  Response body
    =========== =========================================================