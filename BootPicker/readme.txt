Overview
---------------------------------------------------------------------
This code is based largely on the SampleAllControlsCredentialProvider code in the 7.1 version of the Windows Platform SDK.  It implements a simple credential provider which displays one tile whose sole purpose is to provide a one-click way to reboot into Mac OS X from the login screen.  The tile has an Apple icon and is labeled "Reboot to Mac OS X".  It can be used by itself or in conjunction with the BootPickerWrapper that makes the Windows login alternative a little more obvious.

When a user clicks the tile, instead of presenting a login dialog the provider will attempt to locate a copy of BootCamp.exe in %ProgramFiles%\Boot Camp and execute it with the -StartupDisk argument to set the default boot volume back to Mac OS X.  If it succeeds, it will then reboot the host.

If it fails, the tile will be selected and the only thing available will be a "Reboot to Mac OS X" command link like the one in BootPickerWrapper.  There will be a .log file that matches the dll name in the folder where it's installed.

The default icon is embedded in the compiled dll. You can use an alternative icon by placing it in the same folder as the dll with the same filename except for the extension which should be .bmp. 


Compatibility
---------------------------------------------------------------------
This provider is considered a "v1 Credential Provider" that is designed for use with Windows Vista and Windows 7.  It will not work in any OSes prior to Vista and it may work in a degraded state for Windows 8, but has not been tested. For more information on Windows 8, please read the Microsoft document titled "Credential Provider Framework Changes in Windows 8" which you can find here:
http://go.microsoft.com/fwlink/?linkid=253508

The compiled binaries are also platform specific. That means you need to compile and use the Win32 version on 32-bit Windows and the x64 version of 64-bit Windows.


How to use this provider
---------------------------------------------------------------------
Once you have built the project, copy the platform specific BootPicker.dll to the installation directory, and import the Utilities\Register.reg. If your installation directory is not in the %PATH%, you will have to modify the value in the registry file that points to the dll to include the full path to the file. The new tile should appear the next time a logon is invoked (such as when logging out or switching users).


Important parts of the code
---------------------------------------------------------------------
Most of the files in this project are basically unchanged from the sample code.  Here are the files that contain the bulk of the changes:

common.h - sets up what a tile looks like and how each of the UI controls will be displayed.
Credential.h/Credential.cpp - implements ICredentialProviderCredential, which describes one tile and holds the code relating to calling BootCamp.exe and rebooting.
Provider.h/Provider.cpp - implements ICredentialProvider, which is the main interface used by LogonUI to talk to a credential provider.
