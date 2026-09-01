cmake_minimum_required(VERSION 3.16)
project(termx LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    # Defaults to Debug for a plain `cmake .`; pass -DCMAKE_BUILD_TYPE=Release
    # (or reconfigure an existing build dir with it) for an optimized build —
    # CMake's built-in Release flags (-O3 -DNDEBUG for GCC/Clang) apply
    # automatically, no extra flags needed here.
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")

include(GNUInstallDirs)

find_package(wxWidgets 3.0 REQUIRED COMPONENTS core base propgrid aui)
include(${wxWidgets_USE_FILE})

# TerminalPanel embeds VTE's native GtkWidget for full xterm-256color-compatible
# terminal emulation, so this build currently targets wxGTK on Linux only.
find_package(PkgConfig REQUIRED)
pkg_check_modules(VTE REQUIRED IMPORTED_TARGET vte-2.91)

# Connection vault encryption (AES-256-GCM + PBKDF2-HMAC-SHA256) — nettle for
# the cipher/PRF primitives, hogweed (nettle's public-key-adjacent half, but
# also where pbkdf2 lives in this distro's split) for key derivation.
pkg_check_modules(NETTLE REQUIRED IMPORTED_TARGET nettle hogweed)

add_executable(termx
    src/main.cpp
    src/App.cpp
    src/MainFrame.cpp
    src/ConnectionStore.cpp
    src/TerminalSettings.cpp
    src/TerminalPanel.cpp
    src/PreferencesDialog.cpp
    src/ConnectionDialog.cpp
    src/DockBarButton.cpp
    src/GroupColorTabArt.cpp
    src/GroupDialog.cpp
    src/ReverseShellDialog.cpp
    src/VaultCrypto.cpp
    src/VaultUnlockDialog.cpp
)

target_include_directories(termx PRIVATE src)
target_link_libraries(termx PRIVATE ${wxWidgets_LIBRARIES} PkgConfig::VTE PkgConfig::NETTLE)

if(MSVC)
    target_compile_options(termx PRIVATE /W4)
else()
    target_compile_options(termx PRIVATE -Wall -Wextra)
endif()

# `cmake --install . --prefix /some/path` (or `make install` /
# `cmake --build . --target install` after configuring with
# -DCMAKE_INSTALL_PREFIX=/some/path) installs the binary to <prefix>/bin.
install(TARGETS termx RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
