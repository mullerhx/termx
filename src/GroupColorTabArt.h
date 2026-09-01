#pragma once

#include <wx/aui/aui.h>
#include <map>

// A wxAuiNotebook tab art provider that lets individual tabs (pages) draw
// with an overridden background colour instead of the normal theme one —
// used to color-code terminal tabs by which connection group they came
// from. wxAuiNotebook has no built-in per-tab colour API, so this overrides
// DrawTab() to temporarily swap the base class's colour state for just the
// duration of drawing a colored tab, then restores it.
//
// Two wxAuiNotebook quirks (both confirmed empirically) shape how a page's
// colour actually gets applied — see MainFrame::AddTerminalTab:
//   1. The first AddPage() call clones whatever art provider is current for
//      its own internal use; the object handed to SetArtProvider() earlier
//      becomes an orphaned copy from then on. Mutating it directly (e.g.
//      SetPageColour()) has no visible effect — the live instance has to be
//      re-synced with wxNotebook->SetArtProvider(orphan->Clone()), then
//      GetArtProvider() re-fetched as the new "live" pointer to mutate next.
//   2. wxAuiNotebook measures/lays out each tab once, at the point it's
//      added, so step 1 has to happen *before* that page's own AddPage()
//      call — coloring an already-added tab the same way still repaints it
//      correctly, but leaves its geometry stuck at whatever it was
//      originally measured as (a squashed/misaligned tab).
class GroupColorTabArt : public wxAuiDefaultTabArt
{
public:
    wxAuiTabArt* Clone() override;

    void SetPageColour(wxWindow* page, const wxColour& colour);
    void ClearPageColour(wxWindow* page);

    void DrawTab(wxDC& dc, wxWindow* wnd, const wxAuiNotebookPage& page, const wxRect& inRect,
                int closeButtonState, wxRect* outTabRect, wxRect* outButtonRect,
                int* xExtent) override;

private:
    std::map<wxWindow*, wxColour> m_pageColours;
};
