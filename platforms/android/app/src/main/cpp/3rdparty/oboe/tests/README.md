# Oboe Unit Tests

This directory contains the Oboe unit tests. They are run using the bash script `run_tests.sh`. 

The basic operation is:

1. Connect an Android device or start the Android emulator
2. Open a terminal window and execute `run_tests.sh`

## Prerequisites/caveats

1. Java JDK installed.
2. On Mac, you must agree to the XCode license. The script will prompt you.
3. You must have compiled and executed one of the Oboe examples or OboeTester. That ensures that the NDK and cmake is installed.
4. You must define `ANDROID_NDK` as an environment variable and make sure `cmake` is on your path.

To test this on Mac or Linux enter:

    echo $ANDROID_HOME
    echo $ANDROID_NDK
    cmake --version

They may already be set. If so then skip to "Running the Tests" below.

If not, then this may work on Mac OS:

    export ANDROID_HOME=$HOME/Library/Android/sdk
    
or this may work on Linux:

    export ANDROID_HOME=$HOME/Android/Sdk
    
Tadb rooto determine the latest installed version of the NDK. Enter:
    
    ls $ANDROID_HOME/ndk
    
Make note of the folder name. Mine was "21.3.6528147" so I entered:

    export ANDROID_NDK=$ANDROID_HOME/ndk/21.3.6528147/

If you need to add `cmake` to your path then you can find it by entering:

    ls $ANDROID_HOME/cmake
    
Make note of the folder name. Mine was "3.10.2.4988404" so I entered:
    
    export PATH=$PATH:$ANDROID_HOME/cmake/3.10.2.4988404/bin
    cmake --version
    
## Running the Tests

To run the tests, enter:

    cd tests
    ./run_tests.sh
    
When the tests finish, you may need to enter \<control-c\> to exit the script.

If you get this error:

    com.android.builder.testing.api.DeviceException: com.android.ddmlib.InstallException:
        INSTALL_FAILED_UPDATE_INCOMPATIBLE: Package com.google.oboe.tests.unittestrunner
        signatures do not match previously installed version; ignoring!

then uninstall the app "UnitTestRunner" from the Android device. Or try:

    adb root
    adb remount -R

See `run_tests.sh` for more documentation
