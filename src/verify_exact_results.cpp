// Independent audits of the exact Roman domination results. Standard C++14.
// Build: g++ -O3 -std=c++14 verify_exact_results.cpp -o verify_exact_results
// Usage: ./verify_exact_results [directory containing m3_exact.csv, etc.]
// Does not include, link to, or reproduce the transfer-state solver.
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
void require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}
int bitcount(std::uint64_t x) {
    x -= (x >> 1) & UINT64_C(0x5555555555555555);
    x = (x & UINT64_C(0x3333333333333333)) +
        ((x >> 2) & UINT64_C(0x3333333333333333));
    x = (x + (x >> 4)) & UINT64_C(0x0f0f0f0f0f0f0f0f);
    return int((x * UINT64_C(0x0101010101010101)) >> 56);
}
using Key = std::pair<int, int>;
using Table = std::map<Key, int>;
Table load_csv(const std::string& path) {
    std::ifstream file(path);
    require(bool(file), "cannot open " + path);
    Table table;
    std::string line;
    require(bool(std::getline(file, line)) &&
            line == "m,n,gamma_R,projection_lower_bound,excess_times_6,status",
            "unexpected CSV header: " + path);
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream row(line);
        int m, n, value, bound, excess;
        std::string status;
        require(bool(row >> m >> n >> value >> bound >> excess >> status),
                "invalid row in " + path);
        require(table.emplace(Key(m, n), value).second, "duplicate CSV key");
        require(status == "direct_exact" || status == "certified_recurrence",
                "unrecognized CSV result status");
    }
    require(!table.empty(), "empty table " + path);
    return table;
}

// Exhaustively enumerate every center subset D, with no pruning. For fixed D,
// the least-weight RDF has weight 2|D|+|V\N[D]|. This is a direct independent
// optimization over graph vertices, without columns or transfer states.
struct BruteForce {
    int size, optimum;
    std::vector<std::uint64_t> neighborhood;
    std::uint64_t subsets = 0;
    BruteForce(int m, int n) : size(m * n), optimum(m * n), neighborhood(size) {
        require(size < 63, "brute-force graph exceeds mask capacity");
        for (int i = 0; i < m; ++i) for (int j = 0; j < n; ++j) {
            const int v = i * n + j;
            neighborhood[v] = (UINT64_C(1) << v) |
                (UINT64_C(1) << (((i + 1) % m) * n + j)) |
                (UINT64_C(1) << (((i + m - 1) % m) * n + j)) |
                (UINT64_C(1) << (i * n + (j + 1) % n)) |
                (UINT64_C(1) << (i * n + (j + n - 1) % n));
        }
    }
    void visit(int next, int centers, std::uint64_t covered) {
        if (next == size) {
            ++subsets;
            optimum = std::min(optimum, 2 * centers + size - bitcount(covered));
            return;
        }
        visit(next + 1, centers, covered);
        visit(next + 1, centers + 1, covered | neighborhood[next]);
    }
};

// Centers and labels are stored as a full vertex array, independent of masks.
struct Coverage {
    int centers = 0, uncovered = 0, uncovered_light = 0, weight = 0;
};
Coverage audit_centers(const std::vector<std::vector<int>>& center) {
    const int n = int(center.size()), m = int(center[0].size());
    Coverage result;
    for (int j = 0; j < n; ++j) for (int i = 0; i < m; ++i) {
        if (center[j][i]) { ++result.centers; result.weight += 2; continue; }
        const bool covered = center[j][(i + 1) % m] ||
            center[j][(i + m - 1) % m] || center[(j + 1) % n][i] ||
            center[(j + n - 1) % n][i];
        if (!covered) {
            ++result.uncovered;
            result.uncovered_light += (j % 2);
            ++result.weight;
        }
    }
    return result;
}
void audit_witness(const std::string& path, const Table& exact) {
    std::ifstream input(path);
    require(bool(input), "cannot open " + path);
    int m = 0, n = 0, weight = -1;
    std::vector<unsigned long long> masks;
    std::vector<std::string> grid;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream words(line);
        std::string first;
        words >> first;
        if (first == "m") words >> m;
        else if (first == "n") words >> n;
        else if (first == "weight") words >> weight;
        else if (first == "centers") {
            unsigned long long mask;
            while (words >> mask) masks.push_back(mask);
        } else grid.push_back(first);
    }
    require(m >= 3 && m < 63 && n >= 3 && int(masks.size()) == n &&
            int(grid.size()) == m, "bad witness dimensions");
    require(exact.at(Key(m, n)) == weight, "witness and exact table disagree");
    int total = 0;
    std::vector<std::vector<int>> centers(n, std::vector<int>(m));
    for (int i = 0; i < m; ++i) {
        require(int(grid[i].size()) == n, "bad grid row length");
        for (int j = 0; j < n; ++j) {
            require(grid[i][j] >= '0' && grid[i][j] <= '2', "invalid RDF label");
            require(masks[j] < (UINT64_C(1) << m), "center mask out of range");
            centers[j][i] = grid[i][j] == '2';
            require(centers[j][i] == int((masks[j] >> i) & 1),
                    "grid and center mask disagree");
            total += grid[i][j] - '0';
            if (grid[i][j] == '0') {
                require(grid[(i + 1) % m][j] == '2' ||
                    grid[(i + m - 1) % m][j] == '2' ||
                    grid[i][(j + 1) % n] == '2' ||
                    grid[i][(j + n - 1) % n] == '2', "undominated zero in witness");
            }
        }
    }
    require(total == weight, "witness label sum incorrect");
    require(audit_centers(centers).weight == weight,
            "witness contains an unnecessary label 1");
    std::cout << "PASS witness " << m << 'x' << n << " weight=" << weight << '\n';
}

int audit_two_turns() {
    int checked = 0;
    for (int k = 1; k <= 12; ++k) {
        const int m = 5 * k + 2;
        for (int a = 1; a <= 80; ++a) for (int b = 1; b <= 80; ++b) {
            if ((a - b) % m != 0) continue;
            const int half = a + b, n = 2 * half;
            std::vector<std::vector<int>> centers(n, std::vector<int>(m));
            int phase = 0;
            for (int j = 0; j < half; ++j) {
                const int step = j < a ? 1 : -1;
                centers[2 * j][phase] = 1;
                for (int h = 0; h < k; ++h)
                    centers[2 * j][(phase + 2 + 5 * h) % m] = 1;
                for (int h = 1; h <= k; ++h)
                    centers[2 * j + 1][(phase + 5 * h + (step == -1 ? m - 1 : 0)) % m] = 1;
                phase = (phase + step + m) % m;
            }
            require(phase == 0, "two-turn phase walk did not close");
            const Coverage result = audit_centers(centers);
            require(result.centers == (2 * k + 1) * half,
                    "two-turn center count incorrect");
            require(result.uncovered_light == 0 && result.uncovered == 2 * k,
                    "two-turn uncovered-set claim failed");
            require(result.weight == (2 * k + 1) * n + 2 * k,
                    "two-turn weight claim failed");
            ++checked;
        }
    }
    // The explicit exceptional n=6 construction in the proof.
    const std::vector<std::vector<int>> rows = {{3,4},{1,6},{3},{0,5},{2,5},{0}};
    std::vector<std::vector<int>> special(6, std::vector<int>(7));
    for (int j = 0; j < 6; ++j) for (int i : rows[j]) special[j][i] = 1;
    const Coverage result = audit_centers(special);
    require(result.centers == 10 && result.uncovered == 0 && result.weight == 20,
            "explicit T_7,6 witness failed");
    std::cout << "PASS two-turn construction: " << checked
              << " cases, k=1..12, a,b=1..80; exceptional 7x6 witness passes\n";
    return checked;
}
} // namespace

int main(int argc, char** argv) {
    try {
        require(argc <= 2, "usage: verify_exact_results [results-directory]");
        const std::string directory = argc == 2 ? argv[1] : ".";
        const std::string prefix = directory + "/";
        Table exact;
        for (int m = 3; m <= 10; ++m) {
            const Table t = load_csv(prefix + "m" + std::to_string(m) + "_exact.csv");
            for (const auto& row : t) {
                require(row.first.first == m, "wrong width in table");
                exact.insert(row);
            }
        }
        const Key sizes[] = {{3,3},{3,4},{3,5},{4,4},{4,5},{5,5},{3,7}};
        std::uint64_t total_subsets = 0;
        for (const Key& size : sizes) {
            BruteForce brute(size.first, size.second);
            brute.visit(0, 0, 0);
            require(brute.subsets == (UINT64_C(1) << brute.size),
                    "not every center subset was enumerated");
            require(brute.optimum == exact.at(size), "brute-force optimum mismatch");
            total_subsets += brute.subsets;
            std::cout << "PASS all-center-subsets " << size.first << 'x' << size.second
                      << " gamma_R=" << brute.optimum << " subsets=" << brute.subsets << '\n';
        }
        int transpose_checks = 0, even_checks = 0;
        for (const auto& row : exact) {
            const int m = row.first.first, n = row.first.second;
            const auto transpose = exact.find(Key(n, m));
            if (transpose != exact.end()) {
                require(row.second == transpose->second, "transposition symmetry failed");
                ++transpose_checks;
            }
            if (m == 7 && n % 2 == 0) {
                require(row.second == 3 * n + (n % 14 == 0 ? 0 : 2),
                        "m=7 even-length formula disagrees with CSV");
                ++even_checks;
            }
        }
        const Table full = load_csv(prefix + "m7_full_check.csv");
        for (const auto& row : full)
            require(exact.at(row.first) == row.second,
                    "full-state and symmetry-reduced tables disagree");
        std::cout << "PASS " << transpose_checks << " transposition comparisons, "
                  << even_checks << " even m=7 values, " << full.size()
                  << " full-state/reduced-state comparisons\n";
        const char* witnesses[] = {"witness_7_23.txt", "witness_8_31.txt",
                                  "witness_9_17.txt", "witness_10_19.txt"};
        for (const char* path : witnesses) audit_witness(prefix + path, exact);
        audit_two_turns();
        std::cout << "ALL AUDITS PASSED; exhaustive subsets=" << total_subsets << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "AUDIT FAILURE: " << error.what() << '\n';
        return 1;
    }
}
