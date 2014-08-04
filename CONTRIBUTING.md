# CONTRIBUTING

## Overview

This is the contributor's guide for the PCSX2 emulator. In order to expedite any code contribution, these guidelines should be thoroughly read and followed.

## Style

### Alignment

- For left-alignment (indentation), use tabs.
- For alignment within a line, use spaces.
- Always align separate lines of code if it would make said code more readable. An example:

  ```cpp
  int m_FirstField    = 0;
  int m_SecondField   = 0;
  int m_EleventhField = 0;
  ```

### Naming

#### Case

- CamelCase for variable fields, functions, methods, and classnames.
- ALL_CAPS for constants and defines.

#### Prefixes

- Do not use hungarian notation (or any other notation for that matter).
  - An exeption to this is prefixes for global or member variables.
    - Global: `g_SomeField`
    - Member: `m_SomeField`
- All variables that are not global and not members should have no prefixes whatsoever.

### Braces/Brackets

- All curly braces (`{ }`) should be placed on newlines. The only exception to this is **short** inline functions in class header files (getters, setters).
- Newlines are **mandatory** after conditions. This should never occur in your code:

  ```cpp
  if (SomeCondition) DoSomething();
  ```
  - This is because it is impossible to place a breakpoint on `DoSomething()`, only on the entire line of code. Keeping things nicely separated makes debugging much easier:
  
  ```cpp
  if (SomeCondition)
      DoSomething();
  ```

### Whitespace

- Files always need **one** empty line at the end.
- Always add two blank lines between different functions in a cpp file.

## Documentation

- All functions and variables that do not override another should be documented. At the very least, write a couple words about **why it's there**.
- The difference between an evil or a good magic constant is that the good ones are well-documented. All constants in your code should have their meaning instantly apparent to a reader.
- Documentation doesn't exist to explain how something works (your code should be obvious enough for that). Documentation exists to explain why that code is there.
- Use doxygen comments for all kinds of documentation of functions and members. Note that **comments within blocks of code do not need use doxygen**. An example:

  ```cpp
  /** The explanation of what and why this function does.
  @param Param1 Why Param1 needs to be passed.
  @param Param2 Why Param2 needs to be passed. */
  int DoSomething(int Param1, int Param2);
  ```
