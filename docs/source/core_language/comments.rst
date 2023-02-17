Comments
========

The Pattern Language, just like most other programming languages, supports comments.
A comment is a piece of text that is ignored by the evaluator.
Comments are useful for documenting your code, and for temporarily disabling code.


Single line comments
--------------------

Single line comments start with a double slash (//) and continue to the end of the line.

.. code-block:: hexpat

    // This is a single line comment

Multi line comments
--------------------

Multi line comments start with /* and end with */.

.. code-block:: hexpat

    /* This is
        a multi
        line comment */

Doc comments
------------

Doc comments are used to provide extra documentation for the whole pattern, individual functions or types.

There are multiple ways to write doc comments:

.. code-block:: hexpat

    /*!
        This is a global doc comment.
        It documents the whole pattern and can contain various attributes that can be used by tools to extract information about the pattern.
    */

    /**
        This is a local doc comment.
        It documents the function or type that immediately follows it.
    */

    /**
        This is a doc comment documenting a function that adds two number together
        @param x The first parameter.
        @param y The second parameter.
        @return The sum of the two parameters.
    */
    fn add(u32 x, u32 y) {
        return x + y;
    };

    /// This is a single line local comment. It documents the function or type that immediately follows it.

