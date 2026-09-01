#pragma once

#include <wx/wx.h>
#include <wx/accel.h>

// Shared wxWindowIDs for the configurable next/previous-tab shortcuts.
// MainFrame binds these to wxEVT_MENU both via its own accelerator table
// (for when a wx-native control has focus) and as the target TerminalPanel
// re-fires them as when VTE has focus — see TerminalPanel.cpp, where VTE's
// own default key handling would otherwise swallow the keystroke before the
// frame's accelerator table ever saw it.
constexpr wxWindowID kNextTabCommandId = wxID_HIGHEST + 9001;
constexpr wxWindowID kPreviousTabCommandId = wxID_HIGHEST + 9002;

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

    // Tab-switching shortcuts. Stored as a wxAcceleratorEntry flags/keycode
    // pair rather than a display string, so there's no parsing ambiguity —
    // wxAcceleratorEntry::ToString() derives the display form on demand.
    int nextTabAccelFlags = wxACCEL_CTRL;
    int nextTabAccelKeyCode = WXK_TAB;
    int prevTabAccelFlags = wxACCEL_CTRL | wxACCEL_SHIFT;
    int prevTabAccelKeyCode = WXK_TAB;

    // Persists to/from the same "termx" config directory ConnectionStore
    // uses, as its own file — so settings survive restarts. Load() leaves
    // defaults in place for any field missing from disk (a fresh install,
    // or one from before a given field existed).
    void Load();
    void Save() const;
};
