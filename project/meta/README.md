# Bruce v4 Project Map

Diese Mappe ist die **Quelle der Wahrheit** für Bruce v4.

## Ordner
- `axes/axes.json` – Axiome (Achsen + Skala)
- `cores/cores.json` – Core-Labels (nur Beobachter/CLI/Viewer)
- `states/` – Startzustände (JSON)
- `rules/rules.json` – Baseline-Regeln
- `changes/patches.jsonl` – Append-only Eventlog (Lernen/Neue Zustände)
- `logs/` – Exports (z.B. `brucev4 dump`)

## 2D-Viewer (nur Beobachter)

- `meta/viewer.json` – legt fest, welche zwei Achsen als 2D-Projektion exportiert werden (x/y) und welche Grid-Größe genutzt wird.
- Bruce-Core nutzt diese Datei **nicht**. Sie ist nur für `dump`/Visualisierung gedacht.
