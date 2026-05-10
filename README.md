> [!WARNING]
> This is absolutely not production ready this is only hobbyist project

# Lime Bar

A simple lightweight status bar inspired by [dsblocks](https://github.com/ashish-yadav11/dsblocks) and [lemonbar](https://github.com/lemonBoy/bar) that:
- Is simple and works only on X11 (tested only on Linux)
- Is configured in C (file `config.h`)
- Has customizable: content, font, colors, height
- Works on blocks that: can be updated on a interval, can be clicked, can be signaled

## Quick Start

Configure `config.h` however you like.

Compile with:

```console
$ make
```

Run with:

```console
$ ./limebar
```

Then you probably want to add it to your window manager startup script.
