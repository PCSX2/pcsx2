# NOTE: The Debian and Ubuntu Dockerfiles are almost identical:sweat_smile:.
FROM ubuntu:18.04

WORKDIR /project

RUN dpkg --add-architecture i386

RUN apt-get update -qq

RUN apt-get install -qqy \
  cmake \
  g++ \
  g++-multilib \
  gcc \
  gcc-multilib \
  git \
  make \
  > /dev/null

# NOTE: Most of these dependencies are from the PCSX2's TravisCI BASH script
# (https://github.com/PCSX2/pcsx2/blob/3c38087e78b8b5302375dbb571e8c76b904ee697/travis.sh#L36).
# The main changes are the following:
# 1. `libpng12-dev:i386` was not found on Debian 10 (Buster) and was replaced by
#   `libpng-dev:i386`:sweat_smile:.
# 2. `libgtk-3-dev:i386` was added because otherwise `GTK3` and other related
#   dependencies would be reported missing by `cmake`:sweat_smile:.
# 3. `libwxgtk3.0-gtk3-dev:i386` was added because otherwise `wxWidgets`
#   would be reported missing by `cmake`:sweat_smile:.
#
# NOTE: Not all of its parts would be found by `cmake` but PCSX2 should still
# compile successfully:sweat_smile:. This can be seen in a successful PCSX2 x86
# CI job (https://travis-ci.org/PCSX2/pcsx2/jobs/598394643#L2544). There's a way
# a trick in make `cmake` find all HarfBuzz parts but it's kinda hacky and may
# not work in all OSes:sweat_smile:.
RUN apt-get install -qqy \
  gir1.2-freedesktop:i386 \
  gir1.2-gdkpixbuf-2.0:i386 \
  gir1.2-glib-2.0:i386 \
  libaio-dev:i386 \
  libasound2-dev:i386 \
  libcairo2-dev:i386 \
  libegl1-mesa-dev:i386 \
  libgdk-pixbuf2.0-dev:i386 \
  libgirepository-1.0-1:i386 \
  libgl1-mesa-dev:i386 \
  libglib2.0-dev:i386 \
  libglu1-mesa-dev:i386 \
  libgtk-3-dev:i386 \
  libgtk2.0-dev:i386 \
  libharfbuzz-dev:i386 \
  liblzma-dev:i386 \
  libpango1.0-dev:i386 \
  libpcap0.8-dev:i386 \
  libpng-dev:i386 \
  libsdl2-dev:i386 \
  libsoundtouch-dev:i386 \
  libwxgtk3.0-dev:i386 \
  libwxgtk3.0-gtk3-dev:i386 \
  libxext-dev:i386 \
  libxft-dev:i386 \
  libxml2-dev:i386 \
  portaudio19-dev:i386 \
  python:i386 \
  zlib1g-dev:i386 \
  > /dev/null

COPY . /project

CMD ["./build.sh"]
