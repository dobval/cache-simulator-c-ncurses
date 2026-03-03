# Cache Simulator

A command-line and interactive cache simulator demonstrating multi-level cache hierarchies, eviction policies, and write strategies.

## Project Architecture

The project follows a **layered architecture** pattern with clear separation of concerns:

```
+-------------------+
|   Application    |  <- main.c (CLI parsing, event loop)
+-------------------+
         |
+-------------------+
|  Presentation    |  <- ui.c/h (ncurses TUI)
+-------------------+
         |
+-------------------+
|  Test Data       |  <- trace.c/h (trace generation)
+-------------------+
         |
+-------------------+
|    Domain        |  <- cache.c/h (core simulation logic)
+-------------------+
```

### Layer Responsibilities

| Layer | File(s) | Responsibility |
|-------|---------|----------------|
| **Domain** | `cache.c`, `cache.h` | Cache simulation engine: hit/miss logic, eviction algorithms, address decomposition |
| **Test Data** | `trace.c`, `trace.h` | Memory access trace generation (Loop, Random, Sequential patterns) |
| **Presentation** | `ui.c`, `ui.h` | Terminal UI using ncurses: menus, input handling, statistics display |
| **Application** | `main.c` | Entry point: CLI argument parsing, mode routing (CLI vs TUI), event loop |

## File Descriptions

### cache.h / cache.c - Domain Layer
**Purpose:** Core cache simulation logic

**Key Components:**
- `Cache` struct - Individual cache level (sets × associativity lines)
- `CacheSystem` struct - L1 + optional L2 cache hierarchy
- `CacheStats` struct - Performance metrics (hits, misses, evictions, memory traffic)

**Key Functions:**
- `cache_init()` - Initialize cache with configurable sets, associativity, block size
- `cache_access()` - Process memory access, return hit/miss/eviction result
- `cache_get_tag()`, `cache_get_set_index()`, `cache_get_block_offset()` - Address bit decomposition
- `cache_system_init/access/free/clear()` - Multi-level cache orchestration

**Eviction Policies Implemented:**
- **LRU** (Least Recently Used) - Tracks access order via counter
- **FIFO** (First In First Out) - Tracks insertion order via counter
- **LFU** (Least Frequently Used) - Tracks access frequency
- **RANDOM** - Random eviction (for comparison baseline)

**Write Policies:**
- **Write-Through** - Write to cache and memory simultaneously
- **Write-Back** - Write to cache only, write to memory on eviction (dirty bit tracking)

---

### trace.h / trace.c - Test Data Layer
**Purpose:** Generate memory access traces for simulation

**Key Components:**
- `TraceType` enum - Available trace patterns
- `TraceGenerator` struct - Stateful trace generation
- `TraceReader` struct - File-based trace input (for future extension)

**Trace Patterns:**
- **Loop** - Repeated access to same 8 cache lines (temporal locality)
- **Random** - Pseudo-random addresses (no locality)
- **Sequential** - Strided access pattern (spatial locality, configurable stride)

---

### ui.h / ui.c - Presentation Layer
**Purpose:** Interactive terminal user interface using ncurses

**Key Components:**
- `UIState` struct - All configurable parameters + UI state
- `UIMode` enum - CONFIG vs RESULTS display modes

**Key Functions:**
- `ui_init()` - Initialize ncurses, set defaults
- `ui_draw()` - Render all panels
- `ui_handle_input()` - Process keyboard input

**UI Panels:**
1. **Main Border** - Title bar with mode indicator
2. **Config Panel** - L1/L2 cache settings, trace selection, run controls
3. **Stats Panel** - Hit/miss/eviction statistics (results mode only)

---

### main.c - Application Layer
**Purpose:** Application entry point with dual-mode operation

**Key Components:**
- `CLIArgs` struct - Command-line argument storage
- CLI argument parser with validation

**Dual-Mode Operation:**
1. **Interactive Mode** (default) - Full ncurses TUI with menus and navigation
2. **CLI Mode** (with arguments) - Batch simulation for scripting/testing

---

## Features

### Cache Hierarchy
- Configurable L1 cache (mandatory)
- Configurable L2 cache (optional, enableable)
- Independent policy selection per level

### Configurable Parameters
| Parameter | Range | Default |
|-----------|-------|---------|
| L1 Sets | 1-256 | 32 |
| L1 Associativity | 1-16 | 4 |
| L1 Block Size | 4-128 bytes | 64 |
| L2 Sets | 1-256 | 128 |
| L2 Associativity | 1-16 | 8 |
| L2 Block Size | 4-128 bytes | 64 |
| Access Count | 100-10000 | 1000 |
| Stride | 4-1024 | 4 |

### Eviction Policies
- LRU (Least Recently Used)
- FIFO (First In First Out)
- LFU (Least Frequently Used)
- RANDOM (Random Eviction)

### Write Policies
- Write-Through (write to cache and memory)
- Write-Back (write to cache, defer memory write)

### Trace Patterns
- **Loop** - Repeated same addresses (temporal locality, high hit rate)
- **Random** - Pseudo-random addresses (no locality, low hit rate)
- **Sequential** - Strided pattern (spatial locality, depends on stride)

---

## Building

### Prerequisites
- GCC compiler
- ncurses library

### Build Commands
```bash
make          # Build the simulator
make clean    # Remove build artifacts
make run      # Build and run
```

### Build Output
- Executable: `cache_sim`
- Object files: `src/*.o`

---

## Usage

### Interactive Mode (Default)
Run without arguments to launch the TUI:
```bash
./cache_sim
```

**Controls:**
| Key | Action |
|-----|--------|
| `↑` / `↓` | Navigate fields |
| `+` / `-` | Increase/decrease value |
| `→` / `←` | Increase/decrease value |
| `1` - `3` | Select trace type |
| `Enter` | Run simulation |
| `Enter` (in results) | Back to config |
| `Q` | Quit |

### Command-Line Mode
Run with arguments for batch simulation:
```bash
# Basic usage
./cache_sim --trace loop --count 1000

# With L2 cache
./cache_sim --trace sequential --enable-l2 --count 500

# Custom policies
./cache_sim --l1-policy FIFO --l2-policy LFU --write-policy WT

# Full example
./cache_sim --l1-sets 64 --l1-assoc 8 --l1-block 128 \
  --enable-l2 --l2-sets 256 --l2-assoc 4 \
  --trace random --count 10000
```

### CLI Options
```
--help              Show help message
--l1-sets N         L1 sets (default: 32)
--l1-assoc N        L1 associativity (default: 4)
--l1-block N        L1 block size (default: 64)
--l1-policy P       L1 policy: LRU, FIFO, LFU, RANDOM
--write-policy P    Write policy: WT, WB
--enable-l2         Enable L2 cache
--l2-sets N         L2 sets (default: 128)
--l2-assoc N        L2 associativity (default: 8)
--l2-block N        L2 block size (default: 64)
--l2-policy P       L2 policy: LRU, FIFO, LFU, RANDOM
--trace T          Trace: loop, random, sequential
--count N          Accesses (default: 1000)
--stride N         Stride (default: 4)
```

---

## Implementation Notes

### Address Bit Decomposition
The simulator decomposes memory addresses into:
- **Tag bits** - Remaining bits after offset and index
- **Set index bits** - `log2(number of sets)`
- **Block offset bits** - `log2(block size)`

Example: 32 sets, 4-way, 64-byte blocks:
- Block offset: 6 bits (64 = 2^6)
- Set index: 5 bits (32 = 2^5)
- Tag: 21 bits (32 - 6 - 5)

### L2 Cache Behavior
When L2 is enabled:
- L1 miss triggers L2 lookup
- L2 hit reduces memory traffic
- L2 miss triggers memory read
- Evicted L1 lines can optionally be placed in L2 (not implemented - simplified model)

### Write-Back Memory Traffic
Memory writes occur:
- On eviction of dirty lines (Write-Back only)
- Immediately on store (Write-Through only)
