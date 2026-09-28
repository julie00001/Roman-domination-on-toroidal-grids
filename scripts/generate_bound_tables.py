"""Rebuild and audit R3 Appendix D (Tables 4--8).

Python 3.9+, standard library only. No network access is required. Input data
are never changed. --data-dir must contain the eight unrestricted exact CSVs;
the one-click full run supplies freshly recomputed CSVs. The reference-only
audit instead supplies the retained reference CSVs and does not rerun the solver.
"""
from pathlib import Path
from fractions import Fraction
import csv
import hashlib
import json
import argparse
import re
import sys



def scalar(length, residue):
    return (0, 2 * length, (3 * length + 5 * (length % 2)) // 2,
            length + 5 * (length % 2),
            (4 * length + 5 * (length % 3)) // 3)[residue]


def projection(m, n):
    return (2 * m * n + max(scalar(n, -m % 5),
                           scalar(m, -n % 5)) + 4) // 5


def weight_per_column(m):
    return 2 * (m // 5) + (Fraction(0), Fraction(2, 3), Fraction(1),
                           Fraction(3, 2), Fraction(2))[m % 5]


def nearest_multiple_distance(value, period):
    """For remainder r, the distances to the neighboring multiples are r, p-r."""
    remainder = value % period
    return min(remainder, period - remainder)


def known_small_width(m, n):
    width, length = sorted((m, n))
    if width == 3:
        return (3 * length + 1) // 2
    if width == 4:
        return 2 * length
    if width == 5:
        return 2 * length + (0 if length % 5 == 0 else 2)
    if width == 6:
        extra = (0 if length % 6 == 0 else
                 1 if length % 6 == 5 or length % 18 in (3, 8, 13) else 2)
        return 8 * length // 3 + extra
    return None


def theoretical_bounds(m, n, include_literature=True):
    """Unrestricted bounds only; no phase-barrier or profile hypothesis."""
    lowers = [(projection(m, n), 'projection:Proposition3.3+Theorem3.2')]
    uppers = [((2 * m * n + 2 * n * (m % 5 != 0) +
                2 * m * (n % 5 != 0)) // 5, 'global:TheoremC.1')]
    for width, length in ((m, n), (n, m)):
        orient = f'width={width},length={length}'
        per_column = weight_per_column(width)
        uppers.append((int(per_column * length + 2 * width + 2),
                       f'slope:Theorem1.1({orient})'))
        residue = width % 5
        closes = ((residue == 0 and length % 5 == 0) or
                  (residue == 1 and length % (3 * width) == 0) or
                  (residue in (2, 3) and length % (2 * width) == 0) or
                  (residue == 4 and 2 * length % width == 0))
        if closes:
            value = per_column * length
            require(value.denominator == 1, 'nonintegral closing construction')
            lowers.append((int(value), f'closing:Section4({orient})'))
            uppers.append((int(value), f'closing:Section4({orient})'))
        if residue == 2 and width >= 7:
            coefficient = (2 * width + 1) // 5
            base = coefficient * length
            rem = length % (2 * width)
            extra = (0 if rem == 0 else
                     1 if rem in (5 % (2 * width), -5 % (2 * width)) else 2)
            lowers.append((base + extra,
                           f'rigidity+first-excess:Theorems1.2,8.2,9.1({orient})'))
            seam_extra = min(nearest_multiple_distance(coefficient * length, 2 * width),
                             coefficient)
            uppers.append((base + seam_extra,
                           f'phase:Theorem6.1({orient})'))
            k = (width - 2) // 5
            if length >= 2 * k + 7:
                sweep_extra = k + 1 + min(
                    nearest_multiple_distance(coefficient * (length - 2), 2 * width),
                    coefficient)
                uppers.append((base + min(seam_extra, sweep_extra),
                               f'sweep:TheoremA.2({orient})'))
    known = known_small_width(m, n)
    if known is not None:
        lowers.append((known, 'known-small-width:PZ12-Section5.4'))
        uppers.append((known, 'known-small-width:PZ12-Section5.4'))
    if include_literature:
        uppers.extend(literature_upper_bounds(m, n))
    low = max(value for value, _ in lowers)
    high = min(value for value, _ in uppers)
    require(low <= high, f'inconsistent theoretical bounds ({m},{n})')
    return low, high, ';'.join(tag for value, tag in lowers if value == low), \
        ';'.join(tag for value, tag in uppers if value == high)


def literature_upper_bounds(m, n):
    """Verified source statements; domination numbers give upper bounds only."""
    bounds = []
    for path, cycle in ((m, n), (n, m)):
        orientation = f'path={path},cycle={cycle}'
        # PZ12 Section 5.2: the cycle width is fixed.
        if cycle == 3:
            value = 3 * path // 2 + 1
        elif cycle == 4:
            value = 2 * path
        elif cycle == 5:
            value = 2 * path + 2
        elif cycle == 6:
            value = 8 * path // 3 + 2
        elif cycle == 7:
            value = 3 * path + (2 if path == 4 else 3)
        elif cycle == 8:
            value = 7 * path // 2 + (2 if path in (3, 4, 8) else 3)
        else:
            value = None
        if value is not None:
            bounds.append((value, f'literature:PZ12-Section5.2:close-path({orientation})'))
        # PZ12 Section 5.3: the path width is fixed.
        if path == 3:
            value = 5 if cycle == 3 else (3 * cycle + 1) // 2 + (cycle % 4 in (2, 3))
        elif path == 4:
            value = 7 if cycle == 3 else 2 * cycle
        elif path == 5:
            value = (12 * cycle + 4) // 5 + (cycle % 5 == 2)
        elif path == 6:
            value = 14 * cycle // 5 + (0 if cycle % 5 == 0 else 1 if cycle % 5 == 4 else 2)
        else:
            value = None
        if value is not None:
            bounds.append((value, f'literature:PZ12-Section5.3:close-path({orientation})'))
        # MGP21 Section IV and Corollary 12.
        if path == 7:
            value = (16 * cycle + 4) // 5 + (cycle % 5 != 0)
        elif path == 8:
            value = (18 * cycle + 4) // 5 + (0 if cycle % 5 == 0 else
                    1 if cycle % 5 in (2, 3, 4) or cycle == 6 else 2)
        elif path == 9:
            value = 4 * cycle + (0 if cycle % 5 == 0 else 2)
        elif path >= 10 and cycle >= 10 and cycle % 5 == 0:
            value = 2 * (path + 1) * cycle // 5
        else:
            value = None
        if value is not None:
            bounds.append((value, f'literature:MGP21-SectionIV+Corollary12:close-path({orientation})'))
    if m == n:
        # Theorem 4.3 concerns the cardinal product. For odd m it is
        # isomorphic to the Cartesian square via (x,y)->(x+y,x-y).
        if m >= 11 and m % 2:
            residue = m % 10
            if residue == 1:
                value = 2 * m * (m - 1) // 5 + m
            elif residue == 3:
                value = 2 * m * (m - 3) // 5 + 3 * m
            elif residue in (7, 9):
                value = 2 * m * (m - residue) // 5 + 4 * m
            else:
                value = 2 * m * m // 5
            bounds.append((value, 'literature:KP14-Theorem4.3-odd-square-isomorphism'))
        ordinary = {21: 95, 22: 104, 23: 114, 24: 120, 26: 144}
        if m in ordinary:
            bounds.append((2 * ordinary[m], 'literature:SXSW18-Theorems2-6:2gamma'))
    # Original numerical records of A094087, accessed 2026-09-21.
    # These are ordinary domination values, NOT Roman exact values.
    # A toroidal labeling can be repeated in each coordinate when its
    # base period divides both target lengths. No seam repair is needed.
    oeis = (1, 2, 3, 4, 5, 8, 12, 16, 18, 20, 27, 32, 38, 42, 45,
            56, 64, 71, 76, 80, 95, 104, 114, 120, 125, 144, 155,
            166, 174, 180)
    for width in range(3, len(oeis) + 1):
        if m % width == n % width == 0:
            bounds.append((2 * oeis[width - 1] * (m // width) * (n // width),
                           f'literature:OEIS-A094087-2026-09-21:2gamma+periodic-lift(base={width})'))
    return bounds


def require(condition, message):
    if not condition:
        raise ValueError(message)


def read_exact_data(data_dir):
    data, inputs = {}, []
    expected_fields = ['m', 'n', 'gamma_R', 'projection_lower_bound',
                       'excess_times_6', 'status']
    for width in range(3, 11):
        path = data_dir / f'm{width}_exact.csv'
        raw = path.read_bytes()
        seen, used_rows, statuses, canonical = set(), 0, {}, []
        with path.open(encoding='utf-8-sig', newline='') as stream:
            reader = csv.DictReader(stream)
            require(reader.fieldnames == expected_fields, f'{path.name}: incorrect header')
            for row in reader:
                m, n = int(row['m']), int(row['n'])
                require(m == width and n >= 3, f'{path.name}: invalid dimensions')
                require(n not in seen, f'{path.name}: duplicate n={n}')
                seen.add(n)
                value = int(row['gamma_R'])
                status = row['status']
                require(status in ('direct_exact', 'certified_recurrence'),
                        f'{path.name}: unrecognized status at n={n}')
                require(projection(m, n) == int(row['projection_lower_bound']),
                        f'{path.name}: projection mismatch at n={n}')
                require(6 * value - 6 * weight_per_column(m) * n == int(row['excess_times_6']),
                        f'{path.name}: excess mismatch at n={n}')
                canonical.append([row[field] for field in expected_fields])
                if n > 60:
                    continue
                used_rows += 1
                statuses[status] = statuses.get(status, 0) + 1
                key = tuple(sorted((m, n)))
                low, high, _, _ = theoretical_bounds(m, n)
                require(low <= value <= high, f'{path.name}: n={n} is outside theoretical bounds')
                source = f'{path.name}:n={n}'
                if key in data:
                    require(data[key]['value'] == value, f'{path.name}: transpose mismatch at n={n}')
                    data[key]['sources'].append(source)
                else:
                    data[key] = {'value': value, 'sources': [source]}
        require(set(range(3, 61)).issubset(seen), f'{path.name}: missing a length in 3..60')
        inputs.append({'file': path.name, 'sha256': hashlib.sha256(raw).hexdigest(),
                       'canonical_sha256': hashlib.sha256(json.dumps(sorted(canonical),
                          separators=(',', ':')).encode('utf-8')).hexdigest(),
                       'rows_in_file': len(seen), 'rows_used': used_rows,
                       'used_status_counts': statuses})
    return data, inputs


def make_rows(data_dir):
    data, inputs = read_exact_data(data_dir)
    rows = []
    for m in range(3, 31):
        for n in range(m, 61):
            low, high, lower_sources, upper_sources = theoretical_bounds(m, n)
            exact = data.get(tuple(sorted((m, n))))
            theoretical_low, theoretical_high = low, high
            previous_low, previous_high, _, _ = theoretical_bounds(m, n, False)
            if exact:
                require(low <= exact['value'] <= high, 'exact value outside bounds')
                low = high = exact['value']
                previous_low = previous_high = exact['value']
            kind = ('interval' if low < high else
                    'exact_theoretical' if theoretical_low == theoretical_high
                    else 'exact_computational')
            rows.append(dict(m=m, n=n, lower=low, upper=high,
                             display=str(low) if low == high else f'{low}-{high}',
                             kind=kind, theoretical_lower=theoretical_low,
                             theoretical_upper=theoretical_high,
                             theoretical_lower_sources=lower_sources,
                             theoretical_upper_sources=upper_sources,
                             previous_lower=previous_low, previous_upper=previous_high,
                             literature_improved=(low > previous_low or high < previous_high),
                             literature_upper_candidates=';'.join(
                                 f'{tag}={value}' for value, tag in literature_upper_bounds(m, n)),
                             exact_csv_sources=';'.join(exact['sources']) if exact else ''))
    require(len(rows) == 1246, 'incorrect row count')
    keyed = {(row['m'], row['n']): row for row in rows}
    for row in rows:
        swapped = theoretical_bounds(row['n'], row['m'])
        require((row['theoretical_lower'], row['theoretical_upper']) == swapped[:2], 'orientation mismatch')
        opposite = keyed.get((row['n'], row['m']))
        if opposite:
            for field in ('lower', 'upper', 'kind', 'theoretical_lower', 'theoretical_upper'):
                require(row[field] == opposite[field], f'transpose mismatch in {field}')
    return rows, inputs


def tables_tex(rows):
    keyed = {(row['m'], row['n']): row for row in rows}
    chunks = [r'% Generated by scripts/generate_bound_tables.py; current manuscript R3 Appendix D.',
              r'% A star identifies exact CSV values that sharpen the stated theoretical bounds.',
              r'\begingroup', r'\small', r'\setlength{\tabcolsep}{5pt}',
              r'\renewcommand{\arraystretch}{1.08}',
              r'\setlength{\LTpre}{8pt}', r'\setlength{\LTpost}{10pt}',
              r'\setlength{\LTcapwidth}{\textwidth}']
    for group in range(4):
        widths = list(range(3 + 7 * group, 10 + 7 * group))
        header = r'\(n\backslash m\)&' + '&'.join(r'\(' + str(m) + r'\)' for m in widths) + r'\\'
        caption = (r'\caption{Values and bounds for \(m=' + str(widths[0]) +
                   r',\ldots,' + str(widths[-1]) + r'\), \(m\le n\le60\). '
                   r'A single number is exact; \(a\text{--}b\) means '
                   r'\(a\le\gamma_R(C_m\sq C_n)\le b\). '
                   r'\(^{*}\) marks a computational refinement; '
                   r'\(^{\dagger}\) marks a literature improvement (see the source discussion above).}'
                   r'\label{tab:finite-bounds-' + str(group + 1) + r'}\\')
        chunks.extend([r'\begin{longtable}{@{}r*{7}{r}@{}}', caption,
                       r'\toprule', header, r'\midrule', r'\endfirsthead',
                       r'\multicolumn{8}{c}{\tablename\ \thetable\ (continued)}\\',
                       r'\toprule', header, r'\midrule', r'\endhead',
                       r'\midrule', r'\multicolumn{8}{r}{Continued on the next page}\\',
                       r'\endfoot', r'\bottomrule', r'\endlastfoot'])
        for n in range(widths[0], 61):
            values = []
            for m in widths:
                if m > n:
                    values.append('')
                    continue
                row = keyed[m, n]
                if row['lower'] == row['upper']:
                    value = str(row['lower'])
                    if row['kind'] == 'exact_computational':
                        value += r'^{*}'
                    if row['literature_improved']:
                        value += r'^{\dagger}'
                    values.append(r'\(' + value + r'\)')
                else:
                    value = f"{row['lower']}--{row['upper']}"
                    if row['literature_improved']:
                        value += r'\(^{\dagger}\)'
                    values.append(value)
            chunks.append(str(n) + '&' + '&'.join(values) + r'\\')
        chunks.append(r'\end{longtable}')
    chunks.append(r'\endgroup')
    return '\n'.join(chunks) + '\n'


def compare_manuscript(rows, manuscript):
    """Compare all 1,246 printed cells, stars, daggers and the nine Table 4 rows."""
    source = manuscript.read_text(encoding='utf-8-sig')
    keyed = {(row['m'], row['n']): row for row in rows}
    cells = 0
    for group in range(4):
        label = r'\label{tab:finite-bounds-' + str(group + 1) + '}'
        require(source.count(label) == 1, f'missing or duplicate manuscript label {label}')
        block = source.split(label, 1)[1].split(r'\end{longtable}', 1)[0]
        parsed = {}
        for line in block.splitlines():
            if not re.match(r'^\s*\d+&', line):
                continue
            fields = line.strip().rstrip(chr(92)).split('&')
            n = int(fields[0])
            require(len(fields) == 8 and n not in parsed, f'malformed table row n={n}')
            parsed[n] = fields[1:]
        widths = list(range(3 + 7 * group, 10 + 7 * group))
        require(set(parsed) == set(range(widths[0], 61)), f'incomplete Table {5 + group}')
        for n, printed in parsed.items():
            for m, cell in zip(widths, printed):
                cell = cell.replace(r'\(', '').replace(r'\)', '').strip()
                if m > n:
                    require(cell == '', f'nonblank omitted cell ({m},{n})')
                    continue
                row = keyed[m, n]
                expected = str(row['lower']) if row['lower'] == row['upper'] else \
                    f"{row['lower']}--{row['upper']}"
                if row['kind'] == 'exact_computational':
                    expected += r'^{*}'
                if row['literature_improved']:
                    expected += r'^{\dagger}'
                require(cell == expected, f'manuscript mismatch ({m},{n}): {cell!r} != {expected!r}')
                cells += 1
    block = source.split(r'\label{tab:literature-improvements}', 1)[1].split(r'\end{table}', 1)[0]
    printed = []
    for line in block.splitlines():
        match = re.match(r'^\\\(\((\d+),(\d+)\)\\\)&(\d+)--(\d+)&(\d+)--(\d+)&', line.strip())
        if match:
            printed.append(tuple(int(v) for v in match.groups()))
    expected = [(row['m'], row['n'], row['previous_lower'], row['previous_upper'],
                 row['lower'], row['upper']) for row in rows if row['literature_improved']]
    require(printed == expected, 'Table 4 literature-improvement rows differ from generated results')
    require(cells == 1246, 'incorrect number of manuscript cells checked')
    return {'table_numbers': [4, 5, 6, 7, 8], 'finite_cells_checked': cells,
            'literature_rows_checked': len(printed), 'all_values_and_markers_match': True}


def compare_reference(output_dir, reference_dir):
    # Compare numerical/provenance fields and LaTeX, ignoring newline style only.
    for name in ('bounds_m3_30_n3_60.csv', 'literature_comparison.csv'):
        tables = []
        for directory in (output_dir, reference_dir):
            with (directory / name).open(encoding='utf-8-sig', newline='') as stream:
                tables.append(list(csv.DictReader(stream)))
        require(tables[0] == tables[1], f'reference comparison failed: {name}')
    name = 'bounds_longtables.tex'
    require((output_dir / name).read_text(encoding='utf-8') ==
            (reference_dir / name).read_text(encoding='utf-8'),
            f'reference comparison failed: {name}')
    generated = json.loads((output_dir / 'bounds_audit.json').read_text(encoding='utf-8'))
    reference = json.loads((reference_dir / 'bounds_audit.json').read_text(encoding='utf-8'))
    for key in ('parameters', 'm', 'n', 'constraint', 'counts', 'interval_gap_max',
                'literature_improvements', 'all_overlapping_orientations_agree',
                'all_exact_inputs_within_theoretical_bounds', 'manuscript_check'):
        require(generated[key] == reference[key], f'reference audit mismatch: {key}')
    require([(item['file'], item['canonical_sha256']) for item in generated['inputs']] ==
            [(item['file'], item['canonical_sha256']) for item in reference['inputs']],
            'reference input data differ in numerical fields or statuses')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data-dir', type=Path, required=True,
                        help='directory containing m3_exact.csv through m10_exact.csv')
    parser.add_argument('--output-dir', type=Path, required=True,
                        help='directory for the four regenerated Appendix D files')
    parser.add_argument('--manuscript', type=Path, required=True,
                        help='R3 LaTeX source whose embedded Tables 4--8 will be checked')
    parser.add_argument('--reference-dir', type=Path,
                        help='optional reference/tables directory for semantic comparison')
    args = parser.parse_args()
    require(args.data_dir.resolve() != args.output_dir.resolve(), 'input and output directories must differ')
    if args.reference_dir:
        require(args.reference_dir.resolve() != args.output_dir.resolve(), 'reference and output directories must differ')
    rows, inputs = make_rows(args.data_dir)
    manuscript_check = compare_manuscript(rows, args.manuscript)
    tables = args.output_dir
    tables.mkdir(parents=True, exist_ok=True)
    with (tables / 'bounds_m3_30_n3_60.csv').open('w', encoding='utf-8', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    (tables / 'bounds_longtables.tex').write_text(tables_tex(rows), encoding='utf-8')
    comparison_fields = ['m', 'n', 'previous_lower', 'previous_upper', 'lower',
                         'upper', 'literature_improved', 'literature_upper_candidates',
                         'theoretical_upper_sources']
    with (tables / 'literature_comparison.csv').open('w', encoding='utf-8', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=comparison_fields, extrasaction='ignore')
        writer.writeheader()
        writer.writerows(rows)
    summary = dict(parameters=1246, m=[3, 30], n=[3, 60], constraint='m <= n',
                   counts={kind: sum(row['kind'] == kind for row in rows)
                           for kind in ('exact_theoretical', 'exact_computational', 'interval')},
                   interval_gap_max=max(row['upper'] - row['lower'] for row in rows),
                   all_overlapping_orientations_agree=True,
                   all_exact_inputs_within_theoretical_bounds=True,
                   literature_improvements=sum(row['literature_improved'] for row in rows),
                   manuscript_check=manuscript_check,
                   manuscript_sha256=hashlib.sha256(args.manuscript.read_bytes()).hexdigest(),
                   inputs=inputs,
                   external_sources_note='Literature formulas and OEIS snapshot are fixed inputs; '
                       'the reproduction does not independently prove those external results.')
    (tables / 'bounds_audit.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    if args.reference_dir:
        compare_reference(tables, args.reference_dir)
    print(json.dumps({key: value for key, value in summary.items()
                      if key not in ('inputs', 'manuscript_sha256')}, indent=2))
    print('PASS: Appendix D Tables 4--8 match the current R3 source.' +
          (' Reference table comparison passed.' if args.reference_dir else ''))


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, IndexError, AssertionError) as error:
        print(f'ERROR: {error}', file=sys.stderr)
        sys.exit(1)
