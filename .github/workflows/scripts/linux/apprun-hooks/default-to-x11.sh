if [[ -z "$I_WANT_A_BROKEN_WAYLAND_UI" ]]; then
	echo "Forcing X11 instead of Wayland, due to various protocol limitations"
	echo "and Qt issues. If you want to use Wayland, launch PCSX2 with"
	echo "I_WANT_A_BROKEN_WAYLAND_UI=YES set."
	export QT_QPA_PLATFORM=xcb
else
	echo "Wayland is not being disabled. Do not complain when things break."
fi

