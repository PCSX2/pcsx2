#!/bin/sh

set -ex

clang_syntax_check() {
	if [ "${CXX}" = "clang++" ]; then
        ./linux_various/check_format.sh
	fi
}

linux_32_before_install() {
	# Build worker is 64-bit only by default it seems.
	sudo dpkg --add-architecture i386

	sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test

	# Compilers
	if [ "${CXX}" = "clang++" ]; then
		sudo apt-key adv --fetch-keys http://apt.llvm.org/llvm-snapshot.gpg.key
		sudo add-apt-repository -y "deb http://apt.llvm.org/trusty/ llvm-toolchain-trusty-${VERSION} main"
		# g++-x-multilib is necessary for compiler dependencies.
		COMPILER_PACKAGE="clang-${VERSION} g++-7-multilib clang-format-${VERSION}"
	fi
	if [ "${CXX}" = "g++" ]; then
		# python:i386 is required to avoid dependency issues for gcc-4.9 and
		# gcc-7. It causes issues with clang-format though, so the dependency is
		# only specified for gcc.
		COMPILER_PACKAGE="g++-${VERSION}-multilib python:i386"
	fi

	sudo apt-get -qq update

	# The 64-bit versions of the first 7 dependencies are part of the initial
	# build image. libgtk2.0-dev:i386 and libsdl2-dev:i386 require the 32-bit
	# versions of the dependencies, and the 2 versions conflict. So those
	# dependencies must be explicitly installed.
	sudo apt-get -y install \
		gir1.2-freedesktop:i386 \
		gir1.2-gdkpixbuf-2.0:i386 \
		gir1.2-glib-2.0:i386 \
		libcairo2-dev:i386 \
		libegl1-mesa-dev:i386 \
		libgdk-pixbuf2.0-dev:i386 \
		libgirepository-1.0-1:i386 \
		libglib2.0-dev:i386 \
		libaio-dev:i386 \
		libasound2-dev:i386 \
		libgl1-mesa-dev:i386 \
		libglu1-mesa-dev:i386 \
		libgtk2.0-dev:i386 \
		liblzma-dev:i386 \
		libpango1.0-dev:i386 \
		libpng12-dev:i386 \
		libsdl2-dev:i386 \
		libsoundtouch-dev:i386 \
		libwxgtk3.0-dev:i386 \
		libxext-dev:i386 \
		libxft-dev:i386 \
		portaudio19-dev:i386 \
		zlib1g-dev:i386 \
		${COMPILER_PACKAGE}

	# Manually add ccache symlinks for clang
	if [ "${CXX}" = "clang++" ]; then
		sudo ln -sf ../../bin/ccache /usr/lib/ccache/${CXX}-${VERSION}
		sudo ln -sf ../../bin/ccache /usr/lib/ccache/${CC}-${VERSION}
	fi
}

linux_32_script() {
	mkdir build
	cd build

	# Prevents warning spam
	if [ "${CXX}" = "clang++" ]; then
		export CCACHE_CPP2=yes
	fi
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
		sudo apt-key adv --fetch-keys http://apt.llvm.org/llvm-snapshot.gpg.key
		sudo add-apt-repository -y "deb http://apt.llvm.org/trusty/ llvm-toolchain-trusty-${VERSION} main"
		COMPILER_PACKAGE="clang-${VERSION} clang-format-${VERSION}"
	fi
	if [ "${CXX}" = "g++" ]; then
		sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
		COMPILER_PACKAGE="g++-${VERSION}"
	fi

	sudo apt-get -qq update

	# libgl1-mesa-dev, liblzma-dev, libxext-dev, zlib1g-dev already installed on
	# build worker, I put these here in case the build image changes.
	sudo apt-get -y install \
		libaio-dev \
		libasound2-dev \
		libegl1-mesa-dev \
		libgtk2.0-dev \
		libpng12-dev \
		libsdl2-dev \
		libsoundtouch-dev \
		libwxgtk3.0-dev \
		portaudio19-dev \
		${COMPILER_PACKAGE}

	# Manually add ccache symlinks for clang
	if [ "${CXX}" = "clang++" ]; then
		sudo ln -sf ../../bin/ccache /usr/lib/ccache/${CXX}-${VERSION}
		sudo ln -sf ../../bin/ccache /usr/lib/ccache/${CC}-${VERSION}
	fi
}


linux_64_script() {
	mkdir build
	cd build

	# Prevents warning spam
	if [ "${CXX}" = "clang++" ]; then
		export CCACHE_CPP2=yes
	fi
	export CC=${CC}-${VERSION} CXX=${CXX}-${VERSION}
	cmake \
		-DCMAKE_BUILD_TYPE=Devel \
		-DBUILD_REPLAY_LOADERS=TRUE \
		-DCMAKE_BUILD_PO=FALSE \
		..

	# Documentation says 1.5 cores, so 2 or 3 threads should work ok.
	make -j3 install
}

linux_after_success() {
	ccache -s
}

# Just in case I do manual testing and accidentally insert "rm -rf /"
case "${1}" in
before_install|script)
	${TRAVIS_OS_NAME}_${BITS}_${1}
	;;
before_script)
    clang_syntax_check
    ;;
after_success)
	${TRAVIS_OS_NAME}_${1}
	;;
*)
	echo "Unknown command" && false
	;;
esac
