|// i386 address | i386 jitted assembler | x64 address | x64 jitted assembler | Comment |
| ------------------------- | ------------------------- | ------------------------- | ------------------------- | ------------------------- |
   0x30000000|  nop|   0x210000000|  nop|-
   0x30000001|  mov    0x58b7e82c,%eax|   0x210000001|  movabs 0x555557e04b0c,%eax|-
   0x30000006|  cltd   |   0x21000000a|  cltd   |-
   0x30000007|  mov    %eax,0x58b7e770|   0x21000000b|  movabs %eax,0x555557e04a50|-
   0x3000000c|  mov    %edx,0x58b7e774|   0x210000014|  mov    %edx,%eax|-
-|-|   0x210000016|  movabs %eax,0x555557e04a54|-
   0x30000012|  nop|   0x21000001f|  nop|-
   0x30000013|  nop|   0x210000020|  nop|-
-|-|   0x210000021|  movabs 0x555557e04a54,%eax|-
   0x30000014|  cmpl   $0x0,0x58b7e774|   0x21000002a|  cmp    $0x0,%eax|-
   0x3000001b|  mov    $0x1,%eax|   0x21000002d|  mov    $0x1,%eax|-
   0x30000020|  jl     0x3000002f|   0x210000032|  jl     0x21000004b|-
   0x30000022|  jg     0x3000002d|   0x210000034|  jg     0x210000049|-
-|-|   0x210000036|  movabs 0x555557e04a50,%eax|-
   0x30000024|  cmpl   $0x59,0x58b7e770|   0x21000003f|  cmp    $0x59,%eax|-
-|-|   0x210000042|  mov    $0x1,%eax|-
   0x3000002b|  jb     0x3000002f|   0x210000047|  jb     0x21000004b|-
   0x3000002d|  xor    %eax,%eax|   0x210000049|  xor    %eax,%eax|-
   0x3000002f|  mov    %eax,0x58b7e5e0|   0x21000004b|  movabs %eax,0x555557e048c0|-
-|-|   0x210000054|  xor    %eax,%eax|-
   0x30000034|  movl   $0x0,0x58b7e5e4|   0x210000056|  movabs %eax,0x555557e048c4|-
   0x3000003e|  nop|   0x21000005f|  nop|-
-|-|   0x210000060|  movabs 0x5555580fb120,%eax|-
-|-|   0x210000069|  mov    %eax,%ebx|-
-|-|   0x21000006b|  movabs 0x555557e048c0,%eax|-
   0x3000003f|  cmpl   $0x0,0x58b7e5e0|   0x210000074|  cmp    %ebx,%eax|xCMP(ptr32[&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ]], g_cpuConstRegs[_Rt_].UL[0] ); //iR5900Branch.cpp
   0x30000046|  jne    0x30000055|   0x210000076|  jne    0x210000094|-
-|-|   0x210000078|  movabs 0x5555580fb124,%eax|-
-|-|   0x210000081|  mov    %eax,%ebx|-
-|-|   0x210000083|  movabs 0x555557e048c4,%eax|-
   0x30000048|  cmpl   $0x0,0x58b7e5e4|   0x21000008c|  cmp    %ebx,%eax|-
   0x3000004f|  je     0x3000007e|   0x21000008e|  je     0x2100000e3|-
   0x30000055|  nop|   0x210000094|  nop|-
   0x30000056|  movl   $0xbfc00024,0x58b7e878|   0x210000095|  mov    $0xbfc00024,%eax|-
   0x30000060|  mov    0x58b7e990,%eax|   0x21000009a|  movabs %eax,0x555557e04b58|-
-|-|   0x2100000a3|  movabs 0x555557e04c70,%eax|-
   0x30000065|  add    $0xb,%eax|   0x2100000ac|  add    $0xb,%eax|-
   0x30000068|  mov    %eax,0x58b7e990|   0x2100000af|  movabs %eax,0x555557e04c70|-
-|-|   0x2100000b8|  mov    %eax,%ebx|-
-|-|   0x2100000ba|  movabs 0x555557e03de0,%eax|-
   0x3000006d|  sub    0x58b7db40,%eax|   0x2100000c3|  sub    %eax,%ebx|-
-|-|   0x2100000c5|  mov    %ebx,%eax|-
-|-|   0x2100000c7|  jns    0x2100000d6|-
-|-|   0x2100000c9|  movabs $0x55555810203b,%rax|-
   0x30000073|  js     0x58d79019 <_ZL16eeRecDispatchers+25>|   0x2100000d3|  rex.W jmpq *%rax|-
-|-|   0x2100000d6|  movabs $0x555558102000,%rax|-
   0x30000079|  jmp    0x58d79000 <_ZL16eeRecDispatchers>|   0x2100000e0|  rex.W jmpq *%rax|-
   0x3000007e|  nop|   0x2100000e3|  nop|-
   0x3000007f|  movl   $0xbfc00014,0x58b7e878|   0x2100000e4|  mov    $0xbfc00014,%eax|-
   0x30000089|  mov    0x58b7e990,%eax|   0x2100000e9|  movabs %eax,0x555557e04b58|-
-|-|   0x2100000f2|  movabs 0x555557e04c70,%eax|-
   0x3000008e|  add    $0xb,%eax|   0x2100000fb|  add    $0xb,%eax|-
   0x30000091|  mov    %eax,0x58b7e990|   0x2100000fe|  movabs %eax,0x555557e04c70|-
-|-|   0x210000107|  mov    %eax,%ebx|-
-|-|   0x210000109|  movabs 0x555557e03de0,%eax|-
   0x30000096|  sub    0x58b7db40,%eax|   0x210000112|  sub    %eax,%ebx|-
-|-|   0x210000114|  mov    %ebx,%eax|-
-|-|   0x210000116|  jns    0x210000125|-
-|-|   0x210000118|  movabs $0x55555810203b,%rax|-
   0x3000009c|  js     0x58d79019 <_ZL16eeRecDispatchers+25>|   0x210000122|  rex.W jmpq *%rax|-
-|-|   0x210000125|  movabs $0x555558102000,%rax|-
   0x300000a2|  jmp    0x58d79000 <_ZL16eeRecDispatchers>|   0x21000012f|  rex.W jmpq *%rax|-
