#include "PreferencesDialog.h"

#include <wx/fontenum.h>

PreferencesDialog::PreferencesDialog(wxWindow* parent, const TerminalSettings& settings)
    : wxDialog(parent, wxID_ANY, "Terminal Preferences", wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE)
{
    auto* grid = new wxFlexGridSizer(2, wxSize(8, 8));
    grid->AddGrowableCol(1);

    wxArrayString faces;
    {
        wxFontEnumerator enumerator;
        enumerator.EnumerateFacenames(wxFONTENCODING_SYSTEM, true /* fixedWidthOnly */);
        faces = enumerator.GetFacenames();
        faces.Sort();
    }
    if (faces.Index(settings.fontFamily) == wxNOT_FOUND)
        faces.Insert(settings.fontFamily, 0);

    m_fontFamily = new wxComboBox(this, wxID_ANY, settings.fontFamily, wxDefaultPosition,
                                   wxDefaultSize, faces, wxCB_DROPDOWN | wxCB_READONLY);
    grid->Add(new wxStaticText(this, wxID_ANY, "Font family:"), 0, wxALIGN_CENTRE_VERTICAL);
    grid->Add(m_fontFamily, 1, wxEXPAND);

    m_fontSize = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                 wxSP_ARROW_KEYS, 6, 72, settings.fontSize);
    grid->Add(new wxStaticText(this, wxID_ANY, "Font size:"), 0, wxALIGN_CENTRE_VERTICAL);
    grid->Add(m_fontSize, 1, wxEXPAND);

    m_lineSpacing = new wxSpinCtrlDouble(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                         wxDefaultSize, wxSP_ARROW_KEYS, 1.0, 3.0,
                                         settings.lineSpacing, 0.05);
    grid->Add(new wxStaticText(this, wxID_ANY, "Line spacing:"), 0, wxALIGN_CENTRE_VERTICAL);
    grid->Add(m_lineSpacing, 1, wxEXPAND);

    m_foregroundColor = new wxColourPickerCtrl(this, wxID_ANY, settings.foregroundColor);
    grid->Add(new wxStaticText(this, wxID_ANY, "Text color:"), 0, wxALIGN_CENTRE_VERTICAL);
    grid->Add(m_foregroundColor, 1, wxEXPAND);

    m_backgroundColor = new wxColourPickerCtrl(this, wxID_ANY, settings.backgroundColor);
    grid->Add(new wxStaticText(this, wxID_ANY, "Background color:"), 0, wxALIGN_CENTRE_VERTICAL);
    grid->Add(m_backgroundColor, 1, wxEXPAND);

    m_opacity = new wxSpinCtrlDouble(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                      wxDefaultSize, wxSP_ARROW_KEYS, 10.0, 100.0,
                                      settings.opacityPercent, 5.0);
    grid->Add(new wxStaticText(this, wxID_ANY, "Opacity (%):"), 0, wxALIGN_CENTRE_VERTICAL);
    grid->Add(m_opacity, 1, wxEXPAND);

    auto* topSizer = new wxBoxSizer(wxVERTICAL);
    topSizer->Add(grid, 1, wxEXPAND | wxALL, 12);

    m_toolbarShowLabels = new wxCheckBox(this, wxID_ANY, "Show button titles under toolbar icons");
    m_toolbarShowLabels->SetValue(settings.toolbarShowLabels);
    topSizer->Add(m_toolbarShowLabels, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

    m_toolbarLargeIcons = new wxCheckBox(this, wxID_ANY, "Use large toolbar buttons");
    m_toolbarLargeIcons->SetValue(settings.toolbarLargeIcons);
    topSizer->Add(m_toolbarLargeIcons, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

    m_showTerminalScrollbar = new wxCheckBox(this, wxID_ANY, "Show a scrollbar on each terminal");
    m_showTerminalScrollbar->SetValue(settings.showTerminalScrollbar);
    topSizer->Add(m_showTerminalScrollbar, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

    auto* note = new wxStaticText(this, wxID_ANY,
        "Opacity only has a visible effect if your desktop compositor is running.");
    note->SetForegroundColour(*wxLIGHT_GREY);
    topSizer->Add(note, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

    topSizer->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 12);

    SetSizerAndFit(topSizer);
}

TerminalSettings PreferencesDialog::GetSettings() const
{
    TerminalSettings settings;
    settings.fontFamily = m_fontFamily->GetValue();
    settings.fontSize = m_fontSize->GetValue();
    settings.lineSpacing = m_lineSpacing->GetValue();
    settings.foregroundColor = m_foregroundColor->GetColour();
    settings.backgroundColor = m_backgroundColor->GetColour();
    settings.opacityPercent = m_opacity->GetValue();
    settings.toolbarShowLabels = m_toolbarShowLabels->GetValue();
    settings.toolbarLargeIcons = m_toolbarLargeIcons->GetValue();
    settings.showTerminalScrollbar = m_showTerminalScrollbar->GetValue();
    return settings;
}
