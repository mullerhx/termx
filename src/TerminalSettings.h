#pragma once

#include <wx/wx.h>

struct TerminalSettings
{
    wxString fontFamily = "Monospace";
    int fontSize = 11;
    double lineSpacing = 1.0;
    wxColour foregroundColor = wxColour(255, 255, 255);
    wxColour backgroundColor = wxColour(0, 0, 0);
    double opacityPercent = 100.0;

    // Main window toolbar.
    bool toolbarShowLabels = false;
    bool toolbarLargeIcons = false;

    // Per-terminal (per split pane) scrollbar.
    bool showTerminalScrollbar = false;

    // Persists to/from the same "termx" config directory ConnectionStore
    // uses, as its own file — so settings survive restarts. Load() leaves
    // defaults in place for any field missing from disk (a fresh install,
    // or one from before a given field existed).
    void Load();
    void Save() const;
};
