Scripts for the Continuous Integration of the PNG Reference Library
===================================================================

Copyright Notice
----------------

Copyright (c) 2019-2026 Cosmin Truta.

Use, modification and distribution are subject to the MIT License.
Please see the accompanying file `LICENSE_MIT.txt` or visit
<https://opensource.org/license/mit>

File List
---------

    LICENSE_MIT.txt         ==>  The License file
    README.md               ==>  This file
    ci_lint.sh              ==>  Lint the source code
    ci_shellify.sh          ==>  Convert select definitions to shell syntax
    ci_verify_cmake.sh      ==>  Verify the build driven by CMakeLists.txt
    ci_verify_configure.sh  ==>  Verify the build driven by configure
    ci_verify_makefiles.sh  ==>  Verify the build driven by scripts/makefile.*
    ci_verify_version.sh    ==>  Verify the consistency of version definitions
    lib/ci.lib.sh           ==>  Shell utilities for the main ci_*.sh scripts
    targets/*/ci_env.*.sh   ==>  Shell environments for cross-platform testing
