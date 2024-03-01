# pixdecor
![pixdecor](https://github.com/soreau/pixdecor/assets/1450125/af891554-8eeb-4769-b571-fa587afd8350)

A highly configurable and optionally fancy decorator plugin for wayfire.

## Installing

Set `--prefix` to the same as the wayfire installation.

```
$ meson setup build --prefix=/usr
$ ninja -C build
# ninja -C build install
```

Restart wayfire.

## Running

Disable other decorator plugins and enable pixdecor plugin in core section of `wayfire.ini`.
