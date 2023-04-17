The following files are created, or overwrite, files in the downloaded dependencies.

libsmb2-CMakeLists.txt
    - Pick SDK 1.5.0 moved some files around and this requires a different set of include folders

wolfssl library:
wolfssl-Makefile.common replaces wolfssl-5.5.2/IDE/GCC-ARM/Makefile.common
    - This version is set up correctly for the Pico W
    - This version prepends ./ to SRC paths starting with ../ -- This let GDB find the source files when debugging

wolfssh library:
wolfssh-Makefile is INSTALLED in wolfssl-5.5.2/ide/GCC-ARM/
    - The GCC-ARM folder is created first
    - This makefile builds wolfSSH for the Pico W

Note:
    1. Both the wolf libraries have DBGFLAGS enabled.
