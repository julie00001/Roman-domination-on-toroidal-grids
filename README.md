# Roman domination on toroidal grids - private review package

**For the editor and referees only. This package is not a public deposit.**

This package accompanies the current R3 manuscript. Start with
`manuscript/roman_torus_mpcR3.pdf` and `reviewer_guide.pdf`. The guide explains
the motivations and derivation details, the role of each computation, and
the limits of the resulting evidence. References to manuscript sections
and theorem numbers refer to R3.

## One-click reproduction

1. Extract the entire ZIP into a writable directory on Windows x64.
2. Double-click **`reproduce.cmd`**.
3. Open the new `runs/<timestamp>/logs/SUMMARY.txt`. A complete successful
   default run ends with **`FULL SUCCESS`** and exit code 0.

Prerequisites: Windows PowerShell 5.1 or PowerShell 7; Python 3.9+; and
either Visual Studio C++ Build Tools with the x64 C++ tools installed, or
x64 MinGW-w64 GCC. Python uses only its standard library. The launcher
detects suitable installed tools and explains any missing dependency.
There is no download, installation, upload or permanent environment change.
No external optimization solver is required. LaTeX is not needed to run
the computations; the manuscript and guide PDFs are included.

The default run builds all six C++ programs from source, recomputes all
7,984 exact rows, performs 126 direct full-state comparisons, reconstructs
four optimum witnesses, runs the finite formula and profile checks, checks
the all-length bridge certificate, and rebuilds Appendix D. It compares
all 1,246 printed cells and their annotations, plus the nine literature
comparison rows, with the included R3 source. The supplied reference files
and earlier runs are preserved.

To choose another empty output directory or a Python interpreter:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\reproduce.ps1 -OutputDirectory "runs\my-review" -Python "C:\path\python.exe"
```

`-ChecksOnly` is an optional shorter audit. It checks supplied exact data
and rebuilds Appendix D but **does not recompute the exact tables**. Its
summary explicitly distinguishes this mode from a full run. Use the
default mode for full reproduction. Exit code 1 indicates a failed stage;
exit code 2 indicates an incomplete bridge certificate. A resource limit
does not prove nonexistence. The guide gives the detailed interpretation.

## What is included

| Location | Purpose |
|---|---|
| `manuscript/` | The current R3 source and PDF, unchanged by this package revision. |
| `reviewer_guide.pdf`, `.tex` | Detailed English derivation and reproduction guide. |
| `reproduce.cmd`, `reproduce.ps1` | The one-click launcher and transparent orchestration. |
| `src/` | Six C++14 source programs; no precompiled executables are required. |
| `scripts/generate_bound_tables.py` | Rebuilds and checks R3 Appendix D with explicit input paths. |
| `reference/exact/` | One copy of eight exact tables, two direct tables and four witnesses. |
| `reference/tables/` | The four expected Appendix D artifacts, including source attribution. |
| `audit/` | Text records of the completed full run on the delivered sources. |
| `SHA256SUMS.txt` | SHA-256 hashes of the final delivered files, excluding the manifest itself. |

Generated binaries and object files appear only in a reviewer's new run
directory. They are not included in the delivered package. The original
archive, earlier submission, old bilingual supplements, duplicate merged
data, duplicate results and historical binary/log copies are not needed
for this version and have been omitted. The all-length bridge program
also covers the even terminal depths of the omitted duplicate program.

## Which results correspond to which parts of R3

| Computation | R3 location | Main run output |
|---|---|---|
| Unrestricted exact transfer solver | Appendix D, Tables 5-8 | `results/mM_exact.csv`, `logs/mM_exact.log` for M=3,...,10 |
| Direct calculation without symmetry or recurrence | Exact-data validation for Appendix D | `results/m7_full_check.csv`, `results/m8_full_check.csv` |
| Four reconstructed optimum RDFs | Construction/data validation | `results/witness_M_N.txt` for (7,23), (8,31), (9,17), (10,19) |
| Independent small-graph optimization and witness audit | Exact-data validation for Appendix D | `logs/verify_exact_results.log` |
| Scalar, construction, seam and energy checks | Sections 3,4,6,7,8; Appendices A.1,C | `logs/verify_manuscript.log` |
| Binary-delay identities and sharp sweeps | Appendix A.2, Theorem A.5 | `logs/verify_phase_barrier.log` |
| Width-12, budget-2 bridge certificate | Section 10, Algorithm 2 | `logs/bridge_all_lengths.log` |
| Specified normalized (12,8) profile | Appendix B, Theorem B.1 | `logs/check_type4_12_8.log` |
| Bound-table generation and comparison | Appendix D, Tables 4-8 | `tables/`, `logs/generate_bound_tables.log` |

The guide gives the test ranges and proof-to-code connection for each row.
The 7,984 exact rows comprise 177 direct values and 7,807 values extended
using checked **complete matrix identities**, not numerical trend fitting.
Only seven tiny graphs are independently optimized by exhaustive enumeration
of all center subsets. Bridge certificates, phase barriers and profile
checks retain their stated restrictions; they do not classify arbitrary
torus RDFs. General analytical theorems rely on the manuscript's proofs.

Appendix D uses the external literature formulas and OEIS snapshot already
identified in R3. Reproduction checks their encoded application; it does
not independently recompute the external ordinary domination numbers or
claim an exhaustive survey of all published bounds. The guide preserves
the source descriptions and explains the valid conversions.

Both `.tex` documents are standalone. To regenerate a document PDF, run
pdfLaTeX in its directory until references are stable. The numerical
reproduction does not require rebuilding either PDF.
