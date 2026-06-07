# SoundTouch library

## About

SoundTouch is an open-source audio processing library that allows changing the sound tempo, pitch and playback rate parameters independently from each other:
* Change **tempo** while maintaining the original pitch
* Change **pitch** while maintaining the original tempo
* Change **playback rate** that affects both tempo and pitch at the
same time
* Change any combination of tempo/pitch/rate

Visit [SoundTouch website](https://www.surina.net/soundtouch) and see the [README file](https://www.surina.net/soundtouch/readme.html) for more information and audio examples.

## Version

**The latest stable release in Git is 2.4.1**

See the [README file for change history](https://soundtouch.surina.net/README.html#changehistory)

[![latest packaged version(s)](https://repology.org/badge/latest-versions/soundtouch.svg)](https://repology.org/project/soundtouch/versions)

## Example

Use SoundStretch example app for modifying wav audio files, for example as follows:

```
soundstretch my_original_file.wav output_file.wav -tempo=+15 -pitch=-3
```

See the [README file](https://soundtouch.surina.net/README.html) for more usage examples and instructions how to build the software.

Ready [SoundStretch application executables](https://www.surina.net/soundtouch/download.html) are available for download for Windows and Mac OS.

## Language & Platforms

SoundTouch is written in C++ and compiles in virtually any platform:
* Windows
* Mac OS
* Linux & Unices (including also Raspberry, Beaglebone, Yocto etc embedded Linux flavors)
* Android
* iOS
* embedded systems

The source code package includes dynamic library import modules for C#, Java and Pascal/Delphi languages.

## Tarballs

Source code release tarballs:
* https://www.surina.net/soundtouch/soundtouch-2.4.1.tar.gz
* https://www.surina.net/soundtouch/soundtouch-2.4.0.tar.gz
* https://www.surina.net/soundtouch/soundtouch-2.3.3.tar.gz
* https://www.surina.net/soundtouch/soundtouch-2.3.2.tar.gz
* https://www.surina.net/soundtouch/soundtouch-2.3.1.tar.gz
* https://www.surina.net/soundtouch/soundtouch-2.3.0.tar.gz
* https://www.surina.net/soundtouch/soundtouch-2.2.0.tar.gz
* https://www.surina.net/soundtouch/soundtouch-2.1.2.tar.gz
* https://www.surina.net/soundtouch/soundtouch-2.1.1.tar.gz
* https://www.surina.net/soundtouch/soundtouch-2.0.0.tar.gz

## License

SoundTouch is released under LGPL v2.1:

This library is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License version 2.1 as published by the Free Software Foundation.

This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with this library; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

See [LGPL v2.1 full license text ](https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html) for details.

--

Also commercial license free of GPL limitations available upon request
