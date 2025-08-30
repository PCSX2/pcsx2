# Code Formatting Guidelines

This is a basic code formatting guide that you should take into account. To help with code readability, please follow these basic code?formatting guidelines.

## Topic 1: Do not use variable type prefixes
Variable type prefixes are commonly known as Hungarian Notation, and are the sort of variable names popularized by the Win32API. They look something like this in ideal situations:

```
DWORD dwSomeDoubleWord;
```
Besides being unreadable and looking terrible, this naming convention is totally impractical for modern programming via an IDE with autocomplete features. Coders using such tools want to do autocomplete by the subject of the variable's purpose, and are then automatically provisioned variable type information via tooltip, making the prefix nearly worthless. If you're looking for the MasterVolume of a voice, you might think `VoiceVolumeMaster`, `MasterVolumeVoice`, or `VolumeVoiceMaster` and in fact you'll probably have no idea of it's a dword, word, int, or float value type. Any code using this naming convention will be changed to something more sensible.

Just don't do it. Ever.

## Topic 2: Using PCSX2 base types instead of C/C++ atomic types
Most of the time you want to use either PCSX2 cross-platform types in the place of C++ atomic types. Most of the common types are:

```
u8, s8, u16, s16, u32, s32, u64, s64, u128, s128, uptr, sptr
```
These types ensure consistent operand sizes on all compilers and platforms, and should be used almost always when dealing with integer values. There are exceptions where the plain `int` type may be more ideal, however: typically C++ compilers define an `int` as a value at least 32 bits in length (upper range is compiler/platform specific), and temporary arithmetic operations often conform to this spec and thus can use `int` (a compiler may be allowed then to use whichever data type the target CPU is most efficient at handling). Other C++ integer types such as `short` and `long` should not be used for anything except explicit matching against externally defined 3rd party library functions. They are too unpredictable and have no upside over strict operand sizing.

Note
Using the C++ atomic types `float` and `double` are acceptable; there are no defined alternatives at this time.

Info
PCSX2-specific types `uptr` and `sptr` are meant to be integer-typed containers for pointer addresses, and should be used in situations where pointer arithmetic is needed. `uptr`/`sptr` definitions are not type safe and `void*` or `u8*` should be used for parameter passing instead, when possible.

## Topic 3: Do not use private class members; use protected instead
There is no justifiable reason for any class in PCSX2 to use private variable members. Private members are only useful in the context of dynamically shared core libraries, and only hinder the object extensibility of non-shared application code. Use `protected` for all non-public members instead, as it protects members from unwanted public access while still allowing the object to be extended into a derived class without having to waste a lot of silly effort to dance around private mess.

## Topic 4: Name Protected Class Members with m_
It is highly recommended to prefix protected class members with `m_`; such as `m_SomeVar` or `m_somevar`. This is a widely accepted convention that many IDEs are adding special built in support and handling for, and for that reason it can help improve class organization considerably for all contributors to the project. However, like most other guidelines it is not a definitive requirement.

## Topic 5: Avoid compounding complex operations onto a single line with a function declaration
Example:

```cpp
// Not so good...
void DoSomething() { Function1(); Function2(); var += 1; }

// Good...
void DoSomething()
{
    Function1(); Function2();   var += 1;
}
```
The reason for this guideline is that it can assist debugging tremendously. Most C++ debuggers cannot breakpoint function calls to the bad example, disabling a programmer's ability to use the debugger to quickly track the calling patterns for a function or conditional, and thus removing one of the most ideal tools available to a programmer for understanding code execution patterns of code written by another programmer. For these reasons, code written with such compounding may likely be unrolled onto multiple lines by another programmer at any time, if that programmer is tasked with troubleshooting bugs in that code.

If the operation of a function is simpler (one or two operations max), and if there are many such functions as part of a class or interface, then compounding on a single line for sake of compact readability may be justified; and in such cases, the programmer should also use the `inline_always` attribute on the function to denote that it has a trivial and repetitive operation that can be ignored during debugging.
