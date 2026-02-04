# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Bruce v6 is a deterministic semantic computation engine (German: "Deterministischer semantischer Rechenkern") implemented in modern C++20. It provides state transition validation, rule-based decision making, and semantic analysis using multidimensional vector spaces. The core is fully deterministic with no external dependencies.

**Key Design Principles:**
- Determinism: All operations are reproducible
- Immutability: States are immutable; changes append to event log
- Observer Pattern: JSON/visualization is observer-only, not used by core logic
- Fixed-Point Arithmetic: Uses `int16_t` (val_t) for vector values
- Monotonic IDs: States have strictly increasing IDs for audit trail
- Zero External Dependencies: Pure C++ standard library implementation

## Build Commands

```bash
# Build (Visual Studio with Ninja)
cmake -S . -B out/build/x64-Debug -G Ninja
cmake --build out/build/x64-Debug

# Build Release
cmake -S . -B out/build/x64-Release -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build out/build/x64-Release

# Run tests
./out/build/x64-Debug/test_core.exe
./out/build/x64-Debug/test_v6.exe

# Run benchmarks
./out/build/x64-Release/bench_mix_v4.exe --test allowed --dims 7 --loops 200000 --runs 50 --warmup 5 --mode worst

# Run benchmark suite with statistics
python benchmark.py
```

**Build Targets:**
- `bruce_core`: Core library (axes, ops, rules, state)
- `bruce_io`: I/O library (JSON parsing, project loading, export)
- `brucev4`: CLI executable
- `test_core`: Unit tests for core operations
- `test_v6`: Unit tests for decision engine
- `bench_mix_v4`: Performance benchmarks

## CLI Usage

```bash
# Project must be specified with --project flag
brucev4 --project ./project info
brucev4 --project ./project validate
brucev4 --project ./project list-states
brucev4 --project ./project show-state <state_id>

# Vector operations
brucev4 --project ./project mix <state_id_A> <state_id_B>
brucev4 --project ./project distance <state_id_A> <state_id_B>

# Rule validation
brucev4 --project ./project allowed <from_state_id> <to_state_id>

# State and rule management
brucev4 --project ./project new-state <core_id> <v1> <v2> ... <vN>
brucev4 --project ./project learn-add-rule <name> <d1> <d2> ... <dN>

# Fields and projections
brucev4 --project ./project dataset fields-check
brucev4 --project ./project fields list
brucev4 --project ./project fields build <field_id>

# Export full snapshot
brucev4 --project ./project dump --out backup.json
```

## Architecture

### Type System (`include/bruce/types.hpp`)

```cpp
core_id_t  = uint32_t  // Group identifier for states
dim_t      = uint32_t  // Dimension index
val_t      = int16_t   // Fixed-point vector values
state_id_t = uint64_t  // Monotonic state identifier
rule_id_t  = uint64_t  // Rule identifier
```

### Core Modules

**Axes Registry** (`include/bruce/core/axes/`):
- Defines N-dimensional space with per-axis bounds (min/max)
- Human-readable labels (observer-only)
- Clamping functions ensure values stay within bounds

**State & StateStore** (`include/bruce/core/state/`):
- State: Immutable vector of `val_t` values + core_id + version_tag
- StateStore: Monotonically increasing ID assignment
- States are never modified; new states are created with new IDs

**Rules & RuleSet** (`include/bruce/core/rules/`):
- Rule: Per-axis delta constraints (max_delta per dimension)
- RuleSet: Collection of rules that validate state transitions
- `allows(from, to)`: Returns true if transition complies with any rule

**Vector Operations** (`include/bruce/core/ops/`):
- `mix_into()`: Average two vectors element-wise (with clamping)
- `l1_distance()`: Manhattan distance between states
- `collapse_to_neutral()`: Entropy/neutrality score

**Decision Engine** (`include/bruce/core/v6/`):
- DecisionEngine: High-level API for evaluating state transitions
- Guard predicates: AllowedAny, DistanceAtLeast, AxisRange
- Decision outcomes: REJECT, THROTTLE, ACCEPT, ISOLATE
- Version consistency checks prevent mixing incompatible states

### I/O Layer (`src/io_*.cpp`, `include/bruce/io/`)

**Project Structure:**
- `axes/axes.json`: Axis definitions with bounds and labels
- `states/*.json`: Initial state definitions
- `rules/rules.json`: Baseline transition rules
- `changes/patches.jsonl`: Append-only event log (new states/rules)
- `fields/`: Field definitions for 2D projections
- `projections/`: Weighted axis combinations for visualization
- `meta/viewer.json`: 2D viewer configuration (observer-only)
- `logs/`: Export/dump outputs

**JSON Handling:**
- Custom `minijson` parser (no external dependencies)
- JSON schemas in `io/schema/` validate all project files
- Project loader reads axes, states, rules from filesystem
- Export functions dump full project state to single JSON file

## Key Implementation Details

**Deterministic Core:**
- All state transitions are reproducible
- JSON is only for observers/CLI/viewer
- Core operations never read JSON directly
- Version tags ensure compatibility

**Immutability & Event Sourcing:**
- States cannot be modified after creation
- `new-state` and `learn-add-rule` append to `patches.jsonl`
- Event log provides full audit trail
- StateStore assigns monotonically increasing IDs

**Clamping:**
- All vector values are clamped to axis bounds on import
- `new-state` command clamps values before creating state
- Mix operation clamps results to valid range

**2D Projections (Observer-Only):**
- `meta/viewer.json` specifies which axes map to x/y
- Core never uses this file; it's purely for visualization
- `dump` command includes 2D projection data for observers

## Example Projects

**Main Project** (`project/`):
- 7-dimensional "life physics" axes:
  - analyse ↔ execution
  - abstract ↔ concrete
  - emotional ↔ rational
  - social ↔ technical
  - unstable ↔ stable
  - risky ↔ safe
  - slow ↔ fast

**Security Platform Demo** (`demo/security_platform_v1/`):
- 8-dimensional security model:
  - untrusted ↔ trusted
  - weak_auth ↔ strong_auth
  - corrupt_policy ↔ valid_policy
  - low_load ↔ high_load
  - benign ↔ anomalous
  - contained ↔ wide_blast
  - low_risk ↔ high_risk
  - noncompliant ↔ compliant

## Testing

**Unit Tests:**
- `test_core`: Tests vector operations (mix, distance, collapse)
- `test_v6`: Tests decision engine and guard predicates
- Tests are minimal C++ programs with exit(0) on success

**Benchmarks:**
- `bench_mix_v4`: Comprehensive performance suite
  - Tests: mix, allowed, nearest, e2e, stress
  - Args: `--test`, `--dims`, `--loops`, `--runs`, `--warmup`, `--mode`
  - Reports timing in microseconds (median of runs)
- `benchmark.py`: Python wrapper for collecting statistics (median, mean, P95)

## Code Organization

```
src/           - Implementation files (.cpp)
include/bruce/ - Header files (.hpp)
  core/        - Core computation engine
    axes/      - Axis registry
    ops/       - Vector operations
    rules/     - Rule definitions and validation
    state/     - State representation and storage
    transitions/ - Transition structures
    v6/        - Decision engine
  io/          - Project loading and JSON export
  observer/    - CLI interface and usage help
  util/        - Utilities (SHA-256)
tests/         - Unit tests
bench/         - Performance benchmarks
io/schema/     - JSON schemas for validation
project/       - Main project workspace
demo/          - Example projects
```

## Important Notes

- **Language**: C++20 required (uses std::span, concepts)
- **Compiler**: MSVC preferred (CMakeSettings.json configured for Visual Studio)
- **No External Deps**: Self-contained implementation
- **Fixed-Point**: All vector values are int16_t (fixed-point representation)
- **German Comments**: Some inline comments and READMEs are in German
- **Event Log**: `patches.jsonl` is append-only; never truncate or modify
- **Schema Validation**: All JSON files validated against schemas in `io/schema/`
