# Completed full reproduction

This directory records the full default run from the delivered package on
28 September 2026. Read `SUMMARY.txt` first. The run returned exit code 0,
with `FULL SUCCESS`, in 121.53 seconds on the recorded machine. This time
is an observation, not a runtime guarantee for other machines.

The run compiled all six source programs; recomputed 7,984 exact rows;
checked 126 full-state direct rows and four witnesses; ran the finite
mathematical checks; obtained the width-12 budget-2 complete bridge-map
certificate; and rebuilt and compared all 1,246 R3 table cells and the
nine literature-summary rows. The program-specific logs preserve the
certificate identities and the exact test counts. `input_sha256.txt`
identifies the inputs actually used.

Only local filesystem prefixes were replaced by `<PACKAGE>`, `<RUN>` and
`<PYTHON>` in these copied text records. Numerical output, commands' results,
compiler version, timing and statuses were not changed. The generated
binaries, objects and duplicate result CSVs are omitted: a new one-click
run recreates them in its own directory.

The source archive supplied by the author was
`roman_torus_submission_and_review_materials.zip` (21 September 2026).
Its SHA-256 is `a013eb257b76e77cb5e67765eb21a302aed34c6bc71f07ca8ab52f93d1db0c7b`.
The six retained C++ algorithm files and exact reference files are byte-for-byte
copies from that archive. The current R3 manuscript is the later manuscript
provided for this review; the launcher, table support and guide were updated
to match it. This package is for private editorial and referee use.
