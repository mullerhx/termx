#pragma once

#include <wx/wx.h>

// A single toggle "tab" for MainFrame's left-hand dock bar: a small icon
// with its label rotated to read top-to-bottom immediately below it, both
// centered on the same vertical axis — the classic auto-hide tool window
// tab look. wxAuiToolBar has no text-rotation support, so this is a plain
// custom-drawn control instead.
class DockBarButton : public wxWindow
{
public:
    DockBarButton(wxWindow* parent, wxWindowID id, const wxBitmap& icon, const wxString& label);

    bool IsToggled() const { return m_toggled; }
    void SetToggled(bool toggled);

    bool AcceptsFocus() const override { return false; }

private:
    void OnPaint(wxPaintEvent& event);
    void OnLeftUp(wxMouseEvent& event);
    void OnEnter(wxMouseEvent& event);
    void OnLeave(wxMouseEvent& event);
    wxSize DoGetBestClientSize() const override;

    wxBitmap m_icon;
    wxString m_label;
    bool m_toggled = false;
    bool m_hovered = false;
};
