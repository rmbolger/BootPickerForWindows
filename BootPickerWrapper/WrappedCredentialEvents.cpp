// WrappedCredentialEvents is our implementation of ICredentialProviderCredentialEvents (ICPCE).
// Most credential provider authors will not need to implement this interface,
// but a credential provider that wraps another (as this sample does) must.
// The wrapped credential will pass its "this" pointer into any calls to ICPCE,
// but LogonUI will not recognize the wrapped "this" pointer as a valid credential.
// Our implementation translates from the wrapped "this" pointer to the wrapper "this".

#include <unknwn.h>

#include "WrappedCredentialEvents.h"

HRESULT WrappedCredentialEvents::SetFieldState(__in ICredentialProviderCredential* pcpc, __in DWORD dwFieldID, __in CREDENTIAL_PROVIDER_FIELD_STATE cpfs)
{
    UNREFERENCED_PARAMETER(pcpc);

    HRESULT hr = E_FAIL;

    if (_pWrapperCredential && _pEvents)
    {
        hr = _pEvents->SetFieldState(_pWrapperCredential, dwFieldID, cpfs);
    }

    return hr;
}

HRESULT WrappedCredentialEvents::SetFieldInteractiveState(__in ICredentialProviderCredential* pcpc, __in DWORD dwFieldID, __in CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE cpfis)
{
    UNREFERENCED_PARAMETER(pcpc);

    HRESULT hr = E_FAIL;

    if (_pWrapperCredential && _pEvents)
    {
        hr = _pEvents->SetFieldInteractiveState(_pWrapperCredential, dwFieldID, cpfis);
    }

    return hr;
}

HRESULT WrappedCredentialEvents::SetFieldString(__in ICredentialProviderCredential* pcpc, __in DWORD dwFieldID, __in PCWSTR psz)
{
    UNREFERENCED_PARAMETER(pcpc);

    HRESULT hr = E_FAIL;

    if (_pWrapperCredential && _pEvents)
    {
        hr = _pEvents->SetFieldString(_pWrapperCredential, dwFieldID, psz);
    }

    return hr;
}

HRESULT WrappedCredentialEvents::SetFieldBitmap(__in ICredentialProviderCredential* pcpc, __in DWORD dwFieldID, __in HBITMAP hbmp)
{
    UNREFERENCED_PARAMETER(pcpc);

    HRESULT hr = E_FAIL;

    if (_pWrapperCredential && _pEvents)
    {
        hr = _pEvents->SetFieldBitmap(_pWrapperCredential, dwFieldID, hbmp);
    }

    return hr;
}

HRESULT WrappedCredentialEvents::SetFieldCheckbox(__in ICredentialProviderCredential* pcpc, __in DWORD dwFieldID, __in BOOL bChecked, __in PCWSTR pszLabel)
{
    UNREFERENCED_PARAMETER(pcpc);

    HRESULT hr = E_FAIL;

    if (_pWrapperCredential && _pEvents)
    {
        hr = _pEvents->SetFieldCheckbox(_pWrapperCredential, dwFieldID, bChecked, pszLabel);
    }

    return hr;
}

HRESULT WrappedCredentialEvents::SetFieldComboBoxSelectedItem(__in ICredentialProviderCredential* pcpc, __in DWORD dwFieldID, __in DWORD dwSelectedItem)
{
    UNREFERENCED_PARAMETER(pcpc);

    HRESULT hr = E_FAIL;

    if (_pWrapperCredential && _pEvents)
    {
        hr = _pEvents->SetFieldComboBoxSelectedItem(_pWrapperCredential, dwFieldID, dwSelectedItem);
    }

    return hr;
}

HRESULT WrappedCredentialEvents::DeleteFieldComboBoxItem(__in ICredentialProviderCredential* pcpc, __in DWORD dwFieldID, __in DWORD dwItem)
{
    UNREFERENCED_PARAMETER(pcpc);

    HRESULT hr = E_FAIL;

    if (_pWrapperCredential && _pEvents)
    {
        hr = _pEvents->DeleteFieldComboBoxItem(_pWrapperCredential, dwFieldID, dwItem);
    }

    return hr;
}

HRESULT WrappedCredentialEvents::AppendFieldComboBoxItem(__in ICredentialProviderCredential* pcpc, __in DWORD dwFieldID, __in PCWSTR pszItem)
{
    UNREFERENCED_PARAMETER(pcpc);

    HRESULT hr = E_FAIL;

    if (_pWrapperCredential && _pEvents)
    {
        hr = _pEvents->AppendFieldComboBoxItem(_pWrapperCredential, dwFieldID, pszItem);
    }

    return hr;
}

HRESULT WrappedCredentialEvents::SetFieldSubmitButton(__in ICredentialProviderCredential* pcpc, __in DWORD dwFieldID, __in DWORD dwAdjacentTo)
{
    UNREFERENCED_PARAMETER(pcpc);

    HRESULT hr = E_FAIL;

    if (_pWrapperCredential && _pEvents)
    {
        hr = _pEvents->SetFieldSubmitButton(_pWrapperCredential, dwFieldID, dwAdjacentTo);
    }

    return hr;
}

HRESULT WrappedCredentialEvents::OnCreatingWindow(__out HWND* phwndOwner)
{
    HRESULT hr = E_FAIL;

    if (_pWrapperCredential && _pEvents)
    {
        hr = _pEvents->OnCreatingWindow(phwndOwner);
    }

    return hr;
}

WrappedCredentialEvents::WrappedCredentialEvents() :
    _cRef(1), _pWrapperCredential(NULL), _pEvents(NULL)
{}

// 
// Save a copy of LogonUI's ICredentialProviderCredentialEvents pointer for doing callbacks
// and the "this" pointer of the wrapper credential to specify events as coming from.
//
// Pointers are saved as weak references (ie, without a reference count) to avoid circular 
// references.  (For instance, The wrapper credential has a reference on the wrapped credential
// and the wrapped credential should take a reference on this object.  If we had a reference
// on the wrapper credential, there would be a cycle.)  The wrapper credential must manage
// the lifetime of our weak references through calls to Initialize and Uninitialize to
// prevent our weak references from becoming invalid.
//
void WrappedCredentialEvents::Initialize(__in ICredentialProviderCredential* pWrapperCredential, __in ICredentialProviderCredentialEvents* pEvents)
{
    _pWrapperCredential = pWrapperCredential;
    _pEvents = pEvents;
}

//
// Erase our weak references on the wrapper credential and LogonUI's
// ICredentialProviderCredentialEvents pointer.
//
void WrappedCredentialEvents::Uninitialize()
{
    _pWrapperCredential = NULL;
    _pEvents = NULL;
}
