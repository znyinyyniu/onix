## 1. Multiboot2 range selection

- [x] 1.1 In `memory_init` Multiboot2 branch, set `memory_base = MEMORY_BASE` and compute `memory_size` from the maximum Available region end above 1MiB (u64 math, clamp to 4GiB); remove longest-region selection
- [x] 1.2 Keep `assert(memory_base == MEMORY_BASE)`, page-alignment check, `total_pages`/`free_pages` formulas, and `KERNEL_MEMORY_SIZE` panic unchanged in meaning

## 2. Availability snapshot and hole marking

- [x] 2.1 While walking Multiboot2 mmap in `memory_init`, snapshot Available intervals (or equivalent) into static storage before `memory_map` can overwrite the MBI
- [x] 2.2 In `memory_map_init` (after existing low-memory / map-page marking), mark managed-range pages not covered by snapshotted Available as occupied and adjust `free_pages` without double-counting
- [x] 2.3 Leave `ONIX_MAGIC` path behavior intact (no required algorithm change in this change)

## 3. Verification

- [x] 3.1 Rebuild with `make usb-image` and run `make qemu-usb-xhci`; confirm guest no longer spins in `assertion_failure` / `memory_init` base assert (progresses past memory init)
- [x] 3.2 Smoke-check BIOS/`make qemu` (or equivalent ONIX loader path used in this repo) still boots past memory initialization
