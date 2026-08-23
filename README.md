An extensible shell in C for *nix

Dependencies:

3d graphics, X11/Wayland

Plugins may depend on pipewire, brightnessctl, DBus, or whichever interface they interact with.

Goals:
- [x] Display on screen
- [x] Popups
- [ ] Vulkan renderer (or opengl etc.)
- [ ] Multi-display support
- [x] Plugins
  - [x] Custom plugin API (.so)
    - [ ] Widgets oh no
  - [ ] Examples - 
    - [ ] Tray (SNI, DBusMenu)
    - [x] Bar
      - [ ] Chainloading plugins
    - [ ] MPRIS
    - [ ] Notifications
    - [ ] Clock
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
    - [ ] Removable devices (udisk2)
    - [ ] PAM
    - [ ] Lock screen
    - [ ] idle daemon (notify/inhibit)
    - [ ] DPMS
    - [ ] Lock screen
- [ ] IPC (UNIX sockets)
- [ ] Hot reload
- [ ] Configuration
- [ ] Theming
- [ ] Multi monitor
- [ ] Persistent state
- [x] X11
  - [ ] XEmbed
  - [ ] XRandR
  - [ ] XInput
  - [ ] XCB/Xlib
  - [ ] X11 tray
