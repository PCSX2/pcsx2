#!/bin/sh

set -ex

linux_x86_before_install() {
	if [ "${COMPILER}" = "clang" ]; then
		export CC=clang
		export CXX=clang++
	else
		export CC=gcc
		export CXX=g++
	fi

	# Build worker is 64-bit only by default it seems.
	sudo dpkg --add-architecture i386

	# Compilers
	if [ "${CXX}" = "clang++" ]; then
		COMPILER_PACKAGE="g++-${VERSION}-multilib"
	fi
	if [ "${CXX}" = "g++" ]; then
		COMPILER_PACKAGE="g++-${VERSION}-multilib"
	fi


	# The 64-bit versions of the first 7 dependencies are part of the initial
	# build image. libgtk2.0-dev:i386 and libsdl2-dev:i386 require the 32-bit
	# versions of the dependencies, and the 2 versions conflict. So those
	# dependencies must be explicitly installed.
	# Sometimes it complains about Python so we install that too.

	# TODO - i suspect there are unneeded dependencies here
	sudo apt-get -qq update
	sudo apt-get -y install \
		cmake \
		ccache \
		libwxgtk3.0-gtk3-dev:i386 \
		libgtk-3-dev:i386 \
		libaio-dev:i386 \
		libasound2-dev:i386 \
		liblzma-dev:i386 \
		libsdl2-dev:i386 \
		libsoundtouch-dev:i386 \
		libxml2-dev:i386 \
		libpcap0.8-dev:i386 \
		libx11-xcb-dev:i386 \
		gir1.2-freedesktop:i386 \
		gir1.2-gdkpixbuf-2.0:i386 \
		gir1.2-glib-2.0:i386 \
		libcairo2-dev:i386 \
		libegl1-mesa-dev:i386 \
		libgdk-pixbuf2.0-dev:i386 \
		libgirepository-1.0-1:i386 \
		libglib2.0-dev:i386 \
		libgl1-mesa-dev:i386 \
		libglu1-mesa-dev:i386 \
		libgtk2.0-dev:i386 \
		libharfbuzz-dev:i386 \
		libpango1.0-dev:i386 \
		libxext-dev:i386 \
		libxft-dev:i386 \
		portaudio19-dev:i386 \
		zlib1g-dev:i386 \
		${COMPILER_PACKAGE}
}

linux_x86_script() {
	mkdir build
	cd build

	if [ "${COMPILER}" = "clang" ]; then
		export CC=clang
		export CXX=clang++
	else
		export CC=gcc
		export CXX=g++
	fi

	# Prevents warning spam
	if [ "${CXX}" = "clang++" ]; then
		export CCACHE_CPP2=yes
		export CC=${CC} CXX=${CXX}
	else
		export CC=${CC}-${VERSION}
		export CXX=${CXX}-${VERSION}
	fi

	export CCACHE_BASEDIR=${GITHUB_WORKSPACE}
	export CCACHE_DIR=${GITHUB_WORKSPACE}/.ccache
	export CCACHE_COMPRESS="true"
	export CCACHE_COMPRESSLEVEL="6"
	export CCACHE_MAXSIZE="400M"

	ccache -p
	ccache -z

	cmake \
		-D CMAKE_C_COMPILER_LAUNCHER=ccache \
		-D CMAKE_CXX_COMPILER_LAUNCHER=ccache \
		-D CMAKE_TOOLCHAIN_FILE=cmake/linux-compiler-i386-multilib.cmake \
		-D CMAKE_BUILD_TYPE=Release \
		-D BUILD_REPLAY_LOADERS=TRUE \
		-D CMAKE_BUILD_PO=FALSE \
		-D GTK3_API=TRUE \
		..

	make -j2 install

	ccache -s
}


linux_x64_before_install() {
	echo ${COMPILER}
	if [ "${COMPILER}" = "clang" ]; then
		echo "Clang!"
		export CC=clang
		export CXX=clang++
	else
		echo "GCC!"
		export CC=gcc
		export CXX=g++
	fi

	# Compilers
	if [ "${CXX}" = "g++" ]; then
		sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
		COMPILER_PACKAGE="g++-${VERSION}"
	fi

	sudo apt-get -qq update

	# libgl1-mesa-dev, liblzma-dev, libxext-dev, zlib1g-dev already installed on
	# build worker, I put these here in case the build image changes.
	sudo apt-get -y install \
		cmake \
		ccache \
		libwxgtk3.0-gtk3-dev \
		libgtk-3-dev \
		libaio-dev \
		libasound2-dev \
		liblzma-dev \
		libsdl2-dev \
		libsoundtouch-dev \
		libxml2-dev \
		libpcap0.8-dev \
		libx11-xcb-dev \
		gir1.2-freedesktop \
		gir1.2-gdkpixbuf-2.0 \
		gir1.2-glib-2.0 \
		libcairo2-dev \
		libegl1-mesa-dev \
		libgdk-pixbuf2.0-dev \
		libgirepository-1.0-1 \
		libglib2.0-dev \
		libgl1-mesa-dev \
		libglu1-mesa-dev \
		libgtk2.0-dev \
		libharfbuzz-dev \
		libpango1.0-dev \
		libxext-dev \
		libxft-dev \
		portaudio19-dev \
		zlib1g-dev \
		${COMPILER_PACKAGE}
}


linux_x64_script() {
	mkdir build
	cd build

	if [ "${COMPILER}" = "clang" ]; then
		export CC=clang
		export CXX=clang++
	else
		export CC=gcc
		export CXX=g++
	fi

	export CCACHE_BASEDIR=${GITHUB_WORKSPACE}
	export CCACHE_DIR=${GITHUB_WORKSPACE}/.ccache
	export CCACHE_COMPRESS="true"
	export CCACHE_COMPRESSLEVEL="6"
	export CCACHE_MAXSIZE="400M"

	export CC=${CC}-${VERSION} CXX=${CXX}-${VERSION}

	ccache -p
	ccache -z

	cmake \
		-D CMAKE_C_COMPILER_LAUNCHER=ccache \
		-D CMAKE_CXX_COMPILER_LAUNCHER=ccache \
		-D CMAKE_BUILD_TYPE=Devel \
		-D BUILD_REPLAY_LOADERS=TRUE \
		-D CMAKE_BUILD_PO=FALSE \
		-D GTK3_API=TRUE \
		..

	make -j2 install

	ccache -s
}

# Just in case I do manual testing and accidentally insert "rm -rf /"
case "${1}" in
before_install|script)
	linux_${PLATFORM}_${1}
	;;
*)
	echo "Unknown command" && false
	;;
esac
