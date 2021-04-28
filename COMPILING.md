# Compiling on Linux

Usually, when you want to compile some software on Linux, you'll find a list of
packages to install, which are usually distribution specific. As this is far from
being user friendly and can mess with your system, we will instead use
[Nix](https://nixos.org/) to
create a development environment separate from your system.

## 1. Downloading the source code

In order to compile PCSX2 from its source code, you will first need to download
it. In order to do so run the following commands:

```
$ git clone --recurse-submodules https://github.com/PCSX2/pcsx2.git
$ cd pcsx2
```

This will put you inside a folder containing PCSX2's source code.

## 2. Installing Nix

Installing [Nix](https://nixos.org/) will require administrator rights. This is
a one-off thing and it won't ask you again, even when you need to install
pcsx2's dependencies! Make sure you have them (hint: if you can run `sudo` then
it's okay) and run the following commands:

```
$ curl -L https://nixos.org/nix/install | sh
$ . /home/$(whoami)/.nix-profile/etc/profile.d/nix.sh
```

## 3. Enter the development environment 

Now in order to compile PCSX2 you need to enter the development environment. To
do so run the following command:

```
$ nix-build /nix/var/nix/profiles/per-user/$(whoami)/channels/nixpkgs/ --run-env -A pcsx2
```

If you see `nix-shell` at the beginning of your shell, then all is good, no need
to run this command again! To exit the environment at any time, just run `exit`.

## 4. Compile PCSX2

In order to compile PCSX2, make sure you're inside the folder you downloaded
earlier and run the following commands:

```
$ mkdir build
$ cd build
$ cmake ..
$ make -j$(nproc) install
```

You should be good to go! You should be able to run PCSX2 by executing `../bin/PCSX2`.

# Re-using the environment

If you want to reuse the environment, say, to recompile after an update, you can
just go through the steps 3 and 4 again. If you got out of the development shell
make sure to remove the `build` folder before running again steps 4, as some
dependencies might have changed since your last build!
