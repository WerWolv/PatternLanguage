Sections :version:`1.25.0`
==========================

Sections are a way to create additional buffers of data whose content can be generated dynamically.

The following code creates a new section named "My Section" and then creates a buffer of 256 bytes in that section.
The buffer is then filled with data.

At the end, it is possible to access to place additional patterns inside the section to decode the data in it.

.. code-block:: hexpat

    #include <std/mem.pat>

    std::mem::Section mySection = std::mem::create_section("My Section");

    u8 sectionData[0x100] @ 0x00 in mySection;

    sectionData[0] = 0xAA;
    sectionData[1] = 0xBB;
    sectionData[2] = 0xCC;
    sectionData[3] = 0xDD;

    sectionData[0xFF] = 0x00;

    u32 value @ 0x00 in mySection;

This is mainly useful for analyzing data that needs to be generated at runtime such as compressed, encrypted or otherwise transformed data.