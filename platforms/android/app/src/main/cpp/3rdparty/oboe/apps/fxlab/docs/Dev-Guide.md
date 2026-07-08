# Development

The effects included are designed to be as portable as possible, as well as making it easy to add additional effects.

## Building

To build the app, simply open the project in android studio and build. The app integrates Oboe via a git submodule. Make sure
when cloning the repository to clone the submodule as well using `git clone --recursive`, or `git submodule update --init --recursive`.

To update the version of Oboe being used, descend into the Oboe repository [(oboe location)](../app/src/main/cpp) 
and update from its remote. Then, call `git submodule update` in this repository. Alternatively `git submodule update --recursive --remote` will pull the latest version of Oboe from remote.

Although the CMake file requires android headers (to use Oboe), the effects themselves can be compiled with any C++17 compliant compiler.

## Architecture

The UI code (Kotlin) calls native code through the JNI bridge to query information about the various effects implemented,
and how to render the effect information in the UI. This means that adding an effect only needs to be done on the 
native side. The JNI bridge passes information regarding implemented effects descriptions to the UI as well as functions
called when the user modifies effects in the UI.

The `DuplexEngine` is responsible for managing and syncing the input and output Oboe streams for rendering audio
with as low latency as possible. The `FunctionList` class contains the a vector of effects that correspond to the list of 
effects (and their parameters) that the user wants to use to process their audio. Effects (and the `FunctionList`) overload
their function operator to take in two numeric iterator types. E.g `<template iter_type> void operator() 
(iter_type begin, iter_type end)`, where the `iter_types` correspond to C++ iterators carrying audio data. To erase the type
of different objects, the `FunctionList` holds objects of types `std::function<void(iter_type, iter_type)>` i.e. functions
which operate on the range between two numeric iterators in place. The `DuplexEngine`simply calls the `FunctionList` on every 
buffer of samples it receives. 

The effects folder contains the classes of various implemented effects. It also contains `Effects.h` where a global tuple of 
all the Effect descriptions implemented lives. The description folder contains the description for all of the effects. 
Each description takes the form of a class with static methods providing information regarding the effect (including name, 
category, parameters, and a factory method).

## Adding Effects
To add an effect, simply add a Description class similar to the existing classes, and add the class to the tuple in `Effects.h`. The description must provide a way to build the effect by either constructing another class corresponding
to an effect, or pointing to a standalone function. Adding new effects is welcome!

## Existing Effects
A instructional implemented effect to examine is the `TremoloEffect.h` (a modulating gain). 
Many of the effects in the delay category
inherit from `DelayLineEffect` which provides a framework to easily implement many delay based effects, as well as
the comb filter effects (FIR, IIR, allpass). The slides have a block diagram displaying the mathematical basis of the effect.
The nonlinear effects (distortion and overdrive) are implemented using a standalone function (from `SingleEffectFunctions.h`.
The gain effect is implemented by a simple lambda in its description class. 
