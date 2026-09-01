#pragma once

#include <wx/wx.h>
#include <wx/accel.h>

// A read-only field for assigning a keyboard shortcut: click it to focus
// (which starts capture), then press the desired key combination. Any key
// press other than a bare modifier or Escape is accepted immediately —
// there's no separate "confirm" step.
class ShortcutPicker : public wxTextCtrl
{
public:
    ShortcutPicker(wxWindow* parent, int flags, int keyCode);

    int GetFlags() const { return m_flags; }
    int GetKeyCode() const { return m_keyCode; }

private:
    void OnKeyDown(wxKeyEvent& event);
    void OnSetFocus(wxFocusEvent& event);
    void OnKillFocus(wxFocusEvent& event);
    void UpdateLabel();

    int m_flags;
    int m_keyCode;
};
