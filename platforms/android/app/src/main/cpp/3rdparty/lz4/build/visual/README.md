These scripts will generate Visual Studio Solutions for a selected set of supported versions of MS Visual.

For these scripts to work, both `cmake` and the relevant Visual Studio version must be locally installed on the system where the script is run.

If `cmake` is installed into a non-standard directory, or user wants to test a specific version of `cmake`, the target `cmake` directory can be provided via the environment variable `CMAKE_PATH`.
