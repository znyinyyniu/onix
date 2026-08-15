# physical-memory Specification

## Purpose

Defines how Onix discovers usable physical RAM from the bootloader memory map and initializes the page occupancy table so allocation never treats firmware holes as free memory.

## Requirements

### Requirement: Multiboot2 usable range starts at MEMORY_BASE

When entered via Multiboot2, the kernel MUST set the managed usable physical base to `MEMORY_BASE` (1MiB). It MUST derive `memory_size` as the span from `MEMORY_BASE` to the highest ending address among Available regions that contribute usable memory below 4GiB (regions ending at or below `MEMORY_BASE` MUST be ignored for this end calculation). The kernel MUST NOT select an Available region's base solely because that region has the largest length.

#### Scenario: UEFI map with split Available regions

- **WHEN** Multiboot2 provides an Available region starting at 1MiB that is smaller than a later Available region starting above 1MiB (as on typical OVMF maps)
- **THEN** the managed base remains 1MiB and `memory_size` reaches at least the end of the higher Available region (modulo 4GiB clipping), and the `memory_base == MEMORY_BASE` invariant holds

#### Scenario: Insufficient usable span

- **WHEN** the derived `memory_size` is less than `KERNEL_MEMORY_SIZE`
- **THEN** the kernel MUST panic (or otherwise halt with a clear failure) and MUST NOT continue with a undersized managed range

### Requirement: Non-available pages are not allocatable

After the physical page occupancy table is initialized for the managed range `[MEMORY_BASE, MEMORY_BASE + memory_size)`, every page in that range that is not covered by an Available Multiboot2 mmap entry MUST be marked occupied so the physical page allocator cannot hand it out. Available coverage that falls inside the managed range MUST remain free except for pages already reserved by existing low-memory and occupancy-table placement rules.

#### Scenario: NVS or reserved hole inside the managed span

- **WHEN** the Multiboot2 mmap contains NVS, Reserved, or other non-Available types overlapping the managed span between 1MiB and the derived end
- **THEN** those overlapping pages MUST be marked occupied before normal allocation begins

#### Scenario: Gap with no mmap coverage

- **WHEN** an address range inside the managed span is not covered by any Available entry
- **THEN** pages in that gap MUST be treated as occupied for allocation purposes

### Requirement: Bootloader paths remain bootable

The ONIX loader (`ONIX_MAGIC`) path MUST continue to initialize physical memory successfully on configurations that previously worked (contiguous Available-from-1MiB style maps). Multiboot2 changes MUST NOT require removing the `memory_base == MEMORY_BASE` invariant.

#### Scenario: Legacy contiguous map

- **WHEN** the memory map presents a single large Available region starting at 1MiB (typical BIOS / ONIX loader case)
- **THEN** memory initialization succeeds with base 1MiB and a `memory_size` sufficient for `KERNEL_MEMORY_SIZE`
