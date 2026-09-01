#pragma once

#include <wx/wx.h>
#include <wx/clrpicker.h>

// Prompts for a connection group's name and an optional tab color. Pass an
// existing name/color to pre-fill for editing; pass an empty name and
// invalid color to start from defaults for a brand new group.
class GroupDialog : public wxDialog
{
public:
    GroupDialog(wxWindow* parent, const wxString& title, const wxString& initialName,
               const wxColour& initialColor);

    wxString GetGroupName() const;

    // Invalid (!IsOk()) means "no custom color" — use the default look.
    wxColour GetGroupColor() const;

private:
    void OnOk(wxCommandEvent& event);
    void OnUseColorToggled(wxCommandEvent& event);

    wxTextCtrl* m_name = nullptr;
    wxCheckBox* m_useColor = nullptr;
    wxColourPickerCtrl* m_color = nullptr;
};
