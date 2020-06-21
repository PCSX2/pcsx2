|// i386 address | i386 jitted assembler | x64 address | x64 jitted assembler | Comment |
| ------------------------- | ------------------------- | ------------------------- | ------------------------- | ------------------------- |
   0x30000000|  nop|   0x555564983000 <_ZL18memReserve_iR5900A+77074432>|  nop|-
   0x30000001|  mov    0x58b7e82c,%eax|   0x555564983001 <_ZL18memReserve_iR5900A+77074433>|  movabs 0x55555fff9e9c,%eax|-
   0x30000006|  cltd   |   0x55556498300a <_ZL18memReserve_iR5900A+77074442>|  cltd   |-
   0x30000007|  mov    %eax,0x58b7e770|   0x55556498300b <_ZL18memReserve_iR5900A+77074443>|  movabs %eax,0x55555fff9de0|-
   0x3000000c|  mov    %edx,0x58b7e774|   0x555564983014 <_ZL18memReserve_iR5900A+77074452>|  mov    %edx,%eax|-
-|-|   0x555564983016 <_ZL18memReserve_iR5900A+77074454>|  movabs %eax,0x55555fff9de4|-
   0x30000012|  nop|   0x55556498301f <_ZL18memReserve_iR5900A+77074463>|  nop|-
   0x30000013|  nop|   0x555564983020 <_ZL18memReserve_iR5900A+77074464>|  nop|-
-|-|   0x555564983021 <_ZL18memReserve_iR5900A+77074465>|  movabs 0x55555fff9de4,%eax|-
   0x30000014|  cmpl   $0x0,0x58b7e774|   0x55556498302a <_ZL18memReserve_iR5900A+77074474>|  cmp    $0x0,%eax|-
   0x3000001b|  mov    $0x1,%eax|   0x55556498302d <_ZL18memReserve_iR5900A+77074477>|  mov    $0x1,%eax|-
   0x30000020|  jl     0x3000002f|   0x555564983032 <_ZL18memReserve_iR5900A+77074482>|  jl     0x55556498304b <_ZL18memReserve_iR5900A+77074507>|-
   0x30000022|  jg     0x3000002d|   0x555564983034 <_ZL18memReserve_iR5900A+77074484>|  jg     0x555564983049 <_ZL18memReserve_iR5900A+77074505>|-
-|-|   0x555564983036 <_ZL18memReserve_iR5900A+77074486>|  movabs 0x55555fff9de0,%eax|-
   0x30000024|  cmpl   $0x59,0x58b7e770|   0x55556498303f <_ZL18memReserve_iR5900A+77074495>|  cmp    $0x59,%eax|-
-|-|   0x555564983042 <_ZL18memReserve_iR5900A+77074498>|  mov    $0x1,%eax|-
   0x3000002b|  jb     0x3000002f|   0x555564983047 <_ZL18memReserve_iR5900A+77074503>|  jb     0x55556498304b <_ZL18memReserve_iR5900A+77074507>|-
   0x3000002d|  xor    %eax,%eax|   0x555564983049 <_ZL18memReserve_iR5900A+77074505>|  xor    %eax,%eax|-
   0x3000002f|  mov    %eax,0x58b7e5e0|   0x55556498304b <_ZL18memReserve_iR5900A+77074507>|  movabs %eax,0x55555fff9c50|-
-|-|   0x555564983054 <_ZL18memReserve_iR5900A+77074516>|  xor    %eax,%eax|-
   0x30000034|  movl   $0x0,0x58b7e5e4|   0x555564983056 <_ZL18memReserve_iR5900A+77074518>|  movabs %eax,0x55555fff9c54|-
   0x3000003e|  nop|   0x55556498305f <_ZL18memReserve_iR5900A+77074527>|  nop|-
-|-|   0x555564983060 <_ZL18memReserve_iR5900A+77074528>|  movabs 0x55555fff9120,%eax|-
-|-|   0x555564983069 <_ZL18memReserve_iR5900A+77074537>|  mov    %eax,%ebx|-
-|-|   0x55556498306b <_ZL18memReserve_iR5900A+77074539>|  movabs 0x55555fff9c50,%eax|-
   0x3000003f|  cmpl   $0x0,0x58b7e5e0|   0x555564983074 <_ZL18memReserve_iR5900A+77074548>|  cmp    %ebx,%eax|xCMP(ptr32[&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ]], g_cpuConstRegs[_Rt_].UL[0] ); //iR5900Branch.cpp
   0x30000046|  jne    0x30000055|   0x555564983076 <_ZL18memReserve_iR5900A+77074550>|  jne    0x555564983094 <_ZL18memReserve_iR5900A+77074580>|-
-|-|   0x555564983078 <_ZL18memReserve_iR5900A+77074552>|  movabs 0x55555fff9124,%eax|-
-|-|   0x555564983081 <_ZL18memReserve_iR5900A+77074561>|  mov    %eax,%ebx|-
-|-|   0x555564983083 <_ZL18memReserve_iR5900A+77074563>|  movabs 0x55555fff9c54,%eax|-
   0x30000048|  cmpl   $0x0,0x58b7e5e4|   0x55556498308c <_ZL18memReserve_iR5900A+77074572>|  cmp    %ebx,%eax|-
   0x3000004f|  je     0x3000007e|   0x55556498308e <_ZL18memReserve_iR5900A+77074574>|  je     0x5555649830d2 <_ZL18memReserve_iR5900A+77074642>|-
   0x30000055|  nop|   0x555564983094 <_ZL18memReserve_iR5900A+77074580>|  nop|-
   0x30000056|  movl   $0xbfc00024,0x58b7e878|   0x555564983095 <_ZL18memReserve_iR5900A+77074581>|  mov    $0xbfc00024,%eax|-
   0x30000060|  mov    0x58b7e990,%eax|   0x55556498309a <_ZL18memReserve_iR5900A+77074586>|  movabs %eax,0x55555fff9ee8|-
-|-|   0x5555649830a3 <_ZL18memReserve_iR5900A+77074595>|  movabs 0x55555fffa000,%eax|-
   0x30000065|  add    $0xb,%eax|   0x5555649830ac <_ZL18memReserve_iR5900A+77074604>|  add    $0xb,%eax|-
   0x30000068|  mov    %eax,0x58b7e990|   0x5555649830af <_ZL18memReserve_iR5900A+77074607>|  movabs %eax,0x55555fffa000|-
-|-|   0x5555649830b8 <_ZL18memReserve_iR5900A+77074616>|  mov    %eax,%ebx|-
-|-|   0x5555649830ba <_ZL18memReserve_iR5900A+77074618>|  movabs 0x555557e03ac0,%eax|-
   0x3000006d|  sub    0x58b7db40,%eax|   0x5555649830c3 <_ZL18memReserve_iR5900A+77074627>|  sub    %eax,%ebx|-
-|-|   0x5555649830c5 <_ZL18memReserve_iR5900A+77074629>|  mov    %ebx,%eax|-
   0x30000073|  js     0x58d79019 <_ZL16eeRecDispatchers+25>|   0x5555649830c7 <_ZL18memReserve_iR5900A+77074631>|  js     0x55556498203b <_ZL18memReserve_iR5900A+77070395>|-
   0x30000079|  jmp    0x58d79000 <_ZL16eeRecDispatchers>|   0x5555649830cd <_ZL18memReserve_iR5900A+77074637>|  jmpq   0x555564982000 <_ZL18memReserve_iR5900A+77070336>|-
   0x3000007e|  nop|   0x5555649830d2 <_ZL18memReserve_iR5900A+77074642>|  nop|-
   0x3000007f|  movl   $0xbfc00014,0x58b7e878|   0x5555649830d3 <_ZL18memReserve_iR5900A+77074643>|  mov    $0xbfc00014,%eax|-
   0x30000089|  mov    0x58b7e990,%eax|   0x5555649830d8 <_ZL18memReserve_iR5900A+77074648>|  movabs %eax,0x55555fff9ee8|-
-|-|   0x5555649830e1 <_ZL18memReserve_iR5900A+77074657>|  movabs 0x55555fffa000,%eax|-
   0x3000008e|  add    $0xb,%eax|   0x5555649830ea <_ZL18memReserve_iR5900A+77074666>|  add    $0xb,%eax|-
   0x30000091|  mov    %eax,0x58b7e990|   0x5555649830ed <_ZL18memReserve_iR5900A+77074669>|  movabs %eax,0x55555fffa000|-
-|-|   0x5555649830f6 <_ZL18memReserve_iR5900A+77074678>|  mov    %eax,%ebx|-
-|-|   0x5555649830f8 <_ZL18memReserve_iR5900A+77074680>|  movabs 0x555557e03ac0,%eax|-
   0x30000096|  sub    0x58b7db40,%eax|   0x555564983101 <_ZL18memReserve_iR5900A+77074689>|  sub    %eax,%ebx|-
-|-|   0x555564983103 <_ZL18memReserve_iR5900A+77074691>|  mov    %ebx,%eax|-
   0x3000009c|  js     0x58d79019 <_ZL16eeRecDispatchers+25>|   0x555564983105 <_ZL18memReserve_iR5900A+77074693>|  js     0x55556498203b <_ZL18memReserve_iR5900A+77070395>|-
   0x300000a2|  jmp    0x58d79000 <_ZL16eeRecDispatchers>|   0x55556498310b <_ZL18memReserve_iR5900A+77074699>|  jmpq   0x555564982000 <_ZL18memReserve_iR5900A+77070336>|-

