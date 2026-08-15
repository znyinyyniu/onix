## Context

See proposal.md for motivation. Onix physical memory code in `src/kernel/memory.c` assumes:

- `memory_base == MEMORY_BASE` (1MiB)
- `memory_size` is the length of managed RAM **above** 1MiB
- `memory_map` lives at `memory_base`, and `start_page = IDX(MEMORY_BASE) + memory_map_pages` covers the table
- `total_pages = IDX(memory_size) + IDX(MEMORY_BASE)` indexes physical pages by `addr >> 12`

Multiboot2 under OVMF often splits Available RAM (e.g. 1–8MiB then a large block from 9MiB) with NVS/Reserved holes. The current “pick longest Available” rule sets `memory_base` to 9MiB and trips the assert.

MBI for Multiboot2 may sit near 1MiB (observed ~`0x110000`); `memory_map` is also placed at 1MiB and will overwrite the MBI once initialized—hole metadata MUST be copied out before that overwrite.

## Goals / Non-Goals

**Goals:**

- Make Multiboot2/UEFI satisfy the existing 1MiB-based model without redesigning the allocator
- Prevent allocation into non-Available holes inside the managed span
- Keep BIOS / `ONIX_MAGIC` boot working

**Non-Goals:**

- Supporting `memory_base != MEMORY_BASE` as a first-class model
- Early serial / framebuffer console / GRUB warning fixes
- Parsing ACPI or using UEFI boot services from the kernel
- Changing `KERNEL_MEMORY_SIZE` or relocating `memory_map`

## Decisions

### 1. Fix base at MEMORY_BASE; size from max Available end

- **Choice:** On Multiboot2 path, set `memory_base = MEMORY_BASE`. Scan Available entries; compute `mem_end` as the maximum of `min(addr+len, 4GiB)` for entries that extend past `MEMORY_BASE`; set `memory_size = mem_end - MEMORY_BASE`.
- **Why:** Matches existing formulas and assert; uses both the low Available strip and the large upper strip.
- **Alternatives:** Prefer only the Available region with `addr == MEMORY_BASE` → often only ~7MiB on OVMF, fails `KERNEL_MEMORY_SIZE`. Drop assert and keep base=9MiB → breaks `total_pages`/`start_page`/`memory_map` coupling (rejected).

### 2. Snapshot hole/availability data before building `memory_map`

- **Choice:** While walking the Multiboot2 mmap in `memory_init`, record either (a) a compact list of non-Available intervals overlapping the eventual managed range, or (b) enough Available intervals to mark free pages—prefer marking: after zeroing/`start_page` setup, mark all managed pages outside Available as occupied (and adjust `free_pages`). Store the snapshot in static storage or a small fixed array so it survives MBI overwrite.
- **Why:** MBI and `memory_map` collide near 1MiB; cannot re-walk MBI after `memory_map_init`.
- **Alternatives:** Keep a pointer into MBI only → unsafe after map init. Merge adjacent Available into one synthetic region without hole marks → allocator would hand out NVS pages.

### 3. Scope `ONIX_MAGIC` lightly

- **Choice:** Leave the ONIX loader “largest region” loop as-is if it already yields base=1MiB on supported machines; optionally share the max-end helper later. Do not block this change on unifying both paths.
- **Why:** Failure under test is Multiboot2/UEFI; minimize risk to BIOS workflow.

### 4. Implementation surface

- **Choice:** Almost exclusively `src/kernel/memory.c`. No change to `start.asm` call order; `memory_map_init` gains hole application after existing low-memory marking.
- **Why:** Behavior is local to physical memory init.

## Risks / Trade-offs

- **[Risk] Truncation / overflow when converting u64 mmap ends to u32** → Use u64 arithmetic; clamp managed end to 4GiB; skip or clip entries above 4GiB.
- **[Risk] Snapshot array too small for exotic maps** → Cap with a generous fixed limit; panic or truncate conservatively if exceeded (prefer panic in debug builds).
- **[Risk] free_pages drift if hole marking double-counts** → Only mark pages still free; or recount free pages after marking.
- **[Trade-off] Managed span includes holes as “occupied” rather than shrinking size** → Slightly larger `memory_map`; simpler than multiple disjoint free pools; fits current single-bitmap design.

## Migration Plan

- Rebuild USB image (`make usb-image`) and verify `make qemu-usb-xhci` progresses past `memory_init` (no assert spin at former line 117).
- Regression: existing `make qemu` / BIOS Multiboot or ONIX loader path still boots.
- Rollback: revert `memory.c` changes; no disk/format migration.

## Open Questions

- None that block implementation; optional later: unify `ONIX_MAGIC` onto the same max-end algorithm for consistency.
