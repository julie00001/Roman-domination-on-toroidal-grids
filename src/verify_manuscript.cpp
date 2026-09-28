// Reproducible finite checks for roman_torus_manuscript.tex.
// C++14, standard library only. No Python runtime or external libraries.
// These exact-integer and vertex-level tests supplement the manuscript;
// finite tests do NOT prove statements for unbounded graph sizes.
// All checks remain enabled when compiled with -DNDEBUG or /DNDEBUG.

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using Integer = long long;
using Labels = std::vector<std::vector<int>>; // labels[column][row]
using Matrix = std::array<std::array<Integer, 16>, 16>;
constexpr Integer INF = 1000000000000LL;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

int mod(int value, int modulus) {
    const int result = value % modulus;
    return result < 0 ? result + modulus : result;
}

Integer psi(Integer value) {
    return value >= 0 ? 2 * value : -3 * value;
}

Integer expected(int length, int residue) {
    switch (residue) {
    case 0: return 0;
    case 1: return 2LL * length;
    case 2: return (3LL * length + 5 * (length % 2)) / 2;
    case 3: return length + 5 * (length % 2);
    case 4: return (4LL * length + 5 * (length % 3)) / 3;
    default: throw std::runtime_error("invalid scalar residue");
    }
}

Matrix scalar_matrix(int residue) {
    Matrix result;
    for (auto& row : result) row.fill(INF);
    // The ordered pair (x,y), with x,y in {-2,-1,0,1}, has this index.
    const auto index = [](int x, int y) { return 4 * (x + 2) + (y + 2); };
    for (int x = -2; x <= 1; ++x)
        for (int y = -2; y <= 1; ++y)
            for (int z = -2; z <= 1; ++z)
                result[index(x, y)][index(y, z)] = psi(residue + x + 3 * y + z);
    return result;
}

Matrix minplus_multiply(const Matrix& left, const Matrix& right) {
    Matrix result;
    for (auto& row : result) row.fill(INF);
    for (int i = 0; i < 16; ++i)
        for (int k = 0; k < 16; ++k) {
            if (left[i][k] == INF) continue;
            for (int j = 0; j < 16; ++j) {
                if (right[k][j] == INF) continue;
                result[i][j] = std::min(result[i][j], left[i][k] + right[k][j]);
            }
        }
    return result;
}

Matrix minplus_identity() {
    Matrix result;
    for (int i = 0; i < 16; ++i) {
        result[i].fill(INF);
        result[i][i] = 0;
    }
    return result;
}

Integer minplus_trace(const Matrix& matrix) {
    Integer result = INF;
    for (int i = 0; i < 16; ++i) result = std::min(result, matrix[i][i]);
    return result;
}

void audit_certificate(int residue) {
    const int periods[] = {0, 1, 2, 2, 3};
    const int increments[] = {0, 2, 3, 2, 4};
    const int period = periods[residue], increment = increments[residue];
    const Matrix matrix = scalar_matrix(residue);
    std::vector<Matrix> powers(1, minplus_identity());
    for (int power = 1; power <= 200; ++power) {
        powers.push_back(minplus_multiply(powers.back(), matrix));
        if (power < 3 + period) continue;
        const int first = power - period;
        bool matches = true;
        for (int i = 0; i < 16; ++i)
            for (int j = 0; j < 16; ++j)
                matches = matches && (powers[power][i][j] == powers[first][i][j] + increment);
        if (!matches) continue;
        for (int n = 3; n < power; ++n)
            require(minplus_trace(powers[n]) == expected(n, residue), "initial scalar trace");
        std::cout << "r=" << residue << ": A^(" << first << '+' << period << ") = A^"
                  << first << " + " << increment << "; all 256 entries checked exactly\n";
        std::cout << "  initial traces:";
        for (int n = 3; n < power; ++n)
            std::cout << " (" << n << ',' << minplus_trace(powers[n]) << ')';
        std::cout << '\n';
        return;
    }
    throw std::runtime_error("no matrix certificate for residue " + std::to_string(residue));
}

Integer projection_bound(int m, int n) {
    const Integer correction = std::max(expected(n, mod(-m, 5)), expected(m, mod(-n, 5)));
    return (2LL * m * n + correction + 4) / 5;
}

void audit_known_values() {
    for (int r = 1; r <= 4; ++r) audit_certificate(r);
    for (int m = 3; m <= 6; ++m) {
        std::set<Integer> gaps;
        for (int n = std::max(m, 3); n <= 300; ++n) {
            Integer gamma;
            if (m == 3) gamma = (3LL * n + 1) / 2;
            else if (m == 4) gamma = 2LL * n;
            else if (m == 5) gamma = 2LL * n + (n % 5 == 0 ? 0 : 2);
            else {
                const int extra = n % 6 == 0 ? 0 :
                    (n % 6 == 5 || n % 18 == 3 || n % 18 == 8 || n % 18 == 13 ? 1 : 2);
                gamma = 8LL * n / 3 + extra;
            }
            const Integer lower = projection_bound(m, n);
            require(lower <= gamma, "known-width lower bound, m=" + std::to_string(m)
                    + ", n=" + std::to_string(n));
            gaps.insert(gamma - lower);
        }
        std::cout << "m=" << m << ", n through 300: gaps in gamma = {";
        bool first = true;
        for (Integer gap : gaps) {
            if (!first) std::cout << ", ";
            std::cout << gap;
            first = false;
        }
        std::cout << "}\n";
    }
}

Integer weight(const Labels& labels) {
    Integer result = 0;
    for (const auto& column : labels)
        for (int label : column) result += label;
    return result;
}

bool has_two_neighbor(const Labels& labels, int i, int j) {
    const int n = static_cast<int>(labels.size());
    const int m = static_cast<int>(labels[0].size());
    return labels[j][mod(i - 1, m)] == 2 || labels[j][(i + 1) % m] == 2 ||
           labels[mod(j - 1, n)][i] == 2 || labels[(j + 1) % n][i] == 2;
}

void check_roman(const Labels& labels) {
    const int n = static_cast<int>(labels.size());
    require(n >= 3, "cycle must have at least three columns");
    const int m = static_cast<int>(labels[0].size());
    require(m >= 3, "cycle must have at least three rows");
    for (const auto& column : labels)
        require(static_cast<int>(column.size()) == m, "ragged label array");
    for (int j = 0; j < n; ++j)
        for (int i = 0; i < m; ++i) {
            require(labels[j][i] >= 0 && labels[j][i] <= 2, "invalid Roman label");
            require(labels[j][i] != 0 || has_two_neighbor(labels, i, j),
                    "uncovered vertex: m=" + std::to_string(m) + ", n=" + std::to_string(n)
                    + ", row=" + std::to_string(i) + ", column=" + std::to_string(j));
        }
}

Labels construction(int m, int n) {
    const int k = m / 5, r = m % 5;
    Labels labels(n, std::vector<int>(m, 0));
    if (r == 0) {
        require(n % 5 == 0, "construction period for r=0");
        for (int j = 0; j < n; ++j)
            for (int i = 0; i < m; ++i)
                if ((i + 2 * j) % 5 == 0) labels[j][i] = 2;
    } else if (r == 1) {
        require(n % (3 * m) == 0, "construction period for r=1");
        std::array<std::vector<int>, 3> blocks;
        blocks[0].push_back(0);
        for (int h = 0; h < k; ++h) {
            blocks[0].push_back(1 + 5 * h);
            blocks[1].push_back(3 + 5 * h);
            blocks[2].push_back(5 + 5 * h);
        }
        for (int j = 0; j < n; ++j)
            for (int i : blocks[j % 3]) labels[j][(i + j / 3) % m] = 2;
    } else if (r == 2 || r == 3) {
        require(n % (2 * m) == 0, "construction period for r=2 or r=3");
        std::array<std::vector<int>, 2> blocks;
        blocks[0].push_back(0);
        for (int h = 0; h < k; ++h) {
            blocks[0].push_back(r + 5 * h);
            blocks[1].push_back(r + 3 + 5 * h);
        }
        for (int j = 0; j < n; ++j) {
            for (int i : blocks[j % 2]) labels[j][(i + j / 2) % m] = 2;
            if (r == 3 && j % 2 != 0) labels[j][(2 + j / 2) % m] = 1;
        }
    } else {
        require(2 * n % m == 0, "construction period for r=4");
        for (int j = 0; j < n; ++j)
            for (int h = 0; h <= k; ++h) labels[j][(5 * h + 2 * j) % m] = 2;
    }
    return labels;
}

void audit_uniform_constructions() {
    // Six times the rational slope: alpha(m)=2k+(0,2/3,1,3/2,2)[r].
    const int slope_residue_times_six[] = {0, 4, 6, 9, 12};
    for (int m = 3; m <= 80; ++m) {
        const int n = 6 * m;
        const Labels labels = construction(m, n);
        check_roman(labels);
        const Integer slope_times_six = 12 * (m / 5) + slope_residue_times_six[m % 5];
        const Integer actual = weight(labels);
        require(6 * actual == n * slope_times_six, "uniform construction weight");
        require(projection_bound(m, n) == actual, "uniform construction sharpness");
    }
    std::cout << "Uniform constructions: all widths 3 through 80 checked vertex by vertex.\n";
}

std::pair<std::array<Integer, 5>, Integer> seam_data(int m, int n) {
    std::array<Integer, 5> weights{{0, 0, 0, 0, 0}};
    Integer missing_sum = 0;
    for (int i = 0; i < m; ++i)
        for (int j = 0; j < n; ++j) {
            const std::array<std::pair<int, int>, 5> neighbors{{
                {i, j}, {mod(i - 1, m), j}, {(i + 1) % m, j},
                {i, mod(j - 1, n)}, {i, (j + 1) % n}
            }};
            std::array<bool, 5> colors{{false, false, false, false, false}};
            for (const auto& vertex : neighbors) colors[(vertex.first + 2 * vertex.second) % 5] = true;
            missing_sum += std::count(colors.begin(), colors.end(), false);
            for (int phase = 0; phase < 5; ++phase) {
                if ((i + 2 * j) % 5 == phase) weights[phase] += 2;
                else if (!colors[phase]) weights[phase] += 1;
            }
        }
    Integer sum = 0;
    for (Integer value : weights) sum += value;
    require(sum == 2LL * m * n + missing_sum, "seam weight identity");
    return {weights, missing_sum};
}

void audit_seam_averaging() {
    int count = 0;
    for (int m = 3; m <= 40; ++m)
        for (int n = 3; n <= 40; ++n) {
            const auto data = seam_data(m, n);
            const int scale = (m % 5 != 0 ? n : 0) + (n % 5 != 0 ? m : 0);
            const Integer minimum = *std::min_element(data.first.begin(), data.first.end());
            require(data.second <= 2 * scale, "seam missing-color bound");
            require(minimum <= (2LL * m * n + 2 * scale) / 5, "seam average bound");
            require(projection_bound(m, n) <= minimum, "seam consistency with lower bound");
            ++count;
        }
    require(count == 1444, "seam test count");
    std::cout << "Seam averaging: 1,444 tori checked.\n";
}

Labels phase_labels(int k, int n, bool check_seam_location = false) {
    const int m = 5 * k + 2;
    Labels labels(n, std::vector<int>(m, 0));
    for (int j = 0; j < n; ++j) {
        if (j % 2 == 0) {
            labels[j][(j / 2) % m] = 2;
            for (int h = 0; h < k; ++h) labels[j][(2 + 5 * h + j / 2) % m] = 2;
        } else {
            for (int h = 1; h <= k; ++h) labels[j][(5 * h + j / 2) % m] = 2;
        }
    }
    // Added 1-labels do not change which zero vertices have a 2-neighbor.
    for (int j = 0; j < n; ++j)
        for (int i = 0; i < m; ++i)
            if (labels[j][i] == 0 && !has_two_neighbor(labels, i, j)) {
                if (check_seam_location) require(j == 0 || j == n - 1, "hole outside seam columns");
                labels[j][i] = 1;
            }
    return labels;
}

void audit_phase_repairs() {
    int total = 0;
    for (int k = 1; k <= 12; ++k) {
        const int m = 5 * k + 2, c = 2 * k + 1;
        for (int n = 3; n < 4 * m; ++n) {
            const Labels labels = phase_labels(k, n, true);
            check_roman(labels);
            const int r = c * n % (2 * m), rho = std::min(r, 2 * m - r);
            const Integer actual = weight(labels);
            require(actual == 1LL * c * n + std::min(rho, c), "phase repair weight");
            const int length_residue = n % (2 * m);
            if (length_residue == 5 || length_residue == 2 * m - 5)
                require(actual == 1LL * c * n + 1, "first-unit construction");
            if (length_residue == 10 % (2 * m) || length_residue == mod(-10, 2 * m))
                require(actual == 1LL * c * n + 2, "second-unit construction");
            ++total;
        }
    }
    require(total == 1620, "phase repair test count");
    std::cout << "Phase repairs: 1,620 tori checked vertex by vertex.\n";
}

void check_energy(const std::vector<int>& u) {
    const int n = static_cast<int>(u.size());
    std::vector<int> signs(n), positive(n), negative(n), t(n);
    for (int j = 0; j < n; ++j) {
        signs[j] = u[j] < 0 ? 1 : 0;
        positive[j] = std::max(u[j], 0);
        negative[j] = std::max(-u[j] - 1, 0);
        t[j] = 3 + u[mod(j - 1, n)] + 3 * u[j] + u[(j + 1) % n];
    }
    Integer equal = 0, cp = 0, cv = 0, r0 = 0, r1 = 0, f = 0, p = 0, v = 0;
    for (int j = 0; j < n; ++j) {
        const int previous = mod(j - 1, n), next = (j + 1) % n;
        equal += signs[j] == signs[next] ? 1 : 0;
        cp += positive[j] * (2 - signs[previous] - signs[next]);
        cv += negative[j] * (1 + signs[previous] + signs[next]);
        if (signs[j] == 0) r0 += std::max(-t[j], 0);
        else r1 += std::max(t[j], 0);
        f += psi(t[j]);
        p += positive[j];
        v += negative[j];
    }
    const Integer h = equal + cp + cv + r0 + r1;
    require(f == n + 5 * h, "energy decomposition identity");
    require(v <= h && 2 * p <= 3 * h, "energy amplitude estimates");
    if (n % 2 == 0 && f <= n + 5) {
        require(equal == 0 && p == 0, "even first-unit signs");
        require(v == 0 || v == 1, "even first-unit negative amplitude");
        require(f == n + 5 * v, "even first-unit value");
    }
}

// Explicit unsigned 64-bit generator, so the random checks are reproducible
// across C++14 standard libraries. This intentionally is not Python's RNG.
class DeterministicRng {
    std::uint64_t state_;
public:
    explicit DeterministicRng(std::uint64_t seed) : state_(seed) {}
    std::uint64_t next() {
        state_ ^= state_ >> 12;
        state_ ^= state_ << 25;
        state_ ^= state_ >> 27;
        return state_ * UINT64_C(2685821657736338717);
    }
    int range(int low, int high_exclusive) {
        return low + static_cast<int>(next() % static_cast<std::uint64_t>(high_exclusive - low));
    }
};

void audit_energy_identity() {
    int count = 0;
    for (int n = 3; n <= 8; ++n) {
        const int limit = 1 << (2 * n);
        std::vector<int> u(n);
        for (int encoded = 0; encoded < limit; ++encoded) {
            int value = encoded;
            for (int j = n - 1; j >= 0; --j) {
                u[j] = value % 4 - 2;
                value /= 4;
            }
            check_energy(u);
            ++count;
        }
    }
    DeterministicRng rng(UINT64_C(20260908));
    for (int trial = 0; trial < 2000; ++trial) {
        std::vector<int> u(rng.range(3, 51));
        for (int& value : u) value = rng.range(-100, 101);
        check_energy(u);
    }
    require(count == 87360, "energy exhaustive test count");
    std::cout << "Energy decomposition: 87,360 exhaustive small vectors and 2,000 large-amplitude vectors checked.\n";
}

Labels sweep_labels(int k) {
    const int m = 5 * k + 2;
    Labels labels(2 * k + 4, std::vector<int>(m, 0));
    for (int j = 0; j <= k + 1; ++j) {
        std::set<int> heavy, light;
        if (j == 0 || j == k + 1) {
            const int shift = j == 0 ? 0 : k;
            heavy.insert(shift % m);
            for (int h = 0; h < k; ++h) heavy.insert((2 + 5 * h + shift) % m);
            for (int h = 1; h <= k; ++h) light.insert((5 * h + shift) % m);
        } else {
            const int q = k - j + 1;
            heavy.insert((j - 1) % m);
            for (int h = 0; h < k; ++h)
                heavy.insert(mod(2 + 5 * h + j - (h >= q ? 1 : 0), m));
            for (int h = 1; h <= k; ++h)
                light.insert(mod(5 * h + j - (h >= q ? 1 : 0), m));
        }
        require(static_cast<int>(heavy.size()) == k + 1 && static_cast<int>(light.size()) == k,
                "moving-gap center counts");
        for (int i : heavy) labels[2 * j][i] = 2;
        for (int i : light) labels[2 * j + 1][i] = 2;
    }
    for (int j = 0; j <= k; ++j) {
        require(labels[2 * j + 1][(j + 1) % m] == 0, "moving-gap singleton placement");
        labels[2 * j + 1][(j + 1) % m] = 1;
    }
    return labels;
}

Labels replace_sweep(const Labels& original, const Labels& shifted_sweep, int k) {
    Labels result;
    result.insert(result.end(), original.begin(), original.begin() + 2);
    result.insert(result.end(), shifted_sweep.begin(), shifted_sweep.end());
    result.insert(result.end(), original.begin() + (2 * k + 4), original.end());
    return result;
}

void audit_sweep_replacement() {
    int count = 0;
    for (int k = 1; k <= 15; ++k) {
        const int m = 5 * k + 2, c = 2 * k + 1;
        Labels shifted = sweep_labels(k);
        for (auto& column : shifted) std::rotate(column.begin(), column.end() - 1, column.end());
        for (int n = 2 * k + 7; n <= 4 * m + 2; ++n) {
            const Labels original = phase_labels(k, n - 2);
            const Labels replaced = replace_sweep(original, shifted, k);
            require(static_cast<int>(replaced.size()) == n, "moving-gap replacement length");
            check_roman(replaced);
            const int r = c * (n - 2) % (2 * m);
            const int old_excess = std::min(std::min(r, 2 * m - r), c);
            require(weight(replaced) == 1LL * c * n + old_excess + k + 1, "moving-gap weight");
            ++count;
        }
        const int n = 2 * m + 2;
        const Labels replaced = replace_sweep(phase_labels(k, n - 2), shifted, k);
        require(weight(replaced) == 1LL * c * n + k + 1, "moving-gap improved residue-two bound");
    }
    require(count == 2220, "moving-gap test count");
    std::cout << "Moving-gap replacements: 2,220 tori checked, k=1,...,15.\n";
}
} // namespace

int main() {
    try {
        std::cout << "Roman torus manuscript checks (C++14, exact integer arithmetic)\n";
        audit_known_values();
        audit_uniform_constructions();
        audit_seam_averaging();
        audit_phase_repairs();
        audit_energy_identity();
        audit_sweep_replacement();
        std::cout << "PASS: all checks completed. Finite tests do not replace the manuscript proofs.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
