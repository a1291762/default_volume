Default Volume

This application creates a tray icon, but has no other user interface.
Double-click on the tray icon to close the app.

Edit default_volume.ini to change the default percentage from 40. If a default
of 40 is acceptable, default_volume.ini is not required.

This app will monitor for changes to the registry, and will attempt to set the
volume for new applications. Note that previously-launched applications will
not be modified, only applications that the system does not know about.

Windows maintains a list of applications that have been launched (and their
volume level, if set) at the registry key:

HKEY_CURRENT_USER\Software\Microsoft\Internet Explorer\LowRegistry\Audio\PolicyConfig\PropertyStore

It is possible to remove entries for applications in order to have a default
volume applied, although editing the registry is dangerous and should be done
with care.


How to build

The code here should be fairly standard C code for Windows. I built it using
MSYS2 on a Windows 11 system.

1. Install MSYS2

2. Start an MSYS2 shell and install the ucrt compiler, plus make

    pacman -S mingw-w64-ucrt-x86_64-gcc
    pacman -S make

3. Use the MSYS2 UCRT64 shell, or if using another terminal, add these 2
directories to your PATH

    set PATH=c:\msys64\ucrt64\bin;c:\msys64\usr\bin;%PATH%

4. Build the program using make

    make
