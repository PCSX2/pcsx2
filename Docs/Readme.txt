



PCSX2 - A PS2 EMU 
------------------
Here it is. A first try for an ps2 emulator...
Of course it isn't very advance now but there are some stuff here...



Overview
--------
Well i will try to catch up some questions.
First of all pcsx2 don't run Ps2 games yet!
And of course it is far from doing this.
So pcsx2 don't run GT3, get it? :)
So what pcsx2 is? pcsx2 is a try to emulate sony's beast.
Of course it isn't so easy as it might seems.
So far you can consider pcsx2 as a develop tool althought 
i suggest don't use pcsx2 as a tool for writing your ps2dev
stuff :). Consider the opinion that pcsx2 have bugs and we 
wrote this emu by reverse enginnering ps2 demos that might 
have bugs too :)
Hope you enjoy pcsx2..

The Pcsx2 team..


Configuration
-------------

Cpu Options:


Misc Options:

 * Enable Console Output:
    Displays the psx text output.


 * Enable patches
    Enable the patches for games.(if they exist).
    Might fix some stuff might screw some stuff. 
    Enable it at your own risk.
 
 * Enable pad hack. 
    if your pads doesn't seem to work if you enable that much fix the pads for the specific game.
    Warning!! not leave that option checked might broke some other games as well

Recompiler options:

*  Disable Recompiler (default). It will run with interpreter if it is ON.
   Slower but more compatible.

*  Disable Vu recompiler (default). Will disable the vu recompile 
  (of course if recompile mode is used). More compatible recompiler but slower.

 * Enable reg caching (disabled in 0.6)
    Enable the reg caching recompiler (you must have enable interpeter cpu off!)
    It is more faster than the default recompiler


 
Quick Keys:
 F1: Save state
 F2: Change slot (0-5)
 F3: Load State
 F8: Makes a Snapshot
 
 (debugger keys)
----------------
 F11  un/sets Log
 F12 un/sets symbol logging 

Status
------

Most part of ps2 have been emulate.

Things that are still missing or uncomplete

IPU : decoding almost done. Pcsx2 can play *.ipu or *.m2v files but no pss yet
VU  : there are several issues with graphics ingames. Still we are not sure if it is GS, VIF or VU problems
      but we are looking for it 
recompiler: planning for fast reg cache core and recompile of vus . Soon :P


and of course a million other bugs that exists and we hope they will be fixed ;0





How you can help
----------------
If you have any info you think we can use email us, but always ask before
sending files. If you want to help in some other way also email us.



The Team
--------
Nickname    | Real Name           |   Place   | Occupation        | e-mail                    |  Comments 
---------------------------------------------------------------------------------------------------------------
Linuzappz   |                     | Argentina | Main coder        | linuzappz@pcsx.net        | Master of The GS emulation and so many others..
Shadow      | George Moralis      | Greece    | co-coder-webmaster| shadowpcsx2@yahoo.gr      | Master of cpu, master of bugs, general coding...
florin      | Florin Sasu         | Romania   | co-coder          | florin@pcsx2.net          | Master of HLE. Master of cd code and bios HLE..
asadr       |                     | Pakistan  | co-coder          |                           | Fixing bugs around (FPU, Interpreter, VUs...)
Goldfinger  |                     | Brazil    | co-coder          |                           | MMI,FPU  and general stuff
Nachnbrenner|                     | Germany   | co-coder          |                           | patch freak :P
aumatt      |                                 | co-coder          |                           | a bit of all mostly handles CDVD cmds
loser       |                     | Australia | co-coder          | loser@internalreality.com | obscure cdvd related stuff
refraction  | Alex Brown          | England   | co-coder          | refraction@gmail.com      | General Coding DMA/VIF etc

ex-coders: 
basara     -co-coder  . Recompiler programmer. general coding
[TyRaNiD]  -co-coder  . GS programmer.General coding
Roor       -co-coder  . General coding 


Additional coding: 

F|RES
Pofis    
Gigaherz
nocomp


BETA testers
------------
belmont
parotaku
bositman
CKemu
Raziel
Snake875
Elly
CpUMasteR
Falcon4Ever


Team like to thanks the Follow people
-------------------------------------
Duke of NAPALM - for the 3d stars demo. The first demo that worked in pcsx2 :)
Tony Saveski (dreamtime) - for his great ps2tutorials!!
F|res     - You will learn more about him soon. but a big thanks from shadow..
Now3d     - The guy that helped me at my first steps..
Keith     - Who believed in us..
Bobbi     - Thorgal: for hosting us, for design or page and some many other 
Sjeep     - Help and info
BGnome    - Help testing stuff
Dixon     - Design the new pcsx2 page, and the pcsx2.net domain
bositman  - pcsx2 beta tester :)  (gia sou bositman pare ta credits sou )
No-Reccess- nice guy and great demo coder :) 
nsx2 team - for help to vu ;)
razorblade - for the new pcsx2 logo,icon.
snake      - he knows what for :P
ector      - awesome emu :)
zezu       - a good guy. good luck with your emu :P




Credits
--------------
Hiryu & Sjeep - for their libcdvd (iso parsing and filesystem driver code)
Sjeep - for SjDATA filesystem driver
F|res - for the original DECI2 implementation
libmpeg2 - for mpeg2 decoding routines
aumatt   - for applying fixes to pcsx2
Microsoft - for vc.net 2003 :p (really faster than vc6) :P
NASM team - for nasm
CKemu - logos/design


and probably to a few more..

Special Shadow's thanks go to...
--------------------------------
My friends : Dimitris, james, thodoris, thanasis and probably to a few more..
and of course to a lady somewhere out there....





Log off/
Linuzappz/ shadow / florin / asad/  goldfinger / nachbrenner (others???)







