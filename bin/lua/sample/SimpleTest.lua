
----------------------------------------------------------
-- sleep one
----------------------------------------------------------
local function sleepOne()
  local start = os.time()
  while os.time() - start < 1 do end
end 

----------------------------------------------------------
-- info
----------------------------------------------------------
function printStr(s1,s2)
	if s2 ~= nil then
		print( s1 .. " : " ..  s2 )
	end
end
printStr("mode" , movie.mode())
printStr("length" , movie.length())
printStr("author" , movie.author())
printStr("cdrom" , movie.cdrom())
printStr("name" , movie.name())
printStr("rerecordcount" , movie.rerecordcount())


----------------------------------------------------------
-- register
----------------------------------------------------------
emu.registerbefore(function()
	print("register before:" .. emu.framecount() );
end)
emu.registerafter(function()
	print("register after :" .. emu.framecount() );
end)
emu.registerexit(function()
	print("exit:" .. emu.framecount() );
end)

----------------------------------------------------------
-- loop frame
----------------------------------------------------------
print("start : " .. emu.framecount())
for i=1,5 do
	
	-- read memory
	local m = memory.readdword(0x480000 , "r3000");
	print("before(" .. emu.framecount()  .. ") readword(0x480000):" ..  m);
	emu.frameadvance();
	print(" after(" .. emu.framecount()  .. ")");
	
end
print("end : " .. emu.framecount())
emu.pause()




