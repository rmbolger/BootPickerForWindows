Overview
---------------------------------------------------------------------
This code is based largely on the SampleWrapExistingCredentialProvider code in the 7.1 version of the Windows Platform SDK.  It implements a simple credential provider that wraps the built-in password provider and adds one extra field.  It's a  command link labeled "Reboot to Mac OS X".  It also replaces the tile icon with a Windows logo and if the deselected tile text is "Other User", changes it to "Login to Windows" which is usually the case on domain joined machines only.

When a user clicks the command link, the provider will attempt to locate a copy of BootCamp.exe in %ProgramFiles%\Boot Camp and execute it with the -StartupDisk argument to set the default boot volume back to Mac OS X.  If it succeeds, it will then reboot the host. If it fails, nothing happens and there will be a .log file that matches the dll name in the folder where it's installed.

The default icon is embedded in the compiled dll. You can use an alternative icon by placing it in the same folder as the dll with the same filename except for the extension which should be .bmp.

Please note that encapsulation (or "wrapping") should be used sparingly.  It is not a one size fits all replacement for the GINA chaining behavior.  Unlike GINA chaining, the behavior you add only applies if the user clicks on your credential tile and does not apply if they click on another credential tile.  Encapsulation is only done explicitly and should only be done when you know exactly what the behavior of the wrapped credprov is.  It should be used when you want to extend the credential information that the wrapped credprov is getting.  If you merely want to do something extra with the credentials gathered by another credprov, then a network provider is likely more suited to your needs than a credential provider.


Compatibility
---------------------------------------------------------------------
This provider is considered a "v1 Credential Provider" that is designed for use with Windows Vista and Windows 7.  It will not work in any OSes prior to Vista and it may work in a degraded state for Windows 8, but has not been tested. For more information on Windows 8, please read the Microsoft document titled "Credential Provider Framework Changes in Windows 8" which you can find here:
http://go.microsoft.com/fwlink/?linkid=253508

The compiled binaries are also platform specific. That means you need to compile and use the Win32 version on 32-bit Windows and the x64 version of 64-bit Windows.


How to use this provider
---------------------------------------------------------------------
Once you have built the project, copy the platform specific BootPickerWrapper.dll to the installation directory, and import the Utilities\Register.reg. If your installation directory is not in the %PATH%, you will have to modify the value in the registry file that points to the dll to include the full path to the file. The updated tile should appear the next time a logon is invoked (such as when logging out or switching users).

It does not disable the existing default PasswordProvider.  Until you explicitly disable that, it will still show up as an available tile on domain joined machines.  The easiest way to disable it is by setting the following registry value:
HKLM\Software\Microsoft\Windows\CurrentVersion\Authentication\CredentialProviders\{6f45dc1e-5384-457a-bc13-2cd81b0d28ed}
Disabled = 1 (REG_DWORD)


Important parts of the code
---------------------------------------------------------------------
Many of the files are basically unchanged from the sample code.  Here are the files that contain the bulk of the changes:

common.h - sets up what a tile looks like and how each of the UI controls will be displayed.
Credential.h/Credential.cpp - implements ICredentialProviderCredential, which describes one tile and holds the code relating to calling BootCamp.exe and rebooting.
Provider.h/Provider.cpp - implements ICredentialProvider, which is the main interface used by LogonUI to talk to a credential provider.
