#ifndef WIN32_NO_STATUS
#include <ntstatus.h>
#define WIN32_NO_STATUS
#endif
#include <unknwn.h>
#include "Credential.h"
#include "WrappedCredentialEvents.h"
#include "guid.h"
#include <Windows.h>
#include <ShlObj.h>

// Credential ////////////////////////////////////////////////////////

// NOTE: Please read the readme.txt file to understand when it's appropriate to
// wrap an another credential provider and when it's not.  If you have questions
// about whether your scenario is an appropriate use of wrapping another credprov,
// please contact credprov@microsoft.com
Credential::Credential():
    _cRef(1)
{
    DllAddRef();

    ZeroMemory(_rgCredProvFieldDescriptors, sizeof(_rgCredProvFieldDescriptors));
    ZeroMemory(_rgFieldStatePairs, sizeof(_rgFieldStatePairs));
    ZeroMemory(_rgFieldStrings, sizeof(_rgFieldStrings));

    _pWrappedCredential = NULL;
    _pWrappedCredentialEvents = NULL;
    _pCredProvCredentialEvents = NULL;

    _dwWrappedDescriptorCount = 0;
}

Credential::~Credential()
{
    for (int i = 0; i < ARRAYSIZE(_rgFieldStrings); i++)
    {
        CoTaskMemFree(_rgFieldStrings[i]);
        CoTaskMemFree(_rgCredProvFieldDescriptors[i].pszLabel);
    }

    _CleanupEvents();

    if (_pWrappedCredential)
    {
        _pWrappedCredential->Release();
    }

    DllRelease();
}

std::wstring Credential::GetDebugText()
{
	return debug.str();
}

// Initializes one credential with the field information passed in. We also keep track
// of our wrapped credential and how many fields it has.
HRESULT Credential::Initialize(
    __in const CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR* rgcpfd,
    __in const FIELD_STATE_PAIR* rgfsp,
    __in ICredentialProviderCredential *pWrappedCredential,
    __in DWORD dwWrappedDescriptorCount
    )
{
    HRESULT hr = S_OK;

    // Grab the credential we're wrapping for future reference.
    if (_pWrappedCredential != NULL)
    {
        _pWrappedCredential->Release();
    }
    _pWrappedCredential = pWrappedCredential;
    _pWrappedCredential->AddRef();

    // We also need to remember how many fields the inner credential has.
    _dwWrappedDescriptorCount = dwWrappedDescriptorCount;

    // Copy the field descriptors for each field. This is useful if you want to vary the field
    // descriptors based on what Usage scenario the credential was created for.
    for (DWORD i = 0; SUCCEEDED(hr) && i < ARRAYSIZE(_rgCredProvFieldDescriptors); i++)
    {
        _rgFieldStatePairs[i] = rgfsp[i];
        hr = FieldDescriptorCopy(rgcpfd[i], &_rgCredProvFieldDescriptors[i]);
    }

    // Initialize the String value of all of our fields.
    if (SUCCEEDED(hr))
    {
        hr = SHStrDupW(L" ", &_rgFieldStrings[SFI_BLANK_LINE]);
    }
    if (SUCCEEDED(hr))
    {
        hr = SHStrDupW(L"Reboot to Mac OS X", &_rgFieldStrings[SFI_BOOT_MAC_COMMAND]);
    }
    return hr;
}

// LogonUI calls this in order to give us a callback in case we need to notify it of
// anything. We'll also provide it to the wrapped credential.
HRESULT Credential::Advise(
    __in ICredentialProviderCredentialEvents* pcpce
    )
{
    HRESULT hr = S_OK;

    _CleanupEvents();

    // We keep a strong reference on the real ICredentialProviderCredentialEvents
    // to ensure that the weak reference held by the WrappedCredentialEvents is valid.
    _pCredProvCredentialEvents = pcpce;
    _pCredProvCredentialEvents->AddRef();

    _pWrappedCredentialEvents = new WrappedCredentialEvents();

    if (_pWrappedCredentialEvents != NULL)
    {
        _pWrappedCredentialEvents->Initialize(this, pcpce);

        if (_pWrappedCredential != NULL)
        {
            hr = _pWrappedCredential->Advise(_pWrappedCredentialEvents);
        }
    }
    else
    {
        hr = E_OUTOFMEMORY;
    }

    return hr;
}

// LogonUI calls this to tell us to release the callback.
// We'll also provide it to the wrapped credential.
HRESULT Credential::UnAdvise()
{
    HRESULT hr = S_OK;

    if (_pWrappedCredential != NULL)
    {
        _pWrappedCredential->UnAdvise();
    }

    _CleanupEvents();

    return hr;
}

// LogonUI calls this function when our tile is selected (zoomed)
// If you simply want fields to show/hide based on the selected state,
// there's no need to do anything here - you can set that up in the
// field definitions. In fact, we're just going to hand it off to the
// wrapped credential in case it wants to do something.
HRESULT Credential::SetSelected(__out BOOL* pbAutoLogon)
{
    HRESULT hr = E_UNEXPECTED;

    if (_pWrappedCredential != NULL)
    {
        hr = _pWrappedCredential->SetSelected(pbAutoLogon);
    }

    return hr;
}

// Similarly to SetSelected, LogonUI calls this when your tile was selected
// and now no longer is. We'll let the wrapped credential do anything it needs.
HRESULT Credential::SetDeselected()
{
    HRESULT hr = E_UNEXPECTED;

    if (_pWrappedCredential != NULL)
    {
        hr = _pWrappedCredential->SetDeselected();
    }

    return hr;
}

// Get info for a particular field of a tile. Called by logonUI to get information to
// display the tile. We'll check to see if it's for us or the wrapped credential, and then
// handle or route it as appropriate.
HRESULT Credential::GetFieldState(
    __in DWORD dwFieldID,
    __out CREDENTIAL_PROVIDER_FIELD_STATE* pcpfs,
    __out CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE* pcpfis
    )
{
    HRESULT hr = E_UNEXPECTED;

    // Make sure we have a wrapped credential.
    if (_pWrappedCredential != NULL)
    {
        // Validate parameters.
        if ((pcpfs != NULL) && (pcpfis != NULL))
        {
            // If the field is in the wrapped credential, hand it off.
            if (_IsFieldInWrappedCredential(dwFieldID))
            {
                hr = _pWrappedCredential->GetFieldState(dwFieldID, pcpfs, pcpfis);
            }
            // Otherwise, we need to see if it's one of ours.
            else
            {
                FIELD_STATE_PAIR *pfsp = _LookupLocalFieldStatePair(dwFieldID);
                // If the field ID is valid, give it info it needs.
                if (pfsp != NULL)
                {
                    *pcpfs = pfsp->cpfs;
                    *pcpfis = pfsp->cpfis;

                    hr = S_OK;
                }
                else
                {
                    hr = E_INVALIDARG;
                }
            }
        }
        else
        {
            hr = E_INVALIDARG;
        }
    }
    return hr;
}

// Sets ppwsz to the string value of the field at the index dwFieldID. We'll check to see if
// it's for us or the wrapped credential, and then handle or route it as appropriate.
HRESULT Credential::GetStringValue(
    __in DWORD dwFieldID,
    __deref_out PWSTR* ppwsz
    )
{
    HRESULT hr = E_UNEXPECTED;

    // Make sure we have a wrapped credential.
    if (_pWrappedCredential != NULL)
    {
        // If this field belongs to the wrapped credential, hand it off.
        if (_IsFieldInWrappedCredential(dwFieldID))
        {
			// Hijack the normal value for the text field that displays "Other User" and replace
			// it with "Login to Windows"
			// WARNING: The dwFieldID == 0 we're testing against was found by trial and error since we
			// don't have access to the source for the PasswordProvider we're wrapping.  If for some
			// reason it changes in the future, this may break.
			if (dwFieldID == 0)
			{
				// inspect the wrapped value
				wchar_t* pwszWrappedValue = new wchar_t[260];
				hr = _pWrappedCredential->GetStringValue(dwFieldID, &pwszWrappedValue);
				if (hr == S_OK)
				{
					if (_wcsicmp(pwszWrappedValue, L"Other User") == 0)
					{
						debug << L"Found \"Other User\"\n";
						hr = SHStrDupW(L"Login to Windows", ppwsz);
						return hr;
					}
				}
				else debug << L"GetStringValue failed with " << hr << std::endl;
			}

			hr = _pWrappedCredential->GetStringValue(dwFieldID, ppwsz);
        }
        // Otherwise determine if we need to handle it.
        else
        {
            FIELD_STATE_PAIR *pfsp = _LookupLocalFieldStatePair(dwFieldID);
            if (pfsp != NULL)
            {
                hr = SHStrDupW(_rgFieldStrings[dwFieldID - _dwWrappedDescriptorCount], ppwsz);
            }
            else
            {
                hr = E_INVALIDARG;
            }
        }
    }
    return hr;
}

HRESULT Credential::GetComboBoxValueCount(
    __in DWORD dwFieldID,
    __out DWORD* pcItems,
    __out_range(<,*pcItems) DWORD* pdwSelectedItem
    )
{
    HRESULT hr = E_UNEXPECTED;

    if (_pWrappedCredential != NULL)
    {
        hr = _pWrappedCredential->GetComboBoxValueCount(dwFieldID, pcItems, pdwSelectedItem);
    }

    return hr;
}

HRESULT Credential::GetComboBoxValueAt(
    __in DWORD dwFieldID,
    __in DWORD dwItem,
    __deref_out PWSTR* ppwszItem
    )
{
    HRESULT hr = E_UNEXPECTED;

    if (_pWrappedCredential != NULL)
    {
        hr = _pWrappedCredential->GetComboBoxValueAt(dwFieldID, dwItem, ppwszItem);
    }

    return hr;
}

HRESULT Credential::SetComboBoxSelectedValue(
    __in DWORD dwFieldID,
    __in DWORD dwSelectedItem
    )
{
    HRESULT hr = E_UNEXPECTED;

    if (_pWrappedCredential != NULL)
    {
        hr = _pWrappedCredential->SetComboBoxSelectedValue(dwFieldID, dwSelectedItem);
    }

    return hr;
}

//-------------
// The following methods are for logonUI to get the values of various UI elements and
// then communicate to the credential about what the user did in that field. Even though
// we don't offer these field types ourselves, we need to pass along the request to the
// wrapped credential.

// In a normal credential provider wrapper, we'd just pass this request along to the
// wrapped provider.  But for our purposes, we want to replace the icon with our
// Windows icon instead.
HRESULT Credential::GetBitmapValue(
    __in DWORD dwFieldID,
    __out HBITMAP* phbmp
    )
{
	UNREFERENCED_PARAMETER(dwFieldID);

	HRESULT hr;

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

    return hr;
}

HRESULT Credential::GetSubmitButtonValue(
    __in DWORD dwFieldID,
    __out DWORD* pdwAdjacentTo
    )
{
    HRESULT hr = E_UNEXPECTED;

    if (_pWrappedCredential != NULL)
    {
        hr = _pWrappedCredential->GetSubmitButtonValue(dwFieldID, pdwAdjacentTo);
    }

    return hr;
}

HRESULT Credential::SetStringValue(
    __in DWORD dwFieldID,
    __in PCWSTR pwz
    )
{
    HRESULT hr = E_UNEXPECTED;

    if (_pWrappedCredential != NULL)
    {
        hr = _pWrappedCredential->SetStringValue(dwFieldID, pwz);
    }

    return hr;

}

HRESULT Credential::GetCheckboxValue(
    __in DWORD dwFieldID,
    __out BOOL* pbChecked,
    __deref_out PWSTR* ppwszLabel
    )
{
    HRESULT hr = E_UNEXPECTED;

    if (_pWrappedCredential != NULL)
    {
        if (_IsFieldInWrappedCredential(dwFieldID))
        {
            hr = _pWrappedCredential->GetCheckboxValue(dwFieldID, pbChecked, ppwszLabel);
        }
    }

    return hr;
}

HRESULT Credential::SetCheckboxValue(
    __in DWORD dwFieldID,
    __in BOOL bChecked
    )
{
    HRESULT hr = E_UNEXPECTED;

    if (_pWrappedCredential != NULL)
    {
        hr = _pWrappedCredential->SetCheckboxValue(dwFieldID, bChecked);
    }

    return hr;
}

// Called when the user clicks a command link.
HRESULT Credential::CommandLinkClicked(__in DWORD dwFieldID)
{
    HRESULT hr = E_UNEXPECTED;

    if (_pWrappedCredential != NULL)
    {
        // If this field belongs to the wrapped credential, hand it off.
        if (_IsFieldInWrappedCredential(dwFieldID))
        {
	        hr = _pWrappedCredential->CommandLinkClicked(dwFieldID);
        }
        // Otherwise determine if we need to handle it.
        else
        {
			// make sure the normalized dwFieldID is our command link ID
			if (SFI_BOOT_MAC_COMMAND == (dwFieldID - _dwWrappedDescriptorCount))
			{
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
        }
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

//------ end of methods for controls we don't have ourselves ----//


//
// Collect the username and password into a serialized credential for the correct usage scenario
// (logon/unlock is what's demonstrated in this sample).  LogonUI then passes these credentials
// back to the system to log on.
//
HRESULT Credential::GetSerialization(
    __out CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE* pcpgsr,
    __out CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs,
    __deref_out_opt PWSTR* ppwszOptionalStatusText,
    __out CREDENTIAL_PROVIDER_STATUS_ICON* pcpsiOptionalStatusIcon
    )
{
    HRESULT hr = E_UNEXPECTED;

    if (_pWrappedCredential != NULL)
    {
        hr = _pWrappedCredential->GetSerialization(pcpgsr, pcpcs, ppwszOptionalStatusText, pcpsiOptionalStatusIcon);
    }

    return hr;
}

// ReportResult is completely optional. However, we will hand it off to the wrapped
// credential in case they want to handle it.
HRESULT Credential::ReportResult(
    __in NTSTATUS ntsStatus,
    __in NTSTATUS ntsSubstatus,
    __deref_out_opt PWSTR* ppwszOptionalStatusText,
    __out CREDENTIAL_PROVIDER_STATUS_ICON* pcpsiOptionalStatusIcon
    )
{
    HRESULT hr = E_UNEXPECTED;

    if (_pWrappedCredential != NULL)
    {
        hr = _pWrappedCredential->ReportResult(ntsStatus, ntsSubstatus, ppwszOptionalStatusText, pcpsiOptionalStatusIcon);
    }

    return hr;
}

BOOL Credential::_IsFieldInWrappedCredential(
    __in DWORD dwFieldID
    )
{
    return (dwFieldID < _dwWrappedDescriptorCount);
}

FIELD_STATE_PAIR *Credential::_LookupLocalFieldStatePair(
    __in DWORD dwFieldID
    )
{
    // Offset into the ID to account for the wrapped fields.
    dwFieldID -= _dwWrappedDescriptorCount;

    // If the index if valid, give it the info it wants.
    if (dwFieldID < SFI_NUM_FIELDS)
    {
        return &(_rgFieldStatePairs[dwFieldID]);
    }

    return NULL;
}

void Credential::_CleanupEvents()
{
    // Call Uninitialize before releasing our reference on the real
    // ICredentialProviderCredentialEvents to avoid having an
    // invalid reference.
    if (_pWrappedCredentialEvents != NULL)
    {
        _pWrappedCredentialEvents->Uninitialize();
        _pWrappedCredentialEvents->Release();
        _pWrappedCredentialEvents = NULL;
    }

    if (_pCredProvCredentialEvents != NULL)
    {
        _pCredProvCredentialEvents->Release();
        _pCredProvCredentialEvents = NULL;
    }
}
