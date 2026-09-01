#include "TerminalSettings.h"

#include <wx/fileconf.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/utils.h>

namespace
{
    wxString GetConfigPath()
    {
        // See ConnectionStore::GetConfigPath() — GetUserConfigDir() returns
        // bare $HOME here, not ~/.config, so resolve XDG_CONFIG_HOME ourselves.
        wxString configHome;
        if (!wxGetEnv("XDG_CONFIG_HOME", &configHome) || configHome.IsEmpty())
            configHome = wxFileName::GetHomeDir() + "/.config";

        wxFileName path(configHome, "settings.conf");
        path.AppendDir("termx");
        return path.GetFullPath();
    }
}

void TerminalSettings::Load()
{
    const wxString path = GetConfigPath();
    if (!wxFileName::FileExists(path))
        return;

    wxFileConfig cfg(wxEmptyString, wxEmptyString, path, wxEmptyString, wxCONFIG_USE_LOCAL_FILE);

    cfg.Read("/Terminal/FontFamily", &fontFamily, fontFamily);
    cfg.Read("/Terminal/FontSize", &fontSize, fontSize);
    cfg.Read("/Terminal/LineSpacing", &lineSpacing, lineSpacing);
    cfg.Read("/Terminal/OpacityPercent", &opacityPercent, opacityPercent);

    long fg = 0;
    if (cfg.Read("/Terminal/ForegroundColor", &fg, foregroundColor.GetRGB()))
        foregroundColor.SetRGB(static_cast<wxUint32>(fg));

    long bg = 0;
    if (cfg.Read("/Terminal/BackgroundColor", &bg, backgroundColor.GetRGB()))
        backgroundColor.SetRGB(static_cast<wxUint32>(bg));

    cfg.Read("/Toolbar/ShowLabels", &toolbarShowLabels, toolbarShowLabels);
    cfg.Read("/Toolbar/LargeIcons", &toolbarLargeIcons, toolbarLargeIcons);

    cfg.Read("/Terminal/ShowScrollbar", &showTerminalScrollbar, showTerminalScrollbar);
}

void TerminalSettings::Save() const
{
    const wxString path = GetConfigPath();
    wxFileName::Mkdir(wxFileName(path).GetPath(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);

    wxFileConfig cfg(wxEmptyString, wxEmptyString, path, wxEmptyString, wxCONFIG_USE_LOCAL_FILE);
    cfg.DeleteAll();

    cfg.Write("/Terminal/FontFamily", fontFamily);
    cfg.Write("/Terminal/FontSize", fontSize);
    cfg.Write("/Terminal/LineSpacing", lineSpacing);
    cfg.Write("/Terminal/OpacityPercent", opacityPercent);
    cfg.Write("/Terminal/ForegroundColor", static_cast<long>(foregroundColor.GetRGB()));
    cfg.Write("/Terminal/BackgroundColor", static_cast<long>(backgroundColor.GetRGB()));
    cfg.Write("/Terminal/ShowScrollbar", showTerminalScrollbar);

    cfg.Write("/Toolbar/ShowLabels", toolbarShowLabels);
    cfg.Write("/Toolbar/LargeIcons", toolbarLargeIcons);

    cfg.Flush();
}
