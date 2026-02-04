#pragma once

namespace bruce::observer {

inline const char* usage_text = R"(Bruce v4 (C++) — Deterministischer semantischer Rechenkern

Usage:
  brucev4 --project <path> info
  brucev4 --project <path> validate
  brucev4 --project <path> list-states
  brucev4 --project <path> show-state <state_id>
  brucev4 --project <path> mix <state_id_A> <state_id_B>
  brucev4 --project <path> distance <state_id_A> <state_id_B>
  brucev4 --project <path> allowed <from_state_id> <to_state_id>
  brucev4 --project <path> new-state <core_id> <v1> <v2> ... <vN>
  brucev4 --project <path> learn-add-rule <name> <d1> <d2> ... <dN>
  brucev4 --project <path> dataset fields-check
  brucev4 --project <path> fields list
  brucev4 --project <path> fields build <field_id>
  brucev4 --project <path> dump --out <backup.json>

Notes:
  - Der Core ist deterministisch; JSON ist nur Export/Log.
  - `dump` enthält zusätzlich Projektionen/Fields/BR2 und Snapshot-Infos sowie eine 2D-Projektion (`view2d`) und erklärbare Übergänge (`allowed`).
  - States sind immutable; `new-state` und `learn-add-rule` hängen Events in `project/changes/patches.jsonl` an.
  - Alle Vektorwerte werden beim Import und bei `new-state` geclamped.
)";

} // namespace bruce::observer
