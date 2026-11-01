# Stub timings, corrected — and the segment-index shift after re-signing

`journal/29` compared a stub guest against the vendor kernel but its numbers were
sloppy (one check per run). Measured properly, with a checked interval:

| guest | survival |
|---|---|
| vendor kernel (unchanged) | ~0.5 s |
| four-byte `b .` stub | ~10 s |
| stub that reads the window, then loops | ~15 s (within variance of the above) |
| stub that reads the window **and** writes `status = 0`, then loops | ~6 s |

So the "backend setup kills it in 0.5 s" claim needs a correction: the flow
(SET_FW → vCPUs → backend arm → run) is fine, and an inert guest lives an order
of magnitude longer than the vendor kernel. A single MMIO read of
`0xda6f8000` / `0xda6fc000` does not fault. The vendor guest dies early **on its
own**; that is the thing to explain, not the host sequence.

The `~10 s` for an inert guest also means something in the loader/hyp gives up on
a VM that makes no progress — worth finding, but it is not what kills the vendor
kernel at 0.5 s.

## Segment indices shift after re-signing

Worth knowing before reading a delivered bundle: `qtestsign` moves the hash and
signature segments to the front, so the delivered file names no longer match the
original meaning.

| file | before signing | after signing |
|---|---|---|
| `b01` | kernel | hash/signature block |
| `b02` | guest DT (dtbo table) | **kernel** |
| `b03` | config cpio | guest DT |
| `b04` | signature | config cpio |

The loader maps files by segment index, which is why both layouts authenticate.
`tools/rebuild_bundle.py` handles it; when checking a bundle by hand, always look
at `phdr[i].p_offset`, never assume `b01` is the kernel.

## Not established

Writing `status = 0` produced no `GH_WAIT_FOR_EVENT` event. Either the write is
lost (missing barrier / wrong window semantics) or events are delivered somewhere
we are not listening — still open.
