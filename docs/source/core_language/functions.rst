Functions
=========

Functions are reusable pieces of code that can do calculations. Pretty much like functions in any other programming language.

Parameter types need to be specified explicitly, return type is automatically deduced.

.. code-block:: hexpat

    fn min(s32 a, s32 b) {
        if (a > b)
            return b;
        else
            return a;
    };

    std::print(min(100, 200)); // 100

Global Scope
^^^^^^^^^^^^

The global scope for the most part, works the same as a function. All statements in the global scope will get executed when the program is executed.
The only difference is that new types can be defined in the global scope.

Parameter packs :version:`1.14.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To allow passing an unlimited number of parameters to functions, parameter packs can be used.

.. code-block:: hexpat

    fn print_sequence(auto first, auto ... rest) {
        std::print("{}", first);

        if (std::sizeof_pack(rest) > 0)
            print_sequence(rest);
    };

    print_sequence(1, 2, 3, 4, 5, 6);

Parameter packs can exclusively be used as arguments to other functions. Using them automatically expands them acting as if the contained values
get passed to the other function individually.

The above function will print out all passed values in sequence by printing the first parameter and then removing it by not passing it on to the function during
the next run.

Default parameters :version:`1.17.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Default parameters can be used to set a default value for parameters, if they weren provided when the functon got called.

.. code-block:: hexpat

    fn print_numbers(u32 a, u32 b, u32 c = 3, u32 d = 4) {
        std::print("{} {} {} {}", a, b, c, d);
    };

    print_numbers(1, 2);            // Prints 1 2 3 4
    print_numbers(8, 9, 10, 11);    // Prints 8 9 10 11

Reference parameters :version:`1.23.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Reference parameters can be used to avoid unnecessarily copying lots of data around and are helpful when needing to pass custom types that do not have a fixed layout to a function.
Instead of copying the given parameter value onto the function's heap, a reference to the existing pattern is instead used.

.. code-block:: hexpat

    struct MyString {
        char value[while(std::mem::read_unsigned($, 1) != 0xFF)];
    };

    fn print_my_string(ref MyString myString) {
        std::print(myString.value);
    };

Variables
^^^^^^^^^

Variables can be declared in a similar fashion as outside of functions but they will be put onto the function's stack instead of being highlighted.

.. code-block:: hexpat

    fn get_value() {
        u32 value;
        u8 x = 1234;

        value = x * 2;

        return value;
    };

Custom types may also be used inside of functions :version:`1.19.0`

.. code-block:: hexpat

    union FloatConverter {
        u32 integer;
        float floatingPoint;
    };

    fn interpret_as_float(u32 integer) {
        FloatConverter converter;

        converter.integer = integer;
        return converter.floatingPoint;
    };

It is also possible to declare constants using the ``const`` keyword :version:`1.27.0`

.. code-block:: hexpat

    fn get_value() {
        const u32 value = 1234;
        return value;
    };

    std::print("{}", get_value()); // 1234

Control statements
^^^^^^^^^^^^^^^^^^

If-Else-Statements
------------------

If, Else-If and Else statements work the same as in most other C-like languages.
When the condition inside a ``if`` head evaluates to true, the code in its body is executed.
If it evaluates to false, the optional ``else`` block is executed.

Curly braces are optional and only required if more than one statement is present in the body.

.. code-block:: hexpat

    if (x > 5) {
        // Execute when x is greater than 5
    } else if (x == 2) {
        // Execute only when x is equals to 2
    } else {
        // Execute otherwise
    }


While-Loops
-----------

While loops work similarly to if statements. As long as the condition in the head evaluates to true, the body will continuously be executed.

.. code-block:: hexpat

    while (check()) {
        // Keeps on executing as long as the check() function returns true
    }


For-Statement :version:`1.12.0`
--------------------------------

For loops are another kind of loop similar to the while loop. Its head consists of three blocks separated by commas.
The first block is a variable declaration which will only be available inside the current for loop.
The second block is a condition that will continuously be checked. The body is executed as long as this condition evaluates to true.
The third block is a variable assignment which will be executed after all statements in the body have run.

.. code-block:: hexpat

    // Declare a variable called i available only inside the for
    for (u8 i = 0, i < 10, i = i + 1) {
        // Keeps on executing as long as i is less than 10

        // At the end, increment i by 1
    }

Loop control flow statements :version:`1.13.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Inside of loops, the ``break`` and ``continue`` keyword may be used to to control the execution flow inside the loop.

When a ``break`` statement is reached, the loop is terminated immediately and code flow continues after the loop.
When a ``continue`` statement is reached, the current iteration is terminated immediately and code flow continues at the start of the loop again, checking the condition again.

Return statements
^^^^^^^^^^^^^^^^^

In order to return a value from a function, the ``return`` keyword is used.

The return type of the function will automatically be determined by the value returned.

.. code-block:: hexpat

    fn get_value() {
        return 1234;
    };

    std::print("{}", get_value()); // 1234

Pattern views :version:`1.26.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Pattern views are a way to access bytes as a certain type without actually placing a pattern there that will end up in the output.

.. code-block:: hexpat

    fn read_u32(u32 address) {
        u32 value @ address;

        return value;
    };

    std::print("{}", read_u32(0x1234)); // Prints the value at address 0x1234 formatted as a u32