#ifndef WIN32_NO_STATUS
#include <ntstatus.h>
#define WIN32_NO_STATUS
#endif
#include <unknwn.h>
#include "Credential.h"
#include "guid.h"
#include <Windows.h>
#include <ShlObj.h>
#pragma warning(disable:4995)
#include <string>
#include <fstream>
#include <strsafe.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")

// Credential ////////////////////////////////////////////////////////

Credential::Credential():
    _cRef(1),
    _pCredProvCredentialEvents(NULL)
{
    DllAddRef();

    ZeroMemory(_rgCredProvFieldDescriptors, sizeof(_rgCredProvFieldDescriptors));
    ZeroMemory(_rgFieldStatePairs, sizeof(_rgFieldStatePairs));
    ZeroMemory(_rgFieldStrings, sizeof(_rgFieldStrings));

	// build the path to the log file based on this dll's name
	wchar_t* pwszDllPath = new wchar_t[260];
	GetModuleFileName(HINST_THISDLL, pwszDllPath, 260);
	std::wstring logPath(pwszDllPath);
	delete[]pwszDllPath;
	logPath.replace(logPath.end()-3,logPath.end(),L"log");

	debug = std::wofstream(logPath);
}

Credential::~Credential()
{
	if (debug.is_open()) debug.close();

    for (int i = 0; i < ARRAYSIZE(_rgFieldStrings); i++)
    {
        CoTaskMemFree(_rgFieldStrings[i]);
        CoTaskMemFree(_rgCredProvFieldDescriptors[i].pszLabel);
    }

    DllRelease();
}


// Initializes one credential with the field information passed in.
HRESULT Credential::Initialize(
    __in CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
    __in const CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR* rgcpfd,
    __in const FIELD_STATE_PAIR* rgfsp
    )
{
    HRESULT hr = S_OK;

    _cpus = cpus;

    // Copy the field descriptors for each field. This is useful if you want to vary the field
    // descriptors based on what Usage scenario the credential was created for.
    for (DWORD i = 0; SUCCEEDED(hr) && i < ARRAYSIZE(_rgCredProvFieldDescriptors); i++)
    {
        _rgFieldStatePairs[i] = rgfsp[i];
        hr = FieldDescriptorCopy(rgcpfd[i], &_rgCredProvFieldDescriptors[i]);
    }

    // Initialize the String value of all the fields.
    if (SUCCEEDED(hr))
    {
        hr = SHStrDupW(L"Reboot to Mac OS X", &_rgFieldStrings[SFI_LARGE_TEXT]);
    }
    if (SUCCEEDED(hr))
    {
        hr = SHStrDupW(L"Reboot to Mac OS X", &_rgFieldStrings[SFI_COMMAND_LINK]);
    }

    return S_OK;
}

// LogonUI calls this in order to give us a callback in case we need to notify it of anything.
HRESULT Credential::Advise(
    __in ICredentialProviderCredentialEvents* pcpce
    )
{
    if (_pCredProvCredentialEvents != NULL)
    {
        _pCredProvCredentialEvents->Release();
    }
    _pCredProvCredentialEvents = pcpce;
    _pCredProvCredentialEvents->AddRef();
    return S_OK;
}

// LogonUI calls this to tell us to release the callback.
HRESULT Credential::UnAdvise()
{
    if (_pCredProvCredentialEvents)
    {
        _pCredProvCredentialEvents->Release();
    }
    _pCredProvCredentialEvents = NULL;
    return S_OK;
}

// LogonUI calls this function when our tile is selected (zoomed)
// If you simply want fields to show/hide based on the selected state,
// there's no need to do anything here - you can set that up in the
// field definitions. But if you want to do something
// more complicated, like change the contents of a field when the tile is
// selected, you would do it here.
HRESULT Credential::SetSelected(__out BOOL* pbAutoLogon)
{
    *pbAutoLogon = FALSE;
	if (SetMacDefaultBoot())
	{
		Reboot();
	}
    return S_OK;
}

// Similarly to SetSelected, LogonUI calls this when your tile was selected
// and now no longer is. The most common thing to do here is to clear out
// the password field.
HRESULT Credential::SetDeselected()
{
    HRESULT hr = S_OK;
    return hr;
}

// Get info for a particular field of a tile. Called by logonUI to get information
// to display the tile.
HRESULT Credential::GetFieldState(
    __in DWORD dwFieldID,
    __out CREDENTIAL_PROVIDER_FIELD_STATE* pcpfs,
    __out CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE* pcpfis
    )
{
    HRESULT hr;

    // Validate our parameters.
    if ((dwFieldID < ARRAYSIZE(_rgFieldStatePairs)) && pcpfs && pcpfis)
    {
        *pcpfs = _rgFieldStatePairs[dwFieldID].cpfs;
        *pcpfis = _rgFieldStatePairs[dwFieldID].cpfis;
        hr = S_OK;
    }
    else
    {
        hr = E_INVALIDARG;
    }
    return hr;
}

// Sets ppwsz to the string value of the field at the index dwFieldID
HRESULT Credential::GetStringValue(
    __in DWORD dwFieldID,
    __deref_out PWSTR* ppwsz
    )
{
    HRESULT hr;

    // Check to make sure dwFieldID is a legitimate index
    if (dwFieldID < ARRAYSIZE(_rgCredProvFieldDescriptors) && ppwsz)
    {
        // Make a copy of the string and return that. The caller
        // is responsible for freeing it.
        hr = SHStrDupW(_rgFieldStrings[dwFieldID], ppwsz);
    }
    else
    {
        hr = E_INVALIDARG;
    }

    return hr;
}

// Get the image to show in the user tile
HRESULT Credential::GetBitmapValue(
    __in DWORD dwFieldID,
    __out HBITMAP* phbmp
    )
{
    HRESULT hr;
    if ((SFI_TILEIMAGE == dwFieldID) && phbmp)
    {
		// Build the path to the bmp in this dll's folder
		wchar_t* pwszDllPath = new wchar_t[260];
		GetModuleFileName(HINST_THISDLL, pwszDllPath, 260);
		std::wstring bmpPath(pwszDllPath);
		delete[]pwszDllPath;
		bmpPath.replace(bmpPath.end()-3,bmpPath.end(),L"bmp");

		// Look for the filesystem bitmap first
		HBITMAP hbmp = (HBITMAP)LoadImage( NULL, bmpPath.c_str(), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE | LR_LOADFROMFILE );
        if (hbmp != NULL)
        {
			debug << "using filesystem bitmap" << std::endl;
            hr = S_OK;
            *phbmp = hbmp;
        }
        else
        {
			// Use the resource bitmap as a backup
	        hbmp = LoadBitmap(HINST_THISDLL, MAKEINTRESOURCE(IDB_BITMAP1));
			if (hbmp != NULL)
			{
				debug << "using default bitmap" << std::endl;
				hr = S_OK;
				*phbmp = hbmp;
			}
			else
			{
	            hr = HRESULT_FROM_WIN32(GetLastError());
			}
        }
    }
    else
    {
        hr = E_INVALIDARG;
    }

    return hr;
}

// Sets pdwAdjacentTo to the index of the field the submit button should be
// adjacent to. We recommend that the submit button is placed next to the last
// field which the user is required to enter information in. Optional fields
// should be below the submit button.
HRESULT Credential::GetSubmitButtonValue(
    __in DWORD dwFieldID,
    __out DWORD* pdwAdjacentTo
    )
{
	UNREFERENCED_PARAMETER(dwFieldID);
	UNREFERENCED_PARAMETER(pdwAdjacentTo);
	return E_NOTIMPL;
}

// Sets the value of a field which can accept a string as a value.
// This is called on each keystroke when a user types into an edit field
HRESULT Credential::SetStringValue(
    __in DWORD dwFieldID,
    __in PCWSTR pwz
    )
{
    HRESULT hr;

    // Validate parameters.
    if (dwFieldID < ARRAYSIZE(_rgCredProvFieldDescriptors) &&
        (CPFT_EDIT_TEXT == _rgCredProvFieldDescriptors[dwFieldID].cpft ||
        CPFT_PASSWORD_TEXT == _rgCredProvFieldDescriptors[dwFieldID].cpft))
    {
        PWSTR* ppwszStored = &_rgFieldStrings[dwFieldID];
        CoTaskMemFree(*ppwszStored);
        hr = SHStrDupW(pwz, ppwszStored);
    }
    else
    {
        hr = E_INVALIDARG;
    }

    return hr;
}

// Called when the user clicks a command link.  Theoretically, the user
// should never make it here because RebootToMac() is called during
// SetSelected
HRESULT Credential::CommandLinkClicked(__in DWORD dwFieldID)
{
    HRESULT hr;

    // Validate parameter.
    if (dwFieldID < ARRAYSIZE(_rgCredProvFieldDescriptors) &&
        (CPFT_COMMAND_LINK == _rgCredProvFieldDescriptors[dwFieldID].cpft))
    {
        HWND hwndOwner = NULL;

        if (_pCredProvCredentialEvents)
        {
            _pCredProvCredentialEvents->OnCreatingWindow(&hwndOwner);
        }

		// Set Mac as default boot volume and reboot.
		if (SetMacDefaultBoot())
		{
			Reboot();
		}

		hr = S_OK;
    }
    else
    {
        hr = E_INVALIDARG;
    }

    return hr;
}

BOOL Credential::SetMacDefaultBoot()
{
	// get the filesystem location of %ProgramFiles%
	// http://msdn.microsoft.com/en-us/library/bb762188.aspx
	wchar_t* pathProgramFiles = 0;
	SHGetKnownFolderPath(FOLDERID_ProgramFiles, 0, NULL, &pathProgramFiles);

	// build the path to bootcamp.exe
	// http://msdn.microsoft.com/en-us/library/ms647527.aspx
	std::wstring pathBootCamp(pathProgramFiles);
	pathBootCamp += L"\\Boot Camp\\BootCamp.exe";

	// free the memory like the docs tell us
	CoTaskMemFree(static_cast<void*>(pathProgramFiles));

	// Make sure the executable exists
	DWORD attr = GetFileAttributes(pathBootCamp.c_str());
	if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY))
	{
		debug << "BootCamp.exe not found at " << pathBootCamp.c_str() << std::endl;
	    return FALSE;
	}

	// Finish building the complete command line to set the Mac startup volume
	// http://support.apple.com/kb/HT3802
	// "%ProgramFiles%\Boot Camp\BootCamp.exe" -StartupDisk
	std::wstring fullCmd(L"\"" + pathBootCamp + L"\" -StartupDisk");

	debug << fullCmd.c_str() << std::endl;

	// CreateProcess
	//http://msdn.microsoft.com/en-us/library/ms682425.aspx
	STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);
    ZeroMemory( &pi, sizeof(pi) );

	wchar_t * pwszCmd = new wchar_t[fullCmd.length() + 1];
	StringCchCopy(pwszCmd, fullCmd.length() + 1, fullCmd.c_str());

	if (!CreateProcessW(NULL,   // No module name (use command line)
        pwszCmd,        // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        0,              // No creation flags
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory
        &si,            // Pointer to STARTUPINFO structure
        &pi )           // Pointer to PROCESS_INFORMATION structure
    )
    {
        debug << L"CreateProcess failed with error " << GetLastError() << std::endl;
		/* Free memory */
	    delete[]pwszCmd;
		pwszCmd = 0;
		return FALSE;
    }

    // Wait until child process exits.
    WaitForSingleObject( pi.hProcess, 5000 );

	debug << L"BootCamp.exe finished or timeout elapsed" << std::endl;

    // Close process and thread handles.
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );

	/* Free memory */
	delete[]pwszCmd;
	pwszCmd = 0;

	return TRUE;
}

BOOL Credential::Reboot()
{
	HANDLE hToken;
	TOKEN_PRIVILEGES tkp;

	// Get a token for this process.
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
		return( FALSE );

	// Get the LUID for the shutdown privilege.
	LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);

	tkp.PrivilegeCount = 1;  // one privilege to set
	tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	// Get the shutdown privilege for this process.
	AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES)NULL, 0);

	if (GetLastError() != ERROR_SUCCESS)
		return FALSE;

	// Reboot the system and force all applications to close.
	if (!ExitWindowsEx(EWX_REBOOT | EWX_FORCE,
			SHTDN_REASON_MAJOR_OPERATINGSYSTEM | SHTDN_REASON_MINOR_UPGRADE | SHTDN_REASON_FLAG_PLANNED))
		return FALSE;

	//shutdown was successful
	return TRUE;
}

//-------------
// The following methods are for logonUI to get the values of various UI elements and then communicate
// to the credential about what the user did in that field.  However, these methods are not implemented
// because our tile doesn't contain these types of UI elements
HRESULT Credential::GetCheckboxValue(
    __in DWORD dwFieldID,
    __out BOOL* pbChecked,
    __deref_out PWSTR* ppwszLabel
    )
{
    UNREFERENCED_PARAMETER(dwFieldID);
    UNREFERENCED_PARAMETER(pbChecked);
    UNREFERENCED_PARAMETER(ppwszLabel);

    return E_NOTIMPL;
}

HRESULT Credential::GetComboBoxValueCount(
    __in DWORD dwFieldID,
    __out DWORD* pcItems,
    __out_range(<,*pcItems) DWORD* pdwSelectedItem
    )
{
    UNREFERENCED_PARAMETER(dwFieldID);
    UNREFERENCED_PARAMETER(pcItems);
    UNREFERENCED_PARAMETER(pdwSelectedItem);
    return E_NOTIMPL;
}

HRESULT Credential::GetComboBoxValueAt(
    __in DWORD dwFieldID,
    __in DWORD dwItem,
    __deref_out PWSTR* ppwszItem
    )
{
    UNREFERENCED_PARAMETER(dwFieldID);
    UNREFERENCED_PARAMETER(dwItem);
    UNREFERENCED_PARAMETER(ppwszItem);
    return E_NOTIMPL;
}

HRESULT Credential::SetCheckboxValue(
    __in DWORD dwFieldID,
    __in BOOL bChecked
    )
{
    UNREFERENCED_PARAMETER(dwFieldID);
    UNREFERENCED_PARAMETER(bChecked);

    return E_NOTIMPL;
}

HRESULT Credential::SetComboBoxSelectedValue(
    __in DWORD dwFieldId,
    __in DWORD dwSelectedItem
    )
{
    UNREFERENCED_PARAMETER(dwFieldId);
    UNREFERENCED_PARAMETER(dwSelectedItem);
    return E_NOTIMPL;
}
//------ end of methods for controls we don't have in our tile ----//


// Collect the username and password into a serialized credential for the correct usage scenario
// LogonUI then passes these credentials back to the system to log on.
HRESULT Credential::GetSerialization(
    __out CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE* pcpgsr,
    __out CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs,
    __deref_out_opt PWSTR* ppwszOptionalStatusText,
    __in CREDENTIAL_PROVIDER_STATUS_ICON* pcpsiOptionalStatusIcon
    )
{
	UNREFERENCED_PARAMETER(pcpgsr);
	UNREFERENCED_PARAMETER(pcpcs);
    UNREFERENCED_PARAMETER(ppwszOptionalStatusText);
    UNREFERENCED_PARAMETER(pcpsiOptionalStatusIcon);
	return E_NOTIMPL;
}

struct REPORT_RESULT_STATUS_INFO
{
    NTSTATUS ntsStatus;
    NTSTATUS ntsSubstatus;
    PWSTR     pwzMessage;
    CREDENTIAL_PROVIDER_STATUS_ICON cpsi;
};

static const REPORT_RESULT_STATUS_INFO s_rgLogonStatusInfo[] =
{
    { STATUS_LOGON_FAILURE, STATUS_SUCCESS, L"Incorrect password or username.", CPSI_ERROR, },
    { STATUS_ACCOUNT_RESTRICTION, STATUS_ACCOUNT_DISABLED, L"The account is disabled.", CPSI_WARNING },
};

// ReportResult is completely optional.  Its purpose is to allow a credential to customize the string
// and the icon displayed in the case of a logon failure.  For example, we have chosen to
// customize the error shown in the case of bad username/password and in the case of the account
// being disabled.
HRESULT Credential::ReportResult(
    __in NTSTATUS ntsStatus,
    __in NTSTATUS ntsSubstatus,
    __deref_out_opt PWSTR* ppwszOptionalStatusText,
    __out CREDENTIAL_PROVIDER_STATUS_ICON* pcpsiOptionalStatusIcon
    )
{
    *ppwszOptionalStatusText = NULL;
    *pcpsiOptionalStatusIcon = CPSI_NONE;

    DWORD dwStatusInfo = (DWORD)-1;

    // Look for a match on status and substatus.
    for (DWORD i = 0; i < ARRAYSIZE(s_rgLogonStatusInfo); i++)
    {
        if (s_rgLogonStatusInfo[i].ntsStatus == ntsStatus && s_rgLogonStatusInfo[i].ntsSubstatus == ntsSubstatus)
        {
            dwStatusInfo = i;
            break;
        }
    }

    if ((DWORD)-1 != dwStatusInfo)
    {
        if (SUCCEEDED(SHStrDupW(s_rgLogonStatusInfo[dwStatusInfo].pwzMessage, ppwszOptionalStatusText)))
        {
            *pcpsiOptionalStatusIcon = s_rgLogonStatusInfo[dwStatusInfo].cpsi;
        }
    }
    // Since NULL is a valid value for *ppwszOptionalStatusText and *pcpsiOptionalStatusIcon
    // this function can't fail.
    return S_OK;
}
