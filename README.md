# oneko-hypr

a very lightweight port of oneko to hyprland using GTK + gtk-layer-shell. it renders a click-through overlay window that follows the mouse using cursor data read from hyprlands unix socket

## dependencies (arch)

```sh
sudo pacman -S gtk3 gtk-layer-shell gdk-pixbuf2
```

## install

```sh
sudo make install
```

or just:

```sh
make
```
if you don't wanna install it

## autostart

add this to `~/.config/hypr/hyprland.conf`:

```ini
exec-once = oneko-hypr
```

if you didn't install it, use the full path to the built binary:

```ini
exec-once = /path/to/oneko-hypr
```

## extra

optional sprite path:

```sh
./oneko-hypr /path/to/oneko.gif
```