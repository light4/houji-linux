# Desktop for the Arch 客户机

Constraints of the guest ([journal/57](journal/57-hugepage-reserve-1536m.md),
[58](journal/58-pseudo-unprotected-nokaslr.md)):

* ~1.4 GiB RAM (+2 GiB swap on the virtual disk), 4 vCPU;
* display through DroidVM — *Native Display* (virtio-gpu / simplefb), VNC, or an
  external screen via the Presentation API; no native GPU, 3D only via DroidVM's
  virgl/gfxstream path;
* input mostly touch or a mouse/keyboard forwarded by DroidVM.

## Comparison

| | Xfce | LXQt | Sway | GNOME / KDE Plasma |
|---|---|---|---|---|
| display server | X11 | X11 (Wayland optional) | Wayland | Wayland (X11 fallback) |
| idle RAM (approx.) | 300–400 MiB | 250–350 MiB | 100–150 MiB | 800 MiB–1.2 GiB |
| needs 3D / KMS | no (2D, simplefb/virtio-gpu, VNC fine) | no | KMS/DRM required, no 3D | effectively yes for a smooth shell |
| touch / mouse use | good: panel + windows, big targets possible | good | poor: keyboard-driven tiling | good, but heavy |
| VNC friendliness | very good (classic X11) | good | needs `wayvnc` | poor on low RAM |
| maturity on Arch Linux ARM | high | medium | high | high, but heavy |
| fit for this guest | **best** | good alternative | only if keyboard-only | no — RAM too tight |

### Pros and cons

**Xfce — recommended**

* \+ light enough to leave room for a browser in 1.4 GiB;
* \+ runs on plain 2D (simplefb or virtio-gpu 2D) and over VNC; no GPU work needed first;
* \+ classic panel/window UI works with touch and a mouse;
* \+ complete, stable aarch64 packages;
* − X11 only; no smooth compositing without 3D acceleration;
* − looks dated, and its high-DPI scaling is manual.

**LXQt**

* \+ slightly lighter than Xfce, Qt-based;
* − less polished, fewer integrated tools; Wayland session still maturing.

**Sway**

* \+ lightest; Wayland-native;
* − tiling and keyboard-driven, awkward on a phone screen;
* − needs DRM/KMS (virtio-gpu) and `wayvnc` for remote viewing.

**GNOME / KDE Plasma**

* \+ the most polished, touch-friendly shells;
* − need ~1 GiB just idle, plus 3D for a smooth shell; would push the guest
  into swap. Only with a much larger `memory_mb` and `pool_want`.

## Plan (Xfce)

1. Guest kernel v3 (`guest/build_guest.sh`): `DRM_VIRTIO_GPU`, `DRM_SIMPLEDRM`,
   `VIRTIO_INPUT`, `SND_VIRTIO` — these were `=m` in defconfig and dropped by
   `MODULES=n`.
2. In the guest: `xorg-server xfce4 lightdm lightdm-gtk-greeter`, autologin into
   the Xfce session.
3. Enable the VM's display in DroidVM (*Native Display*), check touch/mouse input.
