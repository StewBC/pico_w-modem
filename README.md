# WiFi modem for Raspberry Pi Pico W

This repository contains the code to make a WiFi modem out of a Raspberry Pi Pico W.  I started with this code for the ESP8266 https://github.com/ssshake/vintage-computer-wifi-modem, which itself was based on https://github.com/RolandJuno/esp8266_modem which again was based on https://github.com/jsalin/esp8266_modem.  From there I added and removed features.  
  
I am using this with my Apple II and can connect it to either an Apple II Super Serial Card via a MAX3232 RS232 Serial Port to TTL Converter Module, or I use it on a card that plugs directly into the Apple II Bus.  The latter was made possible by the work from this project: https://github.com/oliverschmidt/a2pico.  I used the demo code almost as-is for the Pico W working with the Apple II bus.  
  
The project was also built in consultation with Oliver Schmidt.  He conceived of the idea and pointed me at some of the libraries to use and was very generous with his time as I put this together.  The card that plugs into the Apple II was made by Glenn Jones.  He had to make me a second one because the first one I destroyed within about a minute of opening the box.  I greatly appreciate his patience with me.  
  
## Releases  
  
In the releases are two .uf2 files.  The one called uart.uf2 is for use with the RS232 TTL module.  The one called bus.uf2 is specific to a plug-in card for the Apple II.

## Issues

Saving to flash (at&w) does work, but in the UART version the Pico W will need to be retsarted.

## Status

I have to move on to something else, so for now, I am not going to develop this any further.
  
## Pico W ready libraries

The libraries in this repo have been prepared or made to work with the Pico W.  They will be fetched from Github, and potentially have patches applied to them so they work with the Pico W.  
  
At the top level is a CMakeLists.txt which requires cmake version 3.17 at least.  If a lesser version is to be used change the version in the top level CMakeList.txt.  The following two lines may also have to be deleted from CMakeModules/FindProjectUtilities.cmake (although I believe this may not be necessary but mentioned for completeness).  
  
```markup
        GIT_SUBMODULES          ""  
        GIT_SUBMODULES_RECURSE  FALSE  
```
  
### Library contents  
  
At the top level, this library has the following:  
  
| Folder | Description |
| ------ | ----------- |
| .vscode/ | Contains files that integrate with Visual Studio Code. |
|  | Allows compiling with arm-none-eabi-\* and launching in GDB |
| CMakeLists.txt | Main CMake file for the project |
| CMakeModules/ | cmake helper functions and patch script |
| config/ | Header files that configure FreeRTOS, lwip, etc. |
| firmware/ | SSC firmware disassembly |
| modem/ | WiFi modem source code |
| patches/ | Patch diff files to be applied to sources - see below |
  
In addition, the pico-sdk, libsmb2, WolfSSL, WolfSSH and FreeRTOS-Kernel will be downloaded from Github (into build/\_deps).  libsmb2 did not have a tag for the version I made work with the Pico W so there's some risk there that a newer version could break compatibility.  The other libraries are fetched by tag and there may be a reason to update them to newer versions.  See Building below for more info.
  
## Building the code
  
`I was unable to build this on any system other than Linux.  I use WSL2 under Windows.`  
  
With a properly configured VS Code, the preferred method now is to simply start code in the folder where this repository was cloned into.  Manually, this can also be done by running the following line in a shell, inside the folder where this repo was cloned (where the top level CMakeLists.txt file resides):  
`cmake -G "Unix Makefiles" -B build`  
This assumes the cross-compile host is a Linux box, and that the ARM-GCC tools (arm-none-eabi-\*) have been installed.  For completeness, I had to create some sym-links myself as not all needed components were "installed" in the path.  I have the following:
```/usr/bin/arm-none-eabi-ar
/usr/bin/arm-none-eabi-gdb
/usr/bin/arm-none-eabi-objdump
/usr/bin/arm-none-eabi-g++
/usr/bin/arm-none-eabi-nm
/usr/bin/arm-none-eabi-size
/usr/bin/arm-none-eabi-gcc
/usr/bin/arm-none-eabi-objcopy
```  
  
The -G "Unix Makefiles" can of course be replaced by Ninja or something else you desire, it is just a suggestion for Linux.  
  
The cmake command will download pico-sdk, libsmb2, WolfSSL, WolfSSH and the FreeRTOS-Kernel from Github.  Using version 3.17 or later will download what's needed for the pico-sdk.  Using an earlier version will recursively init and download all submodules.  That amounts to more than 4GB, whereas V3.17 or later will download 87M of pico-sdk.  Look for GetGithubCode in the root level CMakeLists.txt - the TAG for the library version is part of that function call.  
  
To switch between building the UART and PIO versions look in modem/CMakeLists.txt for the line:  
`set(USING_UART ON)`  
ON builds the UART version and OFF builds the PIO version.  
  
### Patches applied to libraries
  
Patches are documented in the README.txt, inside the patches folder.  In brief - Wolfssl for Pico W is compiled using a provided set of Makefiles.  These are in wolfssl-5.5.3/IDE/GCC-ARM and Makefile.common needs to be adjusted for the Pico W.  Wolfssh has a Makefile added, to ide/GCC-ARM, which builds the Pico W version.  libsmb2`s CmakeLists.txt is patched for Pico SDK 1.5.0 which requires a different set of include folders.  These changes were applied using the patch command, but now the files are just wholesale replaced by new versions from the patches folder since I had reliability problems using patch.  
  
These patches are automatically applied the first time cmake is used to build.  
  
### Notes about the code  
  
modem/main.cpp contains the main() entry point for running the WiFi modem, but the WiFi modem code is in modem/Modem.cpp.  All other files are support files to Modem.cpp, with the exception of:  
  
| File | Description |
| ---- | ----------- |
| CoreUART.cpp | Runs on Core 1 and communicates with the Pico UART |
| bus.pip | Contains pio code to talk to the Apple II bus |
| CoreBUS.cpp | Contains Core 1 code to talk to the Apple II bus via PIO or to Core 0 |
| incbin.s | Contains code to load the firmware for the card into a variable named firmware |

## Using the WiFi Modem  
  
Modem MGR for the Apple II does not rely on IRQs and is the recommended package to use for the PIO version.  (I have only found versions for the Apple //e).  ProTERM will work well with an SSC and the UART version.
  
Issue AT? to see a help message for WiFi modem commands.  Some commands may not yet have been implemented.  To use the modem for a telnet session, this would be the correct set of commands (case does not matter to the commands):  
  
| Command | What it does |
| ------- | ------------ |
| at$ssid\<wifi ssid> | Set the SSID for the WiFi network to connect to |
| at$pass\<passwd> | Set the password to use for connecting to the WiFi network |
| atc1 | Connect to the WiFi network |
| atnet1 | Turn on Telnet mode |
| atdt\<host[:port]> | Connect to host on port.  Port will default to 23 |
| +++ | Go back to command mode from a connection |
| ath | Hang up a call/connection |
| ato | Return from command mode to a connection rather than hanging up |
  
Try using bbs.retrocampus.com:23 as an example (atds1 with default speed dialer).  
  
The settings can be saved to Flash memory.  It is thus possible to save the SSID and password and simply issue atc1 after boot to get a WiFi connection.  SSH user name and password can also be saved so using atdssh also works.  
  
The vsdrive operation does work but is clunky.  The way to do the vsdrive though is with:  
`atvs1smb://host/path/to/fileatvsoatvs1smb`  
atvs1smb can also be atvs2smb for drive 2.  After atvso (AT virtual serial online), quit Modem MGR and run VSDRIVE on the Apple II.  In Bitsy Bye, pressing 1 should switch between drive 1 and 2 of the files mapped as smb://host/path/to/file.  The following basic program will "eject" a disk from the drive, and allow terminal access to the Pico W to work again: POKE 49288+(s*16), 197: POKE 49288+(s*16), 128, where s is the slot the bus card or SSC is installed in. (Right now this has to be done twice for it to work.)
  
## Using with a Raspberry Pi 4 over the network.  
  
Compile OpenOCD in a raspberry Pi 4 and run in a terminal.  Edit the following line in the root of this repo`s .vscode/launch.json to contain the name of the raspberry pi running OpenOCD:"gdbTarget": "your-openocd-server-name-here:3333",  
  
Also update this line to read:"svdFile": "\<path to >un3/build/\_deps/pico\_sdk-src/src/rp2040/hardware\_regs/rp2040.svd",Replace \<path to> with the fully qualified path where the repo was cloned (From root (/) so probably starting with /home).  When cmake is run, it will show the paths where github libraries are placed - for me it is in build/\_deps/ but adjust the path if it is not the same.  
  
In the folder where openocd will be run, create a file named openocd.cfg and put these lines into that file:  
  
```
bindto 0.0.0.0  
source [find interface/raspberrypi-swd.cfg]  
source [find target/rp2040.cfg]  
```
  
Now, when you choose to debug (if using Visual Studio Code use the Cortex-Debug extension by marus25) with openocd already running, code will be downloaded to the Pico W and printf's will display in the console where openocd was started.  
