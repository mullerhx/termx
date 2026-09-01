#pragma once

#include <wx/wx.h>
#include <wx/clrpicker.h>
#include <wx/spinctrl.h>

#include "TerminalSettings.h"

class PreferencesDialog : public wxDialog
{
public:
    PreferencesDialog(wxWindow* parent, const TerminalSettings& settings);

    TerminalSettings GetSettings() const;

private:
    wxComboBox* m_fontFamily = nullptr;
    wxSpinCtrl* m_fontSize = nullptr;
    wxSpinCtrlDouble* m_lineSpacing = nullptr;
    wxColourPickerCtrl* m_foregroundColor = nullptr;
    wxColourPickerCtrl* m_backgroundColor = nullptr;
    wxSpinCtrlDouble* m_opacity = nullptr;
    wxCheckBox* m_toolbarShowLabels = nullptr;
    wxCheckBox* m_toolbarLargeIcons = nullptr;
    wxCheckBox* m_showTerminalScrollbar = nullptr;
};
