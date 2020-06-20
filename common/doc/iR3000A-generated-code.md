|// i386 address | i386 jitted assembler | x64 address | x64 jitted assembler | Comment |
| ------------------------- | ------------------------- | ------------------------- | ------------------------- | ------------------------- |
   0x34000000|  mov    0x58b61824,%eax|   0x214000000|  movabs 0x555557e03ac4,%eax|-
   0x34000005|  mov    %eax,0x58b617c8|   0x214000009|  movabs %eax,0x555557e03a68|-
   0x3400000a|  xor    %eax,%eax|   0x214000012|  xor    %eax,%eax|-
-|-|   0x214000014|  movabs 0x555557e03a68,%eax|-
   0x3400000c|  cmpl   $0x59,0x58b617c8|   0x21400001d|  cmp    $0x59,%eax|-
   0x34000013|  setl   %al|   0x214000020|  setl   %al|-
   0x34000016|  mov    %eax,0x58b61764|   0x214000023|  movabs %eax,0x555557e03a04|-
-|-|   0x21400002c|  movabs 0x555557e03a04,%eax|-
-|-|   0x214000035|  mov    %eax,%ebx|-
-|-|   0x214000037|  movabs 0x555557e03d40,%eax|-
   0x3400001b|  cmpl   $0x0,0x58b61764|   0x214000040|  cmp    %ebx,%eax|-
   0x34000022|  jne    0x34000070|   0x214000042|  jne    0x2140000d0|-
   0x34000028|  movl   $0xbfc00014,0x58b61968|   0x214000048|  mov    $0xbfc00014,%eax|-
   0x34000032|  mov    0x58b61970,%eax|   0x21400004d|  movabs %eax,0x555557e03c08|-
-|-|   0x214000056|  movabs 0x555557e03c10,%eax|-
   0x34000037|  add    $0x5,%eax|   0x21400005f|  add    $0x5,%eax|-
   0x3400003a|  mov    %eax,0x58b61970|   0x214000062|  movabs %eax,0x555557e03c10|-
-|-|   0x21400006b|  mov    $0x28,%ebx|-
   0x3400003f|  subl   $0x28,0x56b222a4|   0x214000070|  sub    %ebx,%eax|-
-|-|   0x214000072|  movabs %eax,0x555555dc0478|-
-|-|   0x21400007b|  jg     0x21400008a|-
-|-|   0x21400007d|  movabs $0x5553440f9003,%rax|-
   0x34000046|  jle    0x58d54045 <_ZL17iopRecDispatchers+69>|   0x214000087|  rex.W jmpq *%rax|-
-|-|   0x21400008a|  mov    %eax,%ebx|-
-|-|   0x21400008c|  movabs 0x555557e03d20,%eax|-
   0x3400004c|  sub    0x58b61a80,%eax|   0x214000095|  sub    %eax,%ebx|-
   0x34000052|  js     0x34000069|   0x214000097|  js     0x2140000bd|-
   0x34000054|  call   0x56629550 <iopEventTest()>|   0x214000099|  callq  0x2556afa90|-
-|-|   0x21400009e|  movabs 0x555557e03c08,%eax|-
-|-|   0x2140000a7|  mov    $0xbfc00014,%ebx|-
   0x34000059|  cmpl   $0xbfc00014,0x58b61968|   0x2140000ac|  cmp    %ebx,%eax|-
-|-|   0x2140000ae|  je     0x2140000bd|-
-|-|   0x2140000b0|  movabs $0x5553440f8f48,%rax|-
   0x34000063|  jne    0x58d54005 <_ZL17iopRecDispatchers+5>|   0x2140000ba|  rex.W jmpq *%rax|-
-|-|   0x2140000bd|  movabs $0x5555580f903b,%rax|-
   0x34000069|  jmp    0x58d54019 <_ZL17iopRecDispatchers+25>|   0x2140000c7|  rex.W jmpq *%rax|-
