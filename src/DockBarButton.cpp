#include "DockBarButton.h"

#include <wx/dcbuffer.h>
#include <wx/tglbtn.h>

namespace
{
    constexpr int kSidePadding = 6;
    constexpr int kTopPadding = 8;
    constexpr int kIconTextGap = 6;
    constexpr int kBottomPadding = 8;
}

DockBarButton::DockBarButton(wxWindow* parent, wxWindowID id, const wxBitmap& icon,
                             const wxString& label)
    : wxWindow(parent, id), m_icon(icon), m_label(label)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetCursor(wxCursor(wxCURSOR_HAND));

    Bind(wxEVT_PAINT, &DockBarButton::OnPaint, this);
    Bind(wxEVT_LEFT_UP, &DockBarButton::OnLeftUp, this);
    Bind(wxEVT_ENTER_WINDOW, &DockBarButton::OnEnter, this);
    Bind(wxEVT_LEAVE_WINDOW, &DockBarButton::OnLeave, this);

    SetMinSize(DoGetBestClientSize());
}

void DockBarButton::SetToggled(bool toggled)
{
    if (m_toggled == toggled)
        return;

    m_toggled = toggled;
    Refresh();
}

wxSize DockBarButton::DoGetBestClientSize() const
{
    wxClientDC dc(const_cast<DockBarButton*>(this));
    dc.SetFont(GetFont());
    const wxSize textExtent = dc.GetTextExtent(m_label);

    // Rotated 90 degrees: the label's normal width becomes vertical extent,
    // its normal height becomes horizontal thickness.
    const int width = wxMax(m_icon.GetWidth(), textExtent.GetHeight()) + 2 * kSidePadding;
    const int height =
        kTopPadding + m_icon.GetHeight() + kIconTextGap + textExtent.GetWidth() + kBottomPadding;

    return wxSize(width, height);
}

void DockBarButton::OnPaint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    const wxSize size = GetClientSize();

    wxColour background = GetParent()->GetBackgroundColour();
    if (m_toggled)
        background = wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT);
    else if (m_hovered)
        background = background.ChangeLightness(115);

    dc.SetBrush(wxBrush(background));
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRectangle(0, 0, size.GetWidth(), size.GetHeight());

    const wxColour foreground = m_toggled
        ? wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT)
        : GetParent()->GetForegroundColour();

    const int iconX = (size.GetWidth() - m_icon.GetWidth()) / 2;
    dc.DrawBitmap(m_icon, iconX, kTopPadding, true);

    dc.SetFont(GetFont());
    dc.SetTextForeground(foreground);
    const wxSize textExtent = dc.GetTextExtent(m_label);

    // Rotated text: (x, y) is the top-left corner of the un-rotated text,
    // which for a 90 degree rotation becomes the top of the vertical run —
    // so this starts it flush below the icon, reading top-to-bottom, and
    // centers it on the icon's horizontal midpoint via the half-thickness
    // offset (its "height" once rotated).
    const int textX = size.GetWidth() / 2 + textExtent.GetHeight() / 2;
    const int textY = kTopPadding + m_icon.GetHeight() + kIconTextGap;
    dc.DrawRotatedText(m_label, textX, textY, -90.0);
}

void DockBarButton::OnLeftUp(wxMouseEvent&)
{
    SetToggled(!m_toggled);

    wxCommandEvent event(wxEVT_TOGGLEBUTTON, GetId());
    event.SetEventObject(this);
    event.SetInt(m_toggled ? 1 : 0);
    ProcessWindowEvent(event);
}

void DockBarButton::OnEnter(wxMouseEvent&)
{
    m_hovered = true;
    Refresh();
}

void DockBarButton::OnLeave(wxMouseEvent&)
{
    m_hovered = false;
    Refresh();
}
