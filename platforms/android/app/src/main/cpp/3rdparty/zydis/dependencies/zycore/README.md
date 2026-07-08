# Zyan Core Library for C

<a href="./LICENSE"><img src="https://img.shields.io/badge/License-MIT-blue.svg" alt="License: MIT"></a>
<a href="https://github.com/zyantific/zycore-c/actions"><img src="https://github.com/zyantific/zycore-c/workflows/GitHub%20Actions%20CI/badge.svg" alt="GitHub Actions"></a>
<a href="https://discord.zyantific.com/"><img src="https://img.shields.io/discord/390136917779415060.svg?logo=discord&label=Discord" alt="Discord"></a>

Internal library providing platform independent types, macros and a fallback for environments without LibC.

## Features

- Platform independent types
  - Integer types (`ZyanU8`, `ZyanI32`, `ZyanUSize`, ...)
  - `ZyanBool` (+ `ZYAN_FALSE`, `ZYAN_TRUE`)
  - `ZYAN_NULL`
- Macros
  - Compiler/Platform/Architecture detection
  - Asserts and static asserts
  - Utils (`ARRAY_LENGTH`, `FALLTHROUGH`, `UNUSED`, ...)
- Common types
  - `ZyanBitset`
  - `ZyanString`/`ZyanStringView`
- Container types
  - `ZyanVector`
  - `ZyanList`
- LibC abstraction (WiP)

## License

Zycore is licensed under the MIT license.
