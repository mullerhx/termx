#pragma once

#include <wx/wx.h>

// Shown before the main window, to unlock (or set up) the encrypted
// connections vault. Doesn't itself know whether the entered password is
// correct — that's discovered later, when ConnectionStore actually tries
// to decrypt the file with it.
class VaultUnlockDialog : public wxDialog
{
public:
    // vaultExists selects the prompt wording: unlocking an existing vault
    // vs. choosing a password for a brand new one.
    VaultUnlockDialog(wxWindow* parent, bool vaultExists);

    wxString GetPassword() const;

private:
    void OnOk(wxCommandEvent& event);

    wxTextCtrl* m_password = nullptr;
    bool m_vaultExists = false;
};
