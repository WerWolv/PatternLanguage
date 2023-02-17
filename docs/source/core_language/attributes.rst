Attributes
==========

Attributes are special directives that can add extra configuration individual variables and types.

.. code-block:: hexpat

  struct RGBA8 {
    u8 red   [[color("FF0000")]];
    u8 green [[color("00FF00")]];
    u8 blue  [[color("0000FF")]];
  } [[static, color(std::format("{:02X}{:02X}{:02X}", red, green, blue))]];

.. note::

  | Attributes can either have no arguments: ``[[attribute_name]]``
  | Or one or more arguments: ``[[attribute_name("arg1", 1234)]]``

It's also possible to apply multiple attributes to the same variable or type: ``[[attribute1, attribute2(1234)]]``

------------------------

Variable Attributes
^^^^^^^^^^^^^^^^^^^

.. note::

  :version:`1.16.0`
  Variable attributes may also be applied to types directly to cause all patterns
  created from them to have those attributes applied.

  To access members of the variable the attribute is applied to, use the ``this`` keyword.
  Some attributes expect an optional pattern as argument. If this is not necessary, you can use the ``null`` keyword instead.

``[[color("RRGGBB")]]``
-----------------------

Changes the color the variable is being highlighted in the hex editor.

``[[name("new_name")]]``
------------------------

Changes the display name of the variable in the pattern data view without affecting the rest of the program.

``[[comment("text")]]``
-----------------------

Adds a comment to the current variable that is displayed when hovering over it in the pattern data view.

``[[format("formatter_function_name")]]`` / ``[[format_read("formatter_function_name")]]``
------------------------------------------------------------------------------------------

Overrides the default display value formatter with a custom function. 
The function requires a single argument representing the value to be formatted (e.g ``u32`` if this attribute was applied to a variable of type ``u32``) and return a string which will be displayed in place of the default value.

It's also possible to return a pattern or value from this function which will then be default formatted using the default pattern language rules. :version:`1.22.0`

``[[format_write("formatter_function_name")]]`` :version:`1.26.0`
-----------------------------------------------------------------

Allows to specify a custom write formatter function for a type.
The function takes in a string of the input provided by the user and returns any pattern that will be turned into bytes and gets written to the address of that pattern.

``[[format_entries("formatter_function_name")]]`` / ``[[format_read_entries("formatter_function_name")]]`` :version:`1.15.0`
----------------------------------------------------------------------------------------------------------------------------

Can be applied to arrays and works the same as the ``[[format_read]]`` attribute but overrides the default display value formatter of all array entries
instead of just the array pattern itself.

``[[format_write_entries("formatter_function_name")]]`` :version:`1.27.0`
-------------------------------------------------------------------------

Can be applied to arrays and works the same as the ``[[format_write]]`` attribute but overrides the default value write formatter of all array entries
instead of just the array pattern itself.

``[[hidden]]``
--------------

Prevents a variable from being shown in the pattern data view but still be usable from the rest of the program.

``[[inline]]`` :version:`1.10.1`
---------------------------------

Can only be applied to Arrays and Struct-like types. Visually inlines all members of this variable into the parent scope. 
Useful to flatten the displayed tree structure and avoid unnecessary indentation while keeping the pattern structured. 

``[[transform("transformer_function_name")]]`` :version:`1.10.1`
----------------------------------------------------------------

Specifies a function that will be executed to preprocess the value read from that variable before it's being accessed through the dot syntax (``some_struct.some_value``).
The function requires a single argument representing the original value that was read (e.g ``u32`` if this attribute was applied to a variable of type ``u32``) and return a value that will be returned instead.

`[[transform_entries("formatter_function_name")]]`` :version:`1.27.0`
---------------------------------------------------------------------

Can be applied to arrays and works the same as the ``[[transform]]`` attribute but overrides the transform function of all array entries
instead of just the array pattern itself.

``[[pointer_base("pointer_base_function_name")]]`` :version:`1.10.1`
--------------------------------------------------------------------

Specifies a function that will be executed to preprocess the address of the pointer this attribute was applied to points to.
The function requires a single argument representing the original pointer address that was read (e.g ``u32`` if this attribute was applied to a pointer with size type ``u32``) and return the offset the pointer should point to instead.

There's a number of :ref:`predefined pointer helper functions <Pointer Helpers>` available in the standard library to rebase pointers off of different places.

``[[no_unique_address]]`` :version:`1.14.0`
-------------------------------------------

A variable marked by this attribute will be placed in memory but not increment the current cursor position. 

``[[single_color]]`` :version:`1.16.0`
--------------------------------------

Forces all members of the struct, union or array to be highlighted using the same color instead of individual ones

Type Attributes
^^^^^^^^^^^^^^^

``[[static]]``
--------------

| The Pattern Language by default optimizes arrays of built-in types so they don't use up as much memory and are evaluated quicker.
| This same optimization can be applied to custom data types when they are marked with this attribute to tell the runtime the size and layout of this type will always be the same.
| **However** if the layout of the type this is applied to is not static, highlighing and decoding of the data will be wrong and only use the layout of the first array entry.


``[[left_to_right]]`` / ``[[right_to_left]]`` :version:`1.15.0`
---------------------------------------------------------------

These attributes can be applied to bitfields to set if bits should be indexed from left to right or from right to left

``[[sealed]]`` :version:`1.20.0`
--------------------------------

This attribute can be applied to structs, unions and bitfields.
It causes tools that display Patterns in some way to not display the implementation details (such as children of this type) anymore but
instead treat like a built-in type. This is mainly useful for making custom types that should decode and display the bytes in a custom
format using the ``[[format]]`` attribute.

``[[highlight_hidden]]`` :version:`1.27.0`
------------------------------------------

Works the same as the ``[[hidden]]`` attribute but only hides the highlighting of the variable and not the variable in the pattern data view.