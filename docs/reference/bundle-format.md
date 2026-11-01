# `trustedvm` bundle and dtbo container formats

Both formats are byte-layout facts, not guesses — they keep costing a cycle each
time they are rediscovered.

## MBN bundle (`trustedvm.mdt` + `trustedvm.bNN`)

* ELF64, one `PT_LOAD` per segment, payloads concatenated in file order.
* segment flags encode the keys: `b04` (flags `0x2000000`) is the signature
  chain, a hash segment sits next to it.
* signing is the standard Qualcomm MBN/ELF flow — `qtestsign` works and the
  driver accepts it (`journal/09`). The images are **test**-signed
  (`SecTools Test User`), so no private key is needed.

Flow used to modify a payload:

```
# 1. concatenate mdt + b00…bNN into one image, patch the payload
qtestsign.py -v 6 -o signed.bin multi-image combined.bin
# 2. split back into .mdt + .b00 .b01 … following the new phdrs
#    .mdt  = bytes[0 : end of the last non-PT_LOAD segment]
#    .b00  = bytes[0 : ELF header end]
#    .bNN  = payload of the Nth PT_LOAD segment
```

Delivery must use the direct firmware path: remount `/vendor/firmware_mnt` rw
and drop the bundle into `image/` (label/context already `firmware_file`).

## `b02` — the guest device tree

`b02` is **not** payload: it is a dtbo table (`0xd7b7ab1e`) with 22 entries, one
per board variant. Entry 13 matches houji
(`qcom,msm-id = <0x22d … 0x241 …>`, `qcom,board-id = <0x15 0x00>`).

The entries are templates: `generate = "/virtio-mmio"`, `vqs-num`, `dma_base`,
`peer-default`, and a `memory { qcom,label, base }` child — but **no `reg`, no
`interrupts`**. `chosen.bootargs` carries `console=hvc0 loglevel=8`.

## DTBO table layout (also applies to `dtbo` partition overlays)

```
0x00 magic 0xd7b7ab1e | total size | header_size | entry_size | count | entries_off | page_size | version
```
* one 32-byte entry struct per fragment (`size`, `data offset`, 4 id slots,
  `custom`); the **data area starts after the last entry struct**
  (`entries_off + count*entry_size`) and must be page aligned-per-item.
* board selectors belong in the fragment body as properties
  (`qcom,msm-id`, `qcom,board-id`, `xiaomi,miboard-id`).
* dtbo-overlay quirks and rollback: `journal/15`, `docs/RESTORE.md`.
  Known: overlays from the dtbo partition never show up in
  `/proc/device-tree` and the loader ignores them (`journal/19`).
