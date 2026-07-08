# Minimal Oboe

## Overview

This app is a very simple demonstration of turning on audio from buttons.
It uses a low-latency Oboe stream.

## Implementation

The app is written using Kotlin and Jetpack Compose.

The app state is maintained by subclassing DefaultLifecycleObserver.
Oboe is called through an external native function.

This app uses shared_ptr for passing callbacks to Oboe.
When the stream is disconnected, it starts a new stream.

## Screenshots
![minimaloboe-screenshot](minimaloboe-screenshot.png)
