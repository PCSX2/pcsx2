|// i386 address | i386 jitted assembler | x64 address | x64 jitted assembler | Comment |
| -------------- | -------------- | -------------- | -------------- | ------------------------- |
   0x631ac27b <m+38601339>|  nop|0x55556499f34c <m+77202252>|  nop|-
   0x631ac27c <m+38601340>|  addl   $0xffffffa0,0x60cd5270|   0x55556499f34d <m+77202253>|  movabs 0x55555fff7670,%eax|-
   0x631ac283 <m+38601347>|  lahf   |   0x55556499f356 <m+77202262>|  add    $0xffffffffffffffa0,%rax|-
   0x631ac284 <m+38601348>|  sar    $0xf,%ax|   0x55556499f35a <m+77202266>|  movabs %rax,0x55555fff7670|-
   0x631ac288 <m+38601352>|  cwtl   |-|-|-
   0x631ac289 <m+38601353>|  mov    %eax,0x60cd5274|-|-|-
   0x631ac28e <m+38601358>|  nop|   0x55556499f364 <m+77202276>|  nop|-
   0x631ac28f <m+38601359>|  nop|   0x55556499f365 <m+77202277>|  nop|-
   0x631ac290 <m+38601360>|  mov    $0x60cd5290,%edx|   0x55556499f366 <m+77202278>|  movabs $0x55555fff7690,%rdx|-
   0x631ac295 <m+38601365>|  mov    0x60cd5270,%ecx|   0x55556499f370 <m+77202288>|  mov    -0x49a7d07(%rip),%rcx        # 0x55555fff7670 <cpuRegs+464>|-
   0x631ac29b <m+38601371>|  add    $0x50,%ecx|   0x55556499f377 <m+77202295>|  add    $0x50,%ecx|-
   0x631ac29e <m+38601374>|  mov    %ecx,%eax|   0x55556499f37a <m+77202298>|  mov    %ecx,%eax|-
   0x631ac2a0 <m+38601376>|  shr    $0xc,%eax|   0x55556499f37c <m+77202300>|  shr    $0xc,%eax|-
-|-|   0x55556499f37f <m+77202303>|  shl    $0x3,%rax|-
-|-|   0x55556499f383 <m+77202307>|  movabs $0x7fff8919f010,%rbx|-
   0x631ac2a3 <m+38601379>|  mov    -0x48600ff0(,%eax,4),%eax|   0x55556499f38d <m+77202317>|  mov    (%rax,%rbx,1),%eax|-
   0x631ac2aa <m+38601386>|  mov    $0x631ac2bd,%ebx|   0x55556499f390 <m+77202320>|  movabs $0x55556499f3a8,%rbx|-
   0x631ac2af <m+38601391>|  add    %eax,%ecx|   0x55556499f39a <m+77202330>|  add    %eax,%ecx|-
   0x631ac2b1 <m+38601393>|  js     0x70cde140 <_ZL21m_IndirectDispatchers+320>|   0x55556499f39c <m+77202332>|  js     0x55556899f140 <m+144310592>|-
   0x631ac2b7 <m+38601399>|  movlps (%edx),%xmm0|   0x55556499f3a2 <m+77202338>|  movlps (%rdx),%xmm0|-
   0x631ac2ba <m+38601402>|  movlps %xmm0,(%ecx)|   0x55556499f3a5 <m+77202341>|  movlps %xmm0,(%rcx)|-
   0x631ac2bd <m+38601405>|  nop|   0x55556499f3a8 <m+77202344>|  nop|-
   0x631ac2be <m+38601406>|  nop|   0x55556499f3a9 <m+77202345>|  nop|-
   0x631ac2bf <m+38601407>|  mov    $0x60cd51c0,%edx|   0x55556499f3aa <m+77202346>|  movabs $0x55555fff75c0,%rdx|-
   0x631ac2c4 <m+38601412>|  mov    0x60cd5270,%ecx|   0x55556499f3b4 <m+77202356>|  mov    -0x49a7d4b(%rip),%rcx        # 0x55555fff7670 <cpuRegs+464>|-
   0x631ac2ca <m+38601418>|  add    $0x40,%ecx|   0x55556499f3bb <m+77202363>|  add    $0x40,%ecx|-
   0x631ac2cd <m+38601421>|  mov    %ecx,%eax|   0x55556499f3be <m+77202366>|  mov    %ecx,%eax|-
   0x631ac2cf <m+38601423>|  shr    $0xc,%eax|   0x55556499f3c0 <m+77202368>|  shr    $0xc,%eax|-
-|-|   0x55556499f3c3 <m+77202371>|  shl    $0x3,%rax|-
-|-|   0x55556499f3c7 <m+77202375>|  movabs $0x7fff8919f010,%rbx|-
   0x631ac2d2 <m+38601426>|  mov    -0x48600ff0(,%eax,4),%eax|   0x55556499f3d1 <m+77202385>|  mov    (%rax,%rbx,1),%eax|-
   0x631ac2d9 <m+38601433>|  mov    $0x631ac2ec,%ebx|   0x55556499f3d4 <m+77202388>|  movabs $0x55556499f3ec,%rbx|-
   0x631ac2de <m+38601438>|  add    %eax,%ecx|   0x55556499f3de <m+77202398>|  add    %eax,%ecx|-
   0x631ac2e0 <m+38601440>|  js     0x70cde140 <_ZL21m_IndirectDispatchers+320>|   0x55556499f3e0 <m+77202400>|  js     0x55556899f140 <m+144310592>|-
   0x631ac2e6 <m+38601446>|  movlps (%edx),%xmm1|   0x55556499f3e6 <m+77202406>|  movlps (%rdx),%xmm1|-
   0x631ac2e9 <m+38601449>|  movlps %xmm1,(%ecx)|   0x55556499f3e9 <m+77202409>|  movlps %xmm1,(%rcx)|-
   0x631ac2ec <m+38601452>|  nop|   0x55556499f3ec <m+77202412>|  nop|-
   0x631ac2ed <m+38601453>|  nop|   0x55556499f3ed <m+77202413>|  nop|-
   0x631ac2ee <m+38601454>|  mov    $0x60cd51b0,%edx|   0x55556499f3ee <m+77202414>|  movabs $0x55555fff75b0,%rdx|-
   0x631ac2f3 <m+38601459>|  mov    0x60cd5270,%ecx|   0x55556499f3f8 <m+77202424>|  mov    -0x49a7d8f(%rip),%rcx        # 0x55555fff7670 <cpuRegs+464>|-
   0x631ac2f9 <m+38601465>|  add    $0x30,%ecx|   0x55556499f3ff <m+77202431>|  add    $0x30,%ecx|-
   0x631ac2fc <m+38601468>|  mov    %ecx,%eax|   0x55556499f402 <m+77202434>|  mov    %ecx,%eax|-
   0x631ac2fe <m+38601470>|  shr    $0xc,%eax|   0x55556499f404 <m+77202436>|  shr    $0xc,%eax|-
-|-|   0x55556499f407 <m+77202439>|  shl    $0x3,%rax|-
-|-|   0x55556499f40b <m+77202443>|  movabs $0x7fff8919f010,%rbx|-
   0x631ac301 <m+38601473>|  mov    -0x48600ff0(,%eax,4),%eax|   0x55556499f415 <m+77202453>|  mov    (%rax,%rbx,1),%eax|-
   0x631ac308 <m+38601480>|  mov    $0x631ac31b,%ebx|   0x55556499f418 <m+77202456>|  movabs $0x55556499f430,%rbx|-
   0x631ac30d <m+38601485>|  add    %eax,%ecx|   0x55556499f422 <m+77202466>|  add    %eax,%ecx|-
   0x631ac30f <m+38601487>|  js     0x70cde140 <_ZL21m_IndirectDispatchers+320>|   0x55556499f424 <m+77202468>|  js     0x55556899f140 <m+144310592>|-
   0x631ac315 <m+38601493>|  movlps (%edx),%xmm2|   0x55556499f42a <m+77202474>|  movlps (%rdx),%xmm2|-
   0x631ac318 <m+38601496>|  movlps %xmm2,(%ecx)|   0x55556499f42d <m+77202477>|  movlps %xmm2,(%rcx)|-
   0x631ac31b <m+38601499>|  nop|   0x55556499f430 <m+77202480>|  nop|-
   0x631ac31c <m+38601500>|  nop|   0x55556499f431 <m+77202481>|  nop|-
   0x631ac31d <m+38601501>|  mov    $0x60cd51a0,%edx|   0x55556499f432 <m+77202482>|  movabs $0x55555fff75a0,%rdx|-
   0x631ac322 <m+38601506>|  mov    0x60cd5270,%ecx|   0x55556499f43c <m+77202492>|  mov    -0x49a7dd3(%rip),%rcx        # 0x55555fff7670 <cpuRegs+464>|-
   0x631ac328 <m+38601512>|  add    $0x20,%ecx|   0x55556499f443 <m+77202499>|  add    $0x20,%ecx|-
   0x631ac32b <m+38601515>|  mov    %ecx,%eax|   0x55556499f446 <m+77202502>|  mov    %ecx,%eax|-
   0x631ac32d <m+38601517>|  shr    $0xc,%eax|   0x55556499f448 <m+77202504>|  shr    $0xc,%eax|-
   0x631ac330 <m+38601520>|  mov    -0x48600ff0(,%eax,4),%eax|   0x55556499f44b <m+77202507>|  shl    $0x3,%rax|-
   0x631ac337 <m+38601527>|  mov    $0x631ac34a,%ebx|   0x55556499f44f <m+77202511>|  movabs $0x7fff8919f010,%rbx|-
-|-|   0x55556499f459 <m+77202521>|  mov    (%rax,%rbx,1),%eax|-
-|-|   0x55556499f45c <m+77202524>|  movabs $0x55556499f474,%rbx|-
   0x631ac33c <m+38601532>|  add    %eax,%ecx|   0x55556499f466 <m+77202534>|  add    %eax,%ecx|-
   0x631ac33e <m+38601534>|  js     0x70cde140 <_ZL21m_IndirectDispatchers+320>|   0x55556499f468 <m+77202536>|  js     0x55556899f140 <m+144310592>|-
   0x631ac344 <m+38601540>|  movlps (%edx),%xmm3|   0x55556499f46e <m+77202542>|  movlps (%rdx),%xmm3|-
   0x631ac347 <m+38601543>|  movlps %xmm3,(%ecx)|   0x55556499f471 <m+77202545>|  movlps %xmm3,(%rcx)|-
   0x631ac34a <m+38601546>|  xor    %eax,%eax|   0x55556499f474 <m+77202548>|  xor    %eax,%eax|-
   0x631ac34c <m+38601548>|  mov    %eax,0x60cd50e4|   0x55556499f476 <m+77202550>|  movabs %eax,0x55555fff74e4|-
   0x631ac351 <m+38601553>|  mov    %eax,0x60cd50f4|   0x55556499f47f <m+77202559>|  movabs %eax,0x55555fff74f4|-
   0x631ac356 <m+38601558>|  mov    %eax,0x60cd5104|   0x55556499f488 <m+77202568>|  movabs %eax,0x55555fff7504|-
   0x631ac35b <m+38601563>|  mov    %eax,0x60cd5294|   0x55556499f491 <m+77202577>|  movabs %eax,0x55555fff7694|-
   0x631ac360 <m+38601568>|  movl   $0x1,0x60cd50e0|   0x55556499f49a <m+77202586>|  movq   $0x1,-0x49a7fc5(%rip)        # 0x55555fff74e0 <cpuRegs+64>|-
   0x631ac36a <m+38601578>|  movl   $0x2,0x60cd50f0|   0x55556499f4a5 <m+77202597>|  movq   $0x2,-0x49a7fc0(%rip)        # 0x55555fff74f0 <cpuRegs+80>|-
   0x631ac374 <m+38601588>|  movl   $0x1,0x60cd5100|   0x55556499f4b0 <m+77202608>|  movq   $0x1,-0x49a7fbb(%rip)        # 0x55555fff7500 <cpuRegs+96>|-
   0x631ac37e <m+38601598>|  movl   $0x9fc41024,0x60cd5290|   0x55556499f4bb <m+77202619>|  movq   $0xffffffff9fc41024,-0x49a7e36(%rip)        # 0x55555fff7690 <cpuRegs+496>|-
   0x631ac388 <m+38601608>|  movl   $0x9fc42ae8,0x60cd5348|   0x55556499f4c6 <m+77202630>|  mov    $0x9fc42ae8,%eax|-
   0x631ac392 <m+38601618>|  mov    0x60cd5460,%eax|   0x55556499f4cb <m+77202635>|  movabs %eax,0x55555fff7748|-
-|-|   0x55556499f4d4 <m+77202644>|  movabs 0x55555fff7860,%eax|-
   0x631ac397 <m+38601623>|  add    $0xc,%eax|   0x55556499f4dd <m+77202653>|  add    $0xc,%eax|
   0x631ac39a <m+38601626>|  mov    %eax,0x60cd5460|   0x55556499f4e0 <m+77202656>|  movabs %eax,0x55555fff7860|
   0x631ac39f <m+38601631>|  sub    0x58b61820,%eax|   0x55556499f4e9 <m+77202665>|  sub    -0xcb9da2f(%rip),%eax        # 0x555557e01ac0 <g_nextEventCycle>|
   0x631ac3a5 <m+38601637>|  js     0x6319c019 <m+38535193>|   0x55556499f4ef <m+77202671>|  js     0x55556497f03b <m+77070395>|
   0x631ac3ab <m+38601643>|  jmp    0x6319c000 <m+38535168>|   0x55556499f4f5 <m+77202677>|  jmpq   0x55556497f000 <m+77070336>|
