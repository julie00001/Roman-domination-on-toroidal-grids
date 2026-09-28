// C++14 verification for the restricted unit-step phase barrier.
// This checks a local identity and explicit sharp witnesses; it does not
// prove or claim a lower bound for arbitrary Roman dominating functions.
// Build, for example: g++ -std=c++14 -O2 verify_phase_barrier.cpp -o verify_phase_barrier
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

int mod(int x, int m) { x %= m; return x < 0 ? x + m : x; }
int positive(int x) { return std::max(0, x); }

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

long long check_binary_delay_identity() {
    long long count = 0;
    for (int k = 1; k <= 5; ++k) {
        const int m = 5 * k + 2;
        for (int X = 0; X < (1 << (k + 1)); ++X)
        for (int Y = 0; Y < (1 << (k + 1)); ++Y)
        for (int Z = 0; Z < (1 << k); ++Z) {
            std::vector<int> x(k + 1), y(k + 1), z(k + 1);
            for (int h = 0; h <= k; ++h) {
                x[h] = (X >> h) & 1;
                y[h] = (Y >> h) & 1;
                if (h) z[h] = (Z >> (h - 1)) & 1;
            }
            std::vector<bool> covered(m, false);
            covered[mod(-x[0], m)] = true;
            covered[mod(1 - y[0], m)] = true;
            for (int h = 1; h <= k; ++h) {
                covered[mod(5 * h - 3 - x[h], m)] = true;
                covered[mod(5 * h - 2 - y[h], m)] = true;
                for (int d = -1; d <= 1; ++d)
                    covered[mod(5 * h - z[h] + d, m)] = true;
            }
            const int actual = static_cast<int>(
                std::count(covered.begin(), covered.end(), false));
            int predicted = positive(y[0] - x[1]);
            for (int h = 0; h <= k; ++h)
                predicted += positive(x[h] - y[h]);
            for (int h = 1; h <= k; ++h) {
                predicted += positive(y[h] - z[h]);
                predicted += positive(z[h] - x[h == k ? 0 : h + 1]);
            }
            require(actual == predicted, "binary-delay identity failed");
            ++count;
        }
    }
    return count;
}

std::vector<int> heavy(int k, int j) {
    const int m = 5 * k + 2;
    std::vector<int> centers{mod(j - (j >= 1), m)};
    for (int h = 1; h <= k; ++h)
        centers.push_back(mod(5 * h - 3 + j - (j >= k - h + 2), m));
    return centers;
}

std::vector<int> light(int k, int j) {
    const int m = 5 * k + 2;
    std::vector<int> centers;
    for (int h = 1; h <= k; ++h)
        centers.push_back(mod(5 * h + j - (j >= k - h + 1), m));
    return centers;
}

int holes(const std::vector<int>& current, const std::vector<int>& left,
          const std::vector<int>& right, int m) {
    std::vector<bool> covered(m, false);
    for (int x : current)
        for (int d = -1; d <= 1; ++d) covered[mod(x + d, m)] = true;
    for (int x : left) covered[x] = true;
    for (int x : right) covered[x] = true;
    return static_cast<int>(std::count(covered.begin(), covered.end(), false));
}

int check_sharp_sweeps() {
    int cases = 0;
    for (int k = 1; k <= 80; ++k) {
        const int m = 5 * k + 2;
        int stops = 0, uncovered = 0;
        for (int j = -1; j <= k + 2; ++j) {
            const auto H = heavy(k, j), next = heavy(k, j + 1);
            for (int h = 0; h <= k; ++h) {
                const int step = mod(next[h] - H[h], m);
                require(step == 0 || step == 1, "non-unit sweep step");
                stops += (step == 0);
            }
            uncovered += holes(light(k, j), H, next, m);
            require(holes(H, light(k, j - 1), light(k, j), m) == 0,
                    "heavy column is not fully dominated");
        }
        require(stops == k + 1, "wrong stationary-match count");
        require(uncovered == k + 1, "sweep does not attain barrier");
        ++cases;
    }
    return cases;
}

int main() {
    try {
        const auto local = check_binary_delay_identity();
        const int sweeps = check_sharp_sweeps();
        std::cout << "PASS: " << local << " exact local identities; "
                  << sweeps << " sharp unit-step sweeps.\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
