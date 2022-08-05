``Time Library`` :version:`1.20.0`
==================================

.. code-block:: hexpat

    #include <std/time.pat>

| This namespace contains time utility functions
|

------------------------

Types
-----

``std::time::Time``
^^^^^^^^^^^^^^^^^^^

**A type representing a time point in a specific timezone.**

------------------------

``std::time::EpochTime``
^^^^^^^^^^^^^^^^^^^^^^^^

**A type representing a time point in seconds since epoch time.**

------------------------

``std::time::TimeZone``
^^^^^^^^^^^^^^^^^^^^^^^

**An enum containing available time zones to convert to.**

- Local
- UTC

------------------------

Functions
---------

``std::time::epoch() -> EpochTime``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Returns the current system time in seconds since epoch.**

.. table::
    :align: left

    =========== =========================================================
    Parameter   Description
    =========== =========================================================
    ``return``  Seconds since epoch
    =========== =========================================================

------------------------

``std::time::to_local(EpochTime epoch_time) -> Time``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Converts a given epoch time to the local timezone time.**

.. table::
    :align: left

    =============== =========================================================
    Parameter       Description
    =============== =========================================================
    ``epoch_time``  Epoch time
    ``return``      Time converted to local time
    =============== =========================================================

------------------------

``std::time::to_utc(EpochTime epoch_time) -> Time``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Converts a given epoch time to the UTC timezone time.**

.. table::
    :align: left

    =============== =========================================================
    Parameter       Description
    =============== =========================================================
    ``epoch_time``  Epoch time
    ``return``      Time converted to UTC time
    =============== =========================================================

------------------------

``std::time::now(TimeZone time_zone = TimeZone::Local) -> Time``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Returns the current time in a given timezone.**

.. table::
    :align: left

    =============== =========================================================
    Parameter       Description
    =============== =========================================================
    ``time_zone``   Entry from the ``TimeZone`` enum
    ``return``      Current time in the given timezone
    =============== =========================================================

------------------------

``std::time::format(Time time, str format_string = "%c") -> str``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Formats the given time to a specified format. System format by default.**

.. table::
    :align: left

    ================== =========================================================
    Parameter          Description
    ================== =========================================================
    ``time``           Time to format
    ``format_string``  Format string using standard C format specifiers
    ``return``         Formatted time string
    ================== =========================================================