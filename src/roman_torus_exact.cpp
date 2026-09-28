// Exact Roman domination of C_m square C_n. C++14, standard library only.
// Build: g++ -O3 -std=c++14 roman_torus_exact.cpp -o roman_torus_exact
// Example: ./roman_torus_exact --m 7 --n-max 733 --csv m7.csv
//          --witness 23 --witness-file witness_7_23.txt
// The cyclic state graph is unrestricted. A row-dihedral orbit reduction
// reduces starting rows only: endpoint states are NEVER identified.
// All certificates and RDF checks remain enabled under -DNDEBUG.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
const int INF = 1000000000;
void require(bool ok, const std::string& why) {
    if (!ok) throw std::runtime_error(why);
}
int popcount(unsigned x) {
    int n = 0;
    for (; x; x &= x - 1) ++n;
    return n;
}
int gcd_int(int a, int b) { return b ? gcd_int(b, a % b) : a; }
int parse_integer(const std::string& text) {
    std::size_t consumed = 0;
    const int value = std::stoi(text, &consumed);
    require(consumed == text.size(), "invalid integer: " + text);
    return value;
}
struct Options {
    int m = 7, nmin = 3, nmax = 733, period = 0, increment = -1;
    int witness = 0;
    bool symmetry = true, recurrence = true;
    std::string csv, witness_file;
};
Options parse(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string key = argv[i];
        if (key == "--help") {
            std::cout << "Exact RDF solver for C_m square C_n\n"
                "--m M             circumference, 3..10 (default 7)\n"
                "--n-min N         first reported length (default 3)\n"
                "--n-max N         last reported length (default 733)\n"
                "--csv FILE        write result table (default stdout)\n"
                "--period P        period to test (default construction period)\n"
                "--increment D     increment to test (default P*alpha(m))\n"
                "--no-recurrence   compute every requested power directly\n"
                "--no-symmetry     use every starting state, useful cross-check\n"
                "--witness N       reconstruct and verify an optimum RDF\n"
                "--witness-file F  output RDF certificate (default stdout)\n";
            std::exit(0);
        }
        if (key == "--no-symmetry") { o.symmetry = false; continue; }
        if (key == "--no-recurrence") { o.recurrence = false; continue; }
        require(i + 1 < argc, "missing value for " + key);
        const std::string value = argv[++i];
        if (key == "--m") o.m = parse_integer(value);
        else if (key == "--n-min") o.nmin = parse_integer(value);
        else if (key == "--n-max") o.nmax = parse_integer(value);
        else if (key == "--period") o.period = parse_integer(value);
        else if (key == "--increment") o.increment = parse_integer(value);
        else if (key == "--csv") o.csv = value;
        else if (key == "--witness") o.witness = parse_integer(value);
        else if (key == "--witness-file") o.witness_file = value;
        else throw std::runtime_error("unknown option " + key);
    }
    require(o.m >= 3 && o.m <= 10, "m must be between 3 and 10");
    require(o.nmin >= 3 && o.nmax >= o.nmin, "invalid length range");
    require(o.nmax <= 1000000, "n-max exceeds supported exact-integer output range");
    require(o.witness == 0 || (o.witness >= 3 && o.witness <= o.nmax),
            "witness length must be between 3 and n-max");
    return o;
}
int alpha_six(int m) {
    const int a[] = {0, 4, 6, 9, 12};
    return 12 * (m / 5) + a[m % 5];
}
long long scalar(int length, int residue) {
    switch (residue) {
    case 0: return 0;
    case 1: return 2LL * length;
    case 2: return (3LL * length + 5 * (length % 2)) / 2;
    case 3: return length + 5 * (length % 2);
    case 4: return (4LL * length + 5 * (length % 3)) / 3;
    }
    throw std::runtime_error("bad residue");
}
long long projection_bound(int m, int n) {
    return (2LL * m * n + std::max(scalar(n, (5 - m % 5) % 5),
                    scalar(m, (5 - n % 5) % 5)) + 4) / 5;
}
struct State { unsigned b, u; };
struct Edge { int destination, weight; };
struct Automaton {
    int m, masks, count;
    unsigned full;
    std::vector<State> states;
    std::vector<unsigned> closed;
    std::vector<int> index, representatives;
    std::vector<Edge> edges;

    unsigned rotate(unsigned x) const {
        return ((x << 1) & full) | (x >> (m - 1));
    }
    unsigned reflect(unsigned x) const {
        unsigned y = 0;
        for (int i = 0; i < m; ++i)
            if (x & (1u << i)) y |= 1u << ((m - i) % m);
        return y;
    }
    explicit Automaton(int width, bool symmetry)
        : m(width), masks(1 << m), full(masks - 1), closed(masks),
          index(static_cast<std::size_t>(masks) * masks, -1) {
        for (unsigned b = 0; b < unsigned(masks); ++b) {
            closed[b] = b | rotate(b) | ((b >> 1) | ((b & 1) << (m - 1)));
            const unsigned available = full ^ closed[b];
            for (unsigned u = available;; u = (u - 1) & available) {
                index[static_cast<std::size_t>(b) * masks + u] = int(states.size());
                states.push_back(State{b, u});
                if (!u) break;
            }
        }
        count = int(states.size());
        for (int s = 0; s < count; ++s) {
            unsigned b = states[s].b, u = states[s].u;
            int canonical = s;
            if (symmetry) for (int k = 0; k < m; ++k) {
                canonical = std::min(canonical, index[std::size_t(b) * masks + u]);
                canonical = std::min(canonical,
                    index[std::size_t(reflect(b)) * masks + reflect(u)]);
                b = rotate(b); u = rotate(u);
            }
            if (canonical == s) representatives.push_back(s);
        }
        edges.reserve(static_cast<std::size_t>(count) * masks);
        for (const State& s : states) for (unsigned c = 0; c < unsigned(masks); ++c) {
            const unsigned next_u = full & ~(closed[c] | s.b);
            const int dst = index[std::size_t(c) * masks + next_u];
            require(dst >= 0, "internal state closure failure");
            edges.push_back(Edge{dst, 2 * popcount(s.b) + popcount(s.u & ~c)});
        }
    }
    void step(const int* previous, int* next, int* parent = nullptr) const {
        std::fill(next, next + count, INF);
        for (int s = 0; s < count; ++s) {
            if (previous[s] == INF) continue;
            const Edge* begin = edges.data() + std::size_t(s) * masks;
            for (int c = 0; c < masks; ++c) {
                const Edge& e = begin[c];
                const int candidate = previous[s] + e.weight;
                if (candidate < next[e.destination]) {
                    next[e.destination] = candidate;
                    if (parent) parent[e.destination] = s;
                }
            }
        }
    }
};
struct Result {
    std::vector<int> value, source;
    int certificate_base = -1, certificate_end = -1, computed = 0;
};
Result solve(const Automaton& a, const Options& o) {
    const int rows = int(a.representatives.size()), s = a.count;
    const std::size_t cells = std::size_t(rows) * s;
    const int ring_size = o.recurrence ? o.period + 1 : 2;
    require(cells * ring_size <= 250000000ULL, "requested recurrence ring is too large");
    std::vector<std::vector<int>> ring(ring_size, std::vector<int>(cells, INF));
    for (int r = 0; r < rows; ++r) ring[0][std::size_t(r) * s + a.representatives[r]] = 0;
    Result result;
    result.value.assign(o.nmax + 1, INF);
    result.source.assign(o.nmax + 1, -1);
    result.value[0] = 0;
    for (int n = 1; n <= o.nmax; ++n) {
        const std::vector<int>& previous = ring[(n - 1) % ring_size];
        std::vector<int>& current = ring[n % ring_size];
        for (int r = 0; r < rows; ++r) {
            a.step(previous.data() + std::size_t(r) * s,
                   current.data() + std::size_t(r) * s);
            const int weight = current[std::size_t(r) * s + a.representatives[r]];
            if (weight < result.value[n]) {
                result.value[n] = weight;
                result.source[n] = a.representatives[r];
            }
        }
        result.computed = n;
        if (n % 25 == 0) std::cerr << "computed all representative rows through n=" << n << '\n';
        if (o.recurrence && n >= o.period + 3) {
            const std::vector<int>& old = ring[(n - o.period) % ring_size];
            bool matches = true;
            for (std::size_t i = 0; i < cells; ++i) {
                if (old[i] == INF) { if (current[i] != INF) { matches = false; break; } }
                else if (current[i] != old[i] + o.increment) { matches = false; break; }
            }
            if (matches) {
                result.certificate_base = n - o.period;
                result.certificate_end = n;
                std::cerr << "CERTIFICATE: A^(" << n << ") = A^(" << n - o.period
                    << ") + " << o.increment << "; all " << cells
                    << " representative-row entries checked; by symmetry all "
                    << std::size_t(s) * s << " matrix entries follow.\n";
                for (int j = n + 1; j <= o.nmax; ++j) {
                    result.value[j] = result.value[j - o.period] + o.increment;
                    result.source[j] = result.source[j - o.period];
                }
                break;
            }
        }
    }
    return result;
}
void witness(const Automaton& a, int n, int source, int optimum, std::ostream& out) {
    const int s = a.count;
    require(std::size_t(n + 1) * s <= 250000000ULL, "witness parent table too large");
    std::vector<int> parent(std::size_t(n + 1) * s, -1), previous(s, INF), next(s);
    previous[source] = 0;
    for (int j = 1; j <= n; ++j) {
        a.step(previous.data(), next.data(), parent.data() + std::size_t(j) * s);
        previous.swap(next);
    }
    require(previous[source] == optimum, "witness DP and exact trace disagree");
    std::vector<unsigned> centers(n);
    int destination = source;
    for (int j = n; j >= 1; --j) {
        destination = parent[std::size_t(j) * s + destination];
        require(destination >= 0, "broken witness predecessor");
        centers[j - 1] = a.states[destination].b;
    }
    require(destination == source, "witness did not close at starting state");
    std::vector<std::vector<int>> labels(n, std::vector<int>(a.m));
    long long weight = 0;
    for (int j = 0; j < n; ++j) for (int i = 0; i < a.m; ++i) {
        const unsigned bit = 1u << i;
        const unsigned covered = a.closed[centers[j]] | centers[(j + n - 1) % n]
                                  | centers[(j + 1) % n];
        labels[j][i] = centers[j] & bit ? 2 : (covered & bit ? 0 : 1);
        weight += labels[j][i];
    }
    require(weight == optimum, "reconstructed RDF weight mismatch");
    for (int j = 0; j < n; ++j) for (int i = 0; i < a.m; ++i) {
        if (labels[j][i] != 0) continue;
        require(labels[j][(i + a.m - 1) % a.m] == 2 || labels[j][(i + 1) % a.m] == 2 ||
                labels[(j + n - 1) % n][i] == 2 || labels[(j + 1) % n][i] == 2,
                "vertex-level RDF audit failed");
    }
    out << "# Verified optimum Roman dominating function\n"
        << "m " << a.m << "\nn " << n << "\nweight " << weight << "\n"
        << "# center masks, columns j=0,...,n-1; bit i is row i\ncenters";
    for (unsigned b : centers) out << ' ' << b;
    out << "\n# label grid: m rows, n columns; both coordinates are cyclic\n";
    for (int i = 0; i < a.m; ++i) {
        for (int j = 0; j < n; ++j) out << labels[j][i];
        out << '\n';
    }
    out << "# Every zero vertex has a neighbor labelled 2; weight audited.\n";
}
} // namespace

int main(int argc, char** argv) {
    try {
        Options o = parse(argc, argv);
        if (o.period == 0) {
            switch (o.m % 5) {
            case 0: o.period = 5; break;
            case 1: o.period = 3 * o.m; break;
            case 2: case 3: o.period = 2 * o.m; break;
            case 4: o.period = o.m / gcd_int(o.m, 2); break;
            }
        }
        require(o.period >= 1 && o.period <= 10000, "invalid period");
        if (o.increment == -1) {
            require((o.period * alpha_six(o.m)) % 6 == 0,
                    "period times alpha(m) is nonintegral; supply --increment");
            o.increment = o.period * alpha_six(o.m) / 6;
        }
        require(o.increment >= 0 && o.increment <= 2 * o.m * o.period,
                "increment must be between 0 and 2*m*period");
        const auto begin = std::chrono::steady_clock::now();
        const Automaton a(o.m, o.symmetry);
        std::cerr << "m=" << o.m << ", states=" << a.count << ", edges=" << a.edges.size()
            << ", starting-row representatives=" << a.representatives.size() << '\n';
        const Result result = solve(a, o);
        std::ofstream csv;
        if (!o.csv.empty()) { csv.open(o.csv); require(bool(csv), "cannot create CSV"); }
        std::ostream& out = o.csv.empty() ? std::cout : csv;
        out << "m,n,gamma_R,projection_lower_bound,excess_times_6,status\n";
        for (int n = o.nmin; n <= o.nmax; ++n) {
            require(result.value[n] >= projection_bound(o.m, n), "projection lower bound violated");
            out << o.m << ',' << n << ',' << result.value[n] << ',' << projection_bound(o.m, n)
                << ',' << 6LL * result.value[n] - 1LL * alpha_six(o.m) * n << ','
                << (n <= result.computed ? "direct_exact" : "certified_recurrence") << '\n';
        }
        if (o.witness) {
            std::ofstream file;
            if (!o.witness_file.empty()) {
                file.open(o.witness_file); require(bool(file), "cannot create witness file");
            }
            witness(a, o.witness, result.source[o.witness], result.value[o.witness],
                    o.witness_file.empty() ? std::cout : file);
        }
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
        std::cerr << "exact lengths " << o.nmin << ".." << o.nmax
            << "; directly computed through " << result.computed
            << "; elapsed_seconds=" << seconds << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }
}
