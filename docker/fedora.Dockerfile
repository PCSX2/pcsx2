FROM fedora:31

WORKDIR /project

RUN sudo dnf install -qy \
  https://download1.rpmfusion.org/free/fedora/rpmfusion-free-release-$(rpm -E %fedora).noarch.rpm \
  https://download1.rpmfusion.org/nonfree/fedora/rpmfusion-nonfree-release-$(rpm -E %fedora).noarch.rpm
RUN sudo dnf install -qy 'dnf-command(config-manager)'
RUN sudo dnf config-manager --set-enabled \
  rpmfusion-free \
  rpmfusion-free-updates \
  rpmfusion-nonfree \
  rpmfusion-nonfree-updates

# FIXME: Install gcc-8 instead of 9 if possible. Because 9 has a bug that can
# cause the compiler to segfault:sweat_smile:.
RUN sudo dnf install -qy \
  cmake \
  gcc \
  gcc-c++ \
  git \
  make

# NOTE: Most of these dependencies are from the Fedora forum thread
# (https://forums.pcsx2.net/Thread-PCSX2-for-Fedora?page=32) and special
# thanks to slavezeo (https://forums.pcsx2.net/User-slavezeo) for this:smiley:.
# The following changes were made from the forum post:
# 1. `mesa-libGL.i686` and `mesa-dri-drivers.i686` were added because otherwise
#   PCSX2 would fail when running a PS2 game with an insufficient resources
#   message. The testing environment was Fedora 30 as a Xen VM on Qubes OS
#   R4.0.1:smiley:.
# 2. `harfbuzz-devel.i686` was added because otherwise not all of its parts
#   would be found by `cmake`:sweat_smile:. And this only works if it's
#   installed last separately:sweat_smile:.
RUN sudo dnf install -qy \
  alsa-lib-devel.i686 \
  bzip2-libs.i686 \
  freetype-devel.i686 \
  glew-devel.i686 \
  glib2-devel.i686 \
  glibc-devel.i686 \
  gtk3-devel.i686 \
  libaio-devel.i686 \
  libao-devel.i686 \
  libCg.i686 \
  libGLEW.i686 \
  libjpeg-turbo-devel.i686 \
  libpcap-devel.i686 \
  libpng-devel.i686 \
  libpng.i686 \
  libX11-devel.i686 \
  libxml2-devel.i686 \
  mesa-dri-drivers.i686 \
  mesa-libGL-devel.i686 \
  mesa-libGL.i686 \
  portaudio-devel.i686 \
  SDL-devel.i686 \
  SDL2-devel.i686 \
  soundtouch-devel.i686 \
  sparsehash-devel.i686 \
  systemd-devel.i686 \
  wxGTK3-devel.i686 \
  xz-devel.i686 \
  zlib-devel.i686

RUN sudo dnf install -qy harfbuzz-devel.i686

COPY . /project

CMD ["./build.sh"]
