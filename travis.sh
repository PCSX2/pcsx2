#!/bin/sh

set -ex

linux_32_before_install() {
	# Build worker is 64-bit only by default it seems.
	sudo dpkg --add-architecture i386

	sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test

	# Compilers
	if [ "${CXX}" = "clang++" ]; then
		sudo apt-key adv --fetch-keys http://llvm.org/apt/llvm-snapshot.gpg.key
		sudo add-apt-repository -y "deb http://llvm.org/apt/trusty/ llvm-toolchain-trusty-${VERSION} main"
		# g++-4.9-multilib is necessary for compiler dependencies. 4.8 currently
		# has dependency issues, but 4.9 from the toolchain repo seems to work
		# fine, so let's just use that.
		COMPILER_PACKAGE="clang-${VERSION} g++-4.9-multilib"
	fi
	if [ "${CXX}" = "g++" ]; then
		COMPILER_PACKAGE="g++-${VERSION}-multilib"
	fi

	# apt-get update fails because Chrome is 64-bit only.
	sudo rm -f /etc/apt/sources.list.d/google-chrome.list

	sudo apt-get -qq update

	# The 64-bit versions of the first 7 dependencies are part of the initial
	# build image. libgtk2.0-dev:i386 and libsdl2-dev:i386 require the 32-bit
	# versions of the dependencies, and the 2 versions conflict. So those
	# dependencies must be explicitly installed.
	sudo apt-get -qq -y install \
		gir1.2-freedesktop:i386 \
		gir1.2-gdkpixbuf-2.0:i386 \
		gir1.2-glib-2.0:i386 \
		libcairo2-dev:i386 \
		libgdk-pixbuf2.0-dev:i386 \
		libgirepository-1.0-1:i386 \
		libglib2.0-dev:i386 \
		libaio-dev:i386 \
		libasound2-dev:i386 \
		libgl1-mesa-dev:i386 \
		libgtk2.0-dev:i386 \
		liblzma-dev:i386 \
		libpng12-dev:i386 \
		libsdl2-dev:i386 \
		libsoundtouch-dev:i386 \
		libwxgtk3.0-dev:i386 \
		libxext-dev:i386 \
		portaudio19-dev:i386 \
		zlib1g-dev:i386 \
		${COMPILER_PACKAGE}
}

linux_32_script() {
	mkdir build
	cd build

	export CC=${CC}-${VERSION} CXX=${CXX}-${VERSION}
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=cmake/linux-compiler-i386-multilib.cmake \
		-DCMAKE_BUILD_TYPE=Release \
		-DBUILD_REPLAY_LOADERS=TRUE \
		-DCMAKE_BUILD_PO=FALSE \
		..

	# Documentation says 1.5 cores, so 2 or 3 threads should work ok.
	make -j3 install
}


linux_64_before_install() {
	# Compilers
	if [ "${CXX}" = "clang++" ]; then
		sudo apt-key adv --fetch-keys http://llvm.org/apt/llvm-snapshot.gpg.key
		sudo add-apt-repository -y "deb http://llvm.org/apt/trusty/ llvm-toolchain-trusty-${VERSION} main"
		COMPILER_PACKAGE="clang-${VERSION}"
	fi
	if [ "${CXX}" = "g++" ]; then
		sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
		COMPILER_PACKAGE="g++-${VERSION}"
	fi

	sudo apt-get -qq update

	# libgl1-mesa-dev, liblzma-dev, libxext-dev, zlib1g-dev already installed on
	# build worker, I put these here in case the build image changes.
	sudo apt-get -qq -y install \
		libaio-dev \
		libasound2-dev \
		libgtk2.0-dev \
		libpng12-dev \
		libsdl2-dev \
		libsoundtouch-dev \
		libwxgtk3.0-dev \
		portaudio19-dev \
		${COMPILER_PACKAGE}
}


linux_64_script() {
	mkdir build
	cd build

	export CC=${CC}-${VERSION} CXX=${CXX}-${VERSION}
	cmake \
		-DCMAKE_BUILD_TYPE=Devel \
		-DBUILD_REPLAY_LOADERS=TRUE \
		-DCMAKE_BUILD_PO=FALSE \
		..

	# Documentation says 1.5 cores, so 2 or 3 threads should work ok.
	make -j3 install
}

# Just in case I do manual testing and accidentally insert "rm -rf /"
case "${1}" in
before_install|script)
	${TRAVIS_OS_NAME}_${BITS}_${1}
	;;
*)
	echo "Unknown command" && false
	;;
esac
