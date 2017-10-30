function draw()
	gui.drawLine(20, 20, 100, 400, "green")
	gui.text(50, 50, "This is a test", "magenta")
	gui.box(200, 200, 250, 250, 255, 125, 125, 255, "yellow")
	gui.rectangle(300, 300, 100, 100, 126, 135, 240, 255, 165, 140, 213, 125)
	gui.pixel(225, 225, "green")
	gui.ellipse(400, 400, 50, 50, "magenta", "yellow")
	gui.circle(500, 500, 25, 123, 142, 12, 123, 157, 184, 160, 147)
end

emu.registerbefore(draw)