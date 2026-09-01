#include "ShortcutPicker.h"

ShortcutPicker::ShortcutPicker(wxWindow* parent, int flags, int keyCode)
    : wxTextCtrl(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                 wxTE_READONLY | wxTE_PROCESS_TAB | wxTE_CENTRE),
      m_flags(flags), m_keyCode(keyCode)
{
    UpdateLabel();
    Bind(wxEVT_KEY_DOWN, &ShortcutPicker::OnKeyDown, this);
    Bind(wxEVT_SET_FOCUS, &ShortcutPicker::OnSetFocus, this);
    Bind(wxEVT_KILL_FOCUS, &ShortcutPicker::OnKillFocus, this);
}

void ShortcutPicker::OnSetFocus(wxFocusEvent& event)
{
    SetValue("Press a key combination...");
    event.Skip();
}

void ShortcutPicker::OnKillFocus(wxFocusEvent& event)
{
    UpdateLabel(); // no-op if a key was already captured and already redrawn
    event.Skip();
}

void ShortcutPicker::OnKeyDown(wxKeyEvent& event)
{
    const int keyCode = event.GetKeyCode();

    // A bare modifier press isn't a usable shortcut on its own — keep
    // waiting for the real key it's held down alongside.
    if (keyCode == WXK_CONTROL || keyCode == WXK_SHIFT || keyCode == WXK_ALT ||
        keyCode == WXK_WINDOWS_LEFT || keyCode == WXK_WINDOWS_RIGHT)
    {
        return;
    }

    // Escape cancels the capture and restores whatever was assigned before.
    if (keyCode != WXK_ESCAPE)
    {
        int flags = wxACCEL_NORMAL;
        if (event.ControlDown()) flags |= wxACCEL_CTRL;
        if (event.ShiftDown()) flags |= wxACCEL_SHIFT;
        if (event.AltDown()) flags |= wxACCEL_ALT;
        m_flags = flags;
        m_keyCode = keyCode;
    }

    UpdateLabel();
    Navigate(); // move focus off, so a stray follow-up key press isn't captured too
}

void ShortcutPicker::UpdateLabel()
{
    const wxAcceleratorEntry entry(m_flags, m_keyCode);
    SetValue(entry.ToString());
}
