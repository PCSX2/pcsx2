# PCSX2 Docker Build-Testing Images

## How to Use

Assuming you have Docker installed and configured you can do the following:
```bash
# For the Ubuntu image.
docker build --tag pcsx2-ubuntu --file docker/ubuntu.Dockerfile .
docker run --rm -it pcsx2-ubuntu ./build.sh --gtk3

# For the Debian image.
docker build --tag pcsx2-debian --file docker/debian.Dockerfile .
docker run --rm -it pcsx2-debian ./build.sh --gtk3

# For the Fedora image.
docker build --tag pcsx2-fedora --file docker/fedora.Dockerfile .
docker run --rm -it pcsx2-fedora ./build.sh --gtk3
```

## Benefits

1. Living documentation on how to compile PCSX2 from source on different
  environments anyone can check that is confirmed and proven to work:smiley:.
2. Ensuring sufficient tooling is present in PCSX2 to build/compile/develop on
  different environments and preventing deviation from this:smiley:.
3. Catching programming portability-errors as soon as possible:smiley:.
4. Highlighting problems that make building and installing PCSX2 harder and more
  volatile. For example, the parts tied to x86/i386/i686 and lack of
  universal packaging (e.g: Flatpack/Snap). In particular, Flatpack can make
  local development easier in addition to packaging:smiley:.
5. Understanding and knowing PCSX2 dependencies better in action:smiley:.
6. Making it easier to develop PCSX2 by just using these images either as
  reference or by building/compiling PCSX2 using them directly without having to
  install all the dependencies locally which may not be possible or practical in
  many environments:sweat_smile:. This would be especially true if the
  dependencies are statically linked to the built PCSX2 binary:smiley:. Still
  more detailed explaination of Docker basics would need to be added to make
  this more accessible and save many people lots of time and
  frustration:sweat_smile:.
7. Now that compiling/building PCSX2 is easier, things for which this may have
  been a bottlenick can be done (e.g: Flatpack/Snap):smiley:.
8. More people can work on PCSX2 now that the building/compiling from source
  wall has been replaced by the Docker wall:sweat_smile:.

## Notes (as of the date of this writing 2019-11-07):

1. The Fedora 31 image commands were tested on Qubes OS R4.0.1 in a fresh
  Fedora 30 Xen VM and confirmed to work correctly and play games:smiley:. The
  The Debian image commands weren't tested because of a multiarch issue where
  Qubes OS packages would be removed from the VM and the Ubuntu image commands
  weren't tested because Canonical does not allow redistribution of a modified
  Ubuntu which makes it harder to run on Qubes OS:sweat_smile:.
2. The Ubuntu 18.04 image commands were not tested because Canonical does not
  allow redistribution of a modified Ubuntu which makes it harder to run on
  Qubes OS and it shouldn't be wildly different from the latest Debian releaseanyway:sweat_smile:. The Ubuntu image was provided just because many
  people use it:sweat_smile:.
3. 64bit/x86_64/AMD64 is the main focus because x86 seems to be dying off and
  even systems that have multiarch support can have some stoppers:sweat_smile:.
  For example, the Fedora 31 image instructions work on Fedora 28-31 except
  Fedora 29 with an error related to a conflict between `glib2-devel.i686` and
  `glib2-devel.x86_64`:sweat_smile:.
4. GTK3 is the main focus as opposed to GTK2 because the future of the project
  is probably more important than the past:sweat_smile:. And Qt may be a better
  option for the UI but this is a different topic and GTK3/WxWidgets could be
  the best choice for PCSX2:sweat_smile:.
5. The focus is on a single version for each OS just for convenience to get
  things started.
6. Docker is the main and hopefully only requirement for this. And `19.03.4` is
  the version that was used to build and test these images:sweat_smile:.
7. These images are intended to be added to the CI pipeline (e.g: TravisCI) for
  many obvious reasons (e.g: making sure they always work):smiley:.
8. Only Linux images are provided because most other OSes have considerably
  fewer variants, less supported on Docker, and not used as much for
  building/compiling:sweat_smile:. Debian & Fedora images are used because many
  distros derive from them and they are easier to work with and Ubuntu
  because it's very popular:sweat_smile:.
9. `build.sh`
  (https://github.com/PCSX2/pcsx2/blob/3c38087e78b8b5302375dbb571e8c76b904ee697/build.sh),
  which is used to build PCSX2 exhibits the default BASH behavior of not failing
  if an intermediate step/process has failed:sweat_smile:. Sooo just keep this
  in mind and look for it if you observe confusing
  behavior/results:sweat_smile:.

## Gotchas

1. Official Docker images don't exactly match a typical OS image you download
  and install:sweat_smile:.
2. Most package managers are non-deterministic and installing some packages in a
  clean environment in a different order can lead to different results let alone
  an existing environment:sweat_smile:.
3. You can run the exact commands in the same order as in the Dockerfile using
  the same OS and version but still not be able to build/compile
  PCSX2:sweat_smile:. This is why it may be best to build/compile only using the
  Docker image even if you have a dedicated development
  environment:sweat_smile:.
