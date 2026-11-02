# Pseudo-unprotected boots with `nokaslr`; unused packages removed

## Pseudo-unprotected

DroidVM's default Gunyah mode (`protected_vm: pseudo_unprotected`) runs the VM
protected but shares guest RAM back at run time: crosvm's `droidvm-shim`
(`droidvm-shim/src/main.rs`) accepts the parcels from the handoff page, patches
`/memory`, and jumps to the payload with x0 = DTB.

With our kernel it died right after the jump (journal/55):

```
SHIM: jumping to 0x80600000
SHIM EXCEPTION 0x4 esr=0x2000000 elr=0x834d9400 far=0x0
```

`elr - payload = 0x2ed9400`, which maps to `swapper_pg_dir+0x6400` in System.map —
page-table data executed as code (EC 0 = unknown instruction), before the
kernel had installed its own vectors. Adding **`nokaslr`** to the cmdline fixes it:

```
SHIM: accept handle 0x9 at 0x80600000 size 0x5f600000
SHIM: /memory now 0x80600000+0x5f600000
SHIM: jumping to 0x80600000
[    0.000000] Booting Linux on physical CPU 0x0000000000 [0x000f0480]
[    0.046994] KASLR disabled on command line
houji-vm login: root (automatic login)
```

So the early KASLR relocation path goes wrong after the shim hand-off. I haven't
narrowed it further; it's worth an upstream report. In this mode there is no swiotlb:
`free` shows **1442 MiB** (vs 1384 MiB without firmware, same 1536 MiB config),
and virtio I/O no longer bounces. This is now the VM's mode
(`cmdline … rootwait nokaslr`).

## Space

`pacman -Rns linux-firmware linux-firmware-*` (+ orphans; `linux-aarch64` and
`mkinitcpio` were already gone): `/` used 4.6 G → 3.2 G. We boot an external
kernel, so none of it was used.
