cluless statusbar
-----------------
A minialistic, lightweight, customizable statusbar (formerly *xdbar*), inspired by [xmobar](https://xmobar.org/) and [dwm](https://dwm.suckless.org/)'s statusbar, for X11 Desktops (wayland in progress), that are compatible with [EWMH](https://specifications.freedesktop.org/wm-spec/latest/) specifications.

Rendering styled/clickable text, is inspired by xmobar's approach of wrapping text in XML like *tags*.  
Populating statusbar is inspired by dwm statusbar's approach of using the root window's `WM_NAME` attribute text.

![dark.png](https://raw.githubusercontent.com/lycuid/clubar/master/screenshots/dark.png)

----

![light.png](https://raw.githubusercontent.com/lycuid/clubar/master/screenshots/light.png)

Description
-----------
```txt
  + - - - - - - - - - - - - - - - - - margin - - - - - - - - - - - - - - - - - +
  ¦ +------------------------------------------------------------------------+ ¦
  ¦ | + - - - - - - - - - - - - - - - padding - - - - - - - - - - - - - - -+ | ¦
  ¦ | ¦stdin                                                        WM_NAME¦ | ¦
  ¦ | +- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - + | ¦
  ¦ +------------------------------------------------------------------------+ ¦
  +- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
```
- ***margin*** is the empty space outside of the bar window.
- ***padding*** is the space between the bar window edges and the drawing region.
- ***stdin*** text is typically provided by the window manager.
- ***WM_NAME*** (of the root window) can be set using the `xsetroot` command.

**Setting `WM_NAME`**
- [slstatus](https://tools.suckless.org/slstatus)
- [smolprog](https://github.com/lycuid/smolprog/) *(my personal script)*
- Custom script example:
```bash
#!/bin/bash
while true; do
  xsetroot -name "$(whoami)@$(hostname) on $(uname -o) | $(date)"
  sleep 1
done
```
Usage
-----
```txt
USAGE: clubar [OPTIONS]...
OPTIONS:
  -h    print this help message.
  -v    print version.
  -c <config_file>
        filepath for runtime configs (supports: lua).
SIGNALS:
  USR1: toggle window visibility (e.g. pkill -USR1 clubar).
```
Requirements
------------
  - *free* and *open source* Operating System (currently doesn't support \*BSD).
  - gnu make
  - libx11
  - libxft

**Optional**
  - pkg-config  (if not installed, update `config.mk` accordingly).
  - lua         (required if using `luaconfig` plugin).

Build
-----
**Default build**
```sh
make
```
**Build with plugins enabled**
```sh
make PLUGINS="luaconfig xrmconfig ..."
```
**Install**
```sh
sudo make install
```

**Available Plugins** 
(Note: plugins are just space seperated c filenames, from [plugins](/src/clubar/plugins/) directory, without file extension, see [examples](/examples).)

- **luaconfig**: runtime config support with lua source file.
- **xrmconfig**: runtime config support with X Resources.

Styling text
------------
**Template for styling text**
```text
<Tag:Mod1|Mod2|...|Modn=Value> Text </Tag>
```

| Tag     | Modifiers                 | Value       | Description                                       |
|---------|---------------------------|-------------|---------------------------------------------------|
| Fn      | -                         | Number      | Index of 'fonts' array from configs (default: 0)  |
| Fg      | -                         | Color       | Valid color name                                  |
| Bg      | -                         | Color       | Valid color name                                  |
| Box     | Left, Right, Top, Bottom  | Color:Size  | valid color name and size (default: 0) in pixels  |
| BtnL    | Shift, Ctrl, Super, Alt   | Command     | Raw command                                       |
| BtnM    | Shift, Ctrl, Super, Alt   | Command     | Raw command                                       |
| BtnR    | Shift, Ctrl, Super, Alt   | Command     | Raw command                                       |
| ScrlU   | Shift, Ctrl, Super, Alt   | Command     | Raw command                                       |
| ScrlD   | Shift, Ctrl, Super, Alt   | Command     | Raw command                                       |

Examples
--------
```xml
<Fn=1><Fg=#131313> Colored </Fg> text with <Bg=#c6c6c6> background</Bg></Fn>.
```
```xml
<Box:Bottom=#ffffff:1> underlined </Box> text.
<Box:Top|Bottom|Left|Right=#ffffff:1> Boxed in </Box> text.
```
```xml
<BtnL=systemctl reboot> reboot </BtnL>
<BtnL:Ctrl|Shift=sudo -A reboot now> reboot w/ confirmation </BtnL>
```
```xml
<ScrlU:Ctrl=amixer sset Master 5%+>
  <ScrlD:Ctrl=amixer sset Master 5%->
    volume
  </ScrlD>
</ScrlU>
```

Licence:
--------
[GPLv3](https://gnu.org/licenses/gpl.html)
