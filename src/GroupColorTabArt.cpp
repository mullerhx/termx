#include "GroupColorTabArt.h"

wxAuiTabArt* GroupColorTabArt::Clone()
{
    return new GroupColorTabArt(*this);
}

void GroupColorTabArt::SetPageColour(wxWindow* page, const wxColour& colour)
{
    m_pageColours[page] = colour;
}

void GroupColorTabArt::ClearPageColour(wxWindow* page)
{
    m_pageColours.erase(page);
}

void GroupColorTabArt::DrawTab(wxDC& dc, wxWindow* wnd, const wxAuiNotebookPage& page,
                               const wxRect& inRect, int closeButtonState, wxRect* outTabRect,
                               wxRect* outButtonRect, int* xExtent)
{
    auto it = m_pageColours.find(page.window);
    if (it == m_pageColours.end())
    {
        wxAuiDefaultTabArt::DrawTab(dc, wnd, page, inRect, closeButtonState, outTabRect,
                                    outButtonRect, xExtent);
        return;
    }

    // wxAuiGenericTabArt (what wxAuiDefaultTabArt resolves to outside of
    // native MSW/GTK2 theming, which is what this GTK3 build uses) derives
    // both the normal and selected tab fill from these members rather than
    // from any per-call parameter, so overriding them for the scope of this
    // one DrawTab() call is the only way to change a single tab's colour.
    const wxColour savedBase = m_baseColour;
    const wxColour savedActive = m_activeColour;
    const wxBrush savedBaseBrush = m_baseColourBrush;
    const wxPen savedBasePen = m_baseColourPen;

    m_baseColour = it->second;
    m_activeColour = it->second;
    m_baseColourBrush = wxBrush(it->second);
    m_baseColourPen = wxPen(it->second.ChangeLightness(70));

    wxAuiDefaultTabArt::DrawTab(dc, wnd, page, inRect, closeButtonState, outTabRect, outButtonRect,
                                xExtent);

    m_baseColour = savedBase;
    m_activeColour = savedActive;
    m_baseColourBrush = savedBaseBrush;
    m_baseColourPen = savedBasePen;
}
