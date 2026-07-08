[Home](README.md)

# How to Build OboeTester

Note that you can now [download OboeTester from the Play Store](https://play.google.com/store/apps/details?id=com.mobileer.oboetester).

But if you want the latest version, or if you want to debug OboeTester, then you can build it using Android Studio.
Download the top level oboe repository from GitHub.
Then use Android Studio (3.3 or above) to build the app in this "apps/OboeTester".

## Requirements

* Android Studio
* Android device or emulator
* git installed on your computer (optional)
* USB cable to connect your computer and your phone

## Download Oboe

You can either use git to clone Oboe or download a Zip archive.
If you use git then you can quickly update the code or contribute changes.
If you don't use git then just download the Zip archive.

1. Visit https://github.com/google/oboe
2. Click on the green button that says "Clone or Download" and follow the instructions.

## Build and Run OboeTester

1. Launch Android Studio
2. Click Open from the File menu and browse to the "oboe/apps/OboeTester" folder. Select that folder.
3. Wait about a minute for the project to load.
4. Connect an Android phone to your computer using a USB cable.
5. Look at your phone. You may need to give permission to use ADB on your phone.
5. Select "Run App" from the "Run" menu.
6. OboeTester should build and then appear on your Android device.
