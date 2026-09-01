#include "GroupDialog.h"

GroupDialog::GroupDialog(wxWindow* parent, const wxString& title, const wxString& initialName,
                         const wxColour& initialColor)
    : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
{
    auto* grid = new wxFlexGridSizer(2, wxSize(8, 8));
    grid->AddGrowableCol(1);

    m_name = new wxTextCtrl(this, wxID_ANY, initialName);
    grid->Add(new wxStaticText(this, wxID_ANY, "Name:"), 0, wxALIGN_CENTRE_VERTICAL);
    grid->Add(m_name, 1, wxEXPAND);

    const bool hasColor = initialColor.IsOk();
    m_color = new wxColourPickerCtrl(this, wxID_ANY, hasColor ? initialColor : *wxBLUE);
    m_color->Enable(hasColor);
    grid->Add(new wxStaticText(this, wxID_ANY, "Tab color:"), 0, wxALIGN_CENTRE_VERTICAL);
    grid->Add(m_color, 1, wxEXPAND);

    auto* topSizer = new wxBoxSizer(wxVERTICAL);
    topSizer->Add(grid, 1, wxEXPAND | wxALL, 12);

    m_useColor = new wxCheckBox(this, wxID_ANY,
        "Color-code terminal tabs opened from this group's connections");
    m_useColor->SetValue(hasColor);
    topSizer->Add(m_useColor, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

    topSizer->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 12);

    SetSizerAndFit(topSizer);

    Bind(wxEVT_BUTTON, &GroupDialog::OnOk, this, wxID_OK);
    m_useColor->Bind(wxEVT_CHECKBOX, &GroupDialog::OnUseColorToggled, this);
}

void GroupDialog::OnUseColorToggled(wxCommandEvent&)
{
    m_color->Enable(m_useColor->GetValue());
}

void GroupDialog::OnOk(wxCommandEvent& event)
{
    if (m_name->GetValue().Trim().IsEmpty())
    {
        wxMessageBox("Please enter a group name.", "New Connection Group", wxOK | wxICON_WARNING,
                     this);
        return;
    }

    event.Skip();
}

wxString GroupDialog::GetGroupName() const
{
    return m_name->GetValue().Trim();
}

wxColour GroupDialog::GetGroupColor() const
{
    return m_useColor->GetValue() ? m_color->GetColour() : wxColour();
}
