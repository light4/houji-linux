# Secure Launch is not implemented on this firmware

## Method

Read-only dumps of every partition that could carry EL2 firmware, then string
scans; plus a full grep of `/vendor`, `/odm`, `/system/etc`, `/data/vendor`.

```bash
for p in hyp_a xbl_a xbl_config_a tz_a qupfw_a devcfg_a; do
  adb shell su -c "dd if=/dev/block/by-name/$p of=/data/local/tmp/$p.bin bs=1M"
  adb pull /data/local/tmp/$p.bin
  strings -n 6 $p.bin | grep -iE 'mssecapp|hyp_manager_launch|SlpApp|tcblaunch'
done
```

## Result

* `mssecapp`, `hyp_manager_launch`, `SlpApp`, `tcblaunch`: **absent everywhere**
* only trace: the string `hyp_secure_launch_enabled` inside `hyp_a`
  ([evidence](../../evidence/hyp-strings.txt))
* `devcfg_a` has no `hyp_secure_launch_enabled` key — the hyp config
  allow-list carries 14 unrelated entries
  ([evidence](../../evidence/devcfg-keys.txt))

## The key is parsed into a field nobody reads

The hyp config parser stores the (never present) value into a global at
`0x221194`:

```
69b88: bl    <read hyp_config prop>
69b90: ldr   w8, [sp, #0x40]      ; value
69b98: str   w8, [x9, #0x194]     ; hyp_secure_launch_enabled
```

Across the full 8 MB disassembly there is **exactly one write and no reader**
of `0x221194`. The neighbouring `hyp_secure_mor_enabled` (global `0x221190`)
does have a consumer, which is how you tell a live field from a stub:

```
696a0: ldr  w8, [x8, #0x190]      ; hyp_secure_mor_enabled
696a4: cbz  w8, 0x698f4           ; skip when 0
696a8: mov  w0, #0x220            ; else reserve 0x220 secure pages
```

Also: only 4 `0xc200*` (TZ app invoke) constants exist in the image, none of
them part of a Secure-Launch auth sequence.

## Meaning

slbounce drives the vendor Secure Launch path: `tcblaunch.exe` is verified by
the TZ app `mssecapp`, and QHEE's `hyp_manager_launch` hands EL2 to it. None of
that machinery is in this build, so slbounce fails at the first SMC regardless
of devcfg. Patching devcfg would not help either — the EL2 side has nobody
listening.
