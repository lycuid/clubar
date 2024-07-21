-- This is an example config file which can be used with the '-c' option.
-- check 'clubar -h' for more info.

local screenwidth = 1366;
local border_width = 2;

local padding = {left = 2, right = 2, top = 2, bottom = 2};
local margin = {left = 0, right = 0, top = 10, bottom = 10 + border_width};

local geometry = {
    x = 0 + margin.left,
    y = 0 + margin.top,
    w = screenwidth - (margin.left + margin.right) - border_width*2,
    h = 32 + (padding.top + padding.bottom)
}

Config = {
    geometry = geometry,
    padding = padding,
    margin = margin,
    topbar = true,
    foreground = "#efefef",
    background = "#090909",
    border = "#ee33ee:" .. border_width,
    fonts = {"monospace-9", "monospace-9:bold"},
};
