An extensible shell in C for *nix

Dependencies:

Graphics(Vulkan), X11/Wayland, Freetype, Harfbuzz, yyjson

Plugins may depend on pipewire, brightnessctl, DBus, or whichever interface they interact with.

Goals:
- [x] Display on screen
- [x] Popups
- [x] Vulkan renderer (or opengl etc.) (partial)
  - [x] Text
  - [x] Rectangle
  - [ ] Shared Vulkan instance & device across surfaces
  - [ ] BVH and lazy redraw support
- [ ] Multi-display / multi-monitor support
- [x] Plugins
  - [x] Custom plugin API (.so)
  - [x] Worker thread & event dispatch
  - [x] Widgets (partial)
    - [x] Containers (VBox, HBox)
    - [ ] Widget event & input handling
    - [ ] Padding, margins, and flexible sizing
  - [ ] Examples
    - [ ] Tray (SNI, DBusMenu)
    - [x] Bar
      - [x] Clock (sort-of, isn't independent)
    - [ ] MPRIS
    - [ ] Notifications
      - [ ] Chainloading plugins
    - [ ] Calendar (CalDAV)
    - [ ] Launcher
    - [ ] Workspace management
    - [ ] Window/task list (Bar plugin)
      - [ ] Hyprland
      - [ ] Sway
      - [ ] xdg-activation
      - [ ] foreign-toplevel-list
      - [x] X11 (kind of - incomplete)
        - [ ] complete it
    - [ ] Bluetooth (BlueZ etc.), NetworkManager
    - [ ] Audio (pipewire/wpctl)
    - [ ] Power / UPower
    - [ ] Brightness (acpilight/brightnessctl etc.)
    - [ ] Network
    - [ ] Clipboard
    - [ ] Removable devices (udisks2)
    - [ ] PAM
    - [ ] Lock screen
    - [ ] idle daemon (notify/inhibit)
    - [ ] DPMS
- [ ] IPC (UNIX sockets)
- [ ] Configuration
  - [x] JSON5 config (via yyjson)
  - [x] Per-plugin config
  - [ ] Config hot reload (file watching / inotify / IPC)
- [ ] Plugin hot reload
- [ ] Theming
- [ ] Persistent state
- [x] Wayland
  - [x] wlr-layer-shell (bars, docks)
  - [x] xdg-shell (popups, toplevel)
  - [ ] Layer shell fractional scaling
  - [ ] Input handling (wl_seat, pointer, keyboard)
- [ ] X11
  - [ ] XEmbed
  - [ ] XRandR
  - [ ] XInput
  - [x] XCB / EWMH
  - [ ] X11 tray
