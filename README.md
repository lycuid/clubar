XD Bar
------
A minialistic, lightweight, customizable statusbar, inspired by [**X**mobar](https://xmobar.org/) and [**D**wm](https://dwm.suckless.org/)'s statusbar, for X11 Desktops, that are compatible with
[EWMH](https://specifications.freedesktop.org/wm-spec/latest/) specifications.

Rendering styled/clickable text, is inspired by xmobar's approach of having XML like `tags` in the string.  
Populating statusbar is inspired by dwm statusbar's approach of using the root window's `WM_NAME` attribute text.

Future Goals: Maybe rewrite in Rust. :pepehands:

- [Description](#description)
- [Build](#build)
  - [Requirements](#requirements)
  - [Build and Install](#build-and-install)
- [Styling text](#styling-text)
- [Examples](#examples)
- [Licence](#licence)

Screenshots
-----------
![dark.png](https://raw.githubusercontent.com/lycuid/xdbar/master/screenshots/dark.png)

----

![light.png](https://raw.githubusercontent.com/lycuid/xdbar/master/screenshots/light.png)

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
- ***stdin*** text is typically provided by the window manager
- ***WM_NAME*** (of the root window) can be set using the `xsetroot` command.

**Setting `WM_NAME`**
- [slstatus](https://tools.suckless.org/slstatus)
- [smolprog](https://github.com/lycuid/smolprog) (much faster and overall just better than slstatus...coz. Rust. I guess?).
- Custom script example:
```bash
#!/bin/bash
while true; do
  xsetroot -name "$(whoami)@$(hostname) on $(uname -o) | $(date)"
  sleep(1)
done
```

Build
-----
#### Requirements
  - gnu make
  - libx11
  - libxft

**Optional Requirements**
  - pkg-config  (if not installed, update `config.mk` accordingly).
  - lua         (required if using `luaconfig` patch).

#### Build and install
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
(Note: plugins are just space seperated c filenames, from `include/plugins/`,
  without file extension.)

- **luaconfig**: runtime config support with lua source file.
- **xrmconfig**: runtime config support with X Resources.

Styling text
------------
**Template for styling text**
```text
<Tag:Mod1|Mod2|...|Modn=Value> Text </Tag>
```

| Tag     | Modifiers                 |
|---------|---------------------------|
| Fn      | -                         |
| Fg      | -                         |
| Bg      | -                         |
| Box     | Left, Right, Top, Bottom  |
| BtnL    | Shift, Ctrl, Super, Alt   |
| BtnM    | Shift, Ctrl, Super, Alt   |
| BtnR    | Shift, Ctrl, Super, Alt   |
| ScrlU   | Shift, Ctrl, Super, Alt   |
| ScrlD   | Shift, Ctrl, Super, Alt   |

**Valid Values for Tags**
```xml
<Fn=Number>
```
- `Number` is the index of 'fonts' array in 'config.h'.
- `fonts[0]` will be used as default font.
```xml
<Fg=Color>
<Bg=Color>
```
- `Color` can be any valid color name.  
  - (eg. `<Fg=black> </Fg>` `<Bg=#000000> </Bg>`)
```xml
<Box=Color>
<Box=Color:Size>
```
- `Color` can be any valid color name.
- `Size` in pixels (if not provided, defaults to 1).  
  - (eg. `<Box=yellow:2> </Box>` `<Box=#dfdfdf> </Box>`)
```xml
<BtnL=Command>
<BtnM=Command>
<BtnR=Command>
<ScrlU=Command>
<ScrlD=Command>
```
- `Command` can be any string. command output/error will not be handled.  
  - (eg. `<BtnL=systemctl poweroff> </BtnL>` `<BtnR=systemctl reboot> </BtnR>`)

Examples
--------
```xml
<Fg=#131313> Colored </Fg> text with <Bg=#c6c6c6> background</Bg>.
```
```xml
<Box:Bottom=#ffffff:1> underlined </Bottom> text.
<Box:Top|Bottom|Left|Right=#ffffff:1> Boxed in </Bottom> text.
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
