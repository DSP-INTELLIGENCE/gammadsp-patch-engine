// LPCRoots.hpp
#pragma once
#include <array>
#include <complex>
#include <cmath>
#include <algorithm>

namespace lpc {

template<typename T, int MAX_DEG>
struct RootsResult {
    int n = 0;
    std::array<std::complex<T>, MAX_DEG> roots{};
};

// Solve quadratic: z^2 + r z + s = 0
template<typename T>
inline void quadratic_roots(T r, T s, std::complex<T>& z1, std::complex<T>& z2)
{
    // Discriminant
    T disc = r*r - T(4)*s;
    if (disc >= T(0)) {
        T sd = std::sqrt(disc);
        z1 = std::complex<T>((-r + sd) / T(2), T(0));
        z2 = std::complex<T>((-r - sd) / T(2), T(0));
    } else {
        T sd = std::sqrt(-disc);
        z1 = std::complex<T>(-r / T(2),  sd / T(2));
        z2 = std::complex<T>(-r / T(2), -sd / T(2));
    }
}

// One Bairstow iteration step. Polynomial is degree n with coeffs c[0..n]
// where c[0] is leading coefficient.
// Factors polynomial by (z^2 + r z + s).
template<typename T, int MAX_DEG>
inline bool bairstow_factor(
    const std::array<T, MAX_DEG+1>& c, int n,
    T& r, T& s,
    std::array<T, MAX_DEG+1>& b,
    std::array<T, MAX_DEG+1>& d,
    int maxIters = 20, T tol = T(1e-6))
{
    // Deterministic, bounded iterations
    for (int iter = 0; iter < maxIters; ++iter) {

        // Synthetic division to compute b (remainder ends up in b[n], b[n-1])
        // b[0]=c[0], b[1]=c[1]-r*b[0]
        b[0] = c[0];
        b[1] = c[1] - r*b[0];
        for (int i = 2; i <= n; ++i) {
            b[i] = c[i] - r*b[i-1] - s*b[i-2];
        }

        // d is derivative-like for corrections
        d[0] = b[0];
        d[1] = b[1] - r*d[0];
        for (int i = 2; i <= n; ++i) {
            d[i] = b[i] - r*d[i-1] - s*d[i-2];
        }

        // Remainders
        T R1 = b[n-1];
        T R0 = b[n];

        // Converged?
        if (std::abs(R0) < tol && std::abs(R1) < tol)
            return true;

        // Solve for delta r, delta s using linear system:
        // [d[n-2] d[n-3]] [dr] = [-R1]
        // [d[n-1] d[n-2]] [ds]   [-R0]
        // (indices valid for n>=2)
        T c1 = d[n-2];
        T c2 = d[n-3];
        T c3 = d[n-1];
        T c4 = d[n-2];

        T det = c1*c4 - c2*c3;
        if (std::abs(det) < T(1e-12)) {
            // Nudge to avoid singularity
            r += T(1e-3);
            s += T(1e-3);
            continue;
        }

        T dr = (-R1*c4 - (-R0)*c2) / det;
        T ds = ( c1*(-R0) - c3*(-R1)) / det;

        r += dr;
        s += ds;

        if (std::abs(dr) < tol && std::abs(ds) < tol)
            return true;
    }
    return false;
}

// Main: find all roots of real polynomial P(z) degree n, coeffs c[0..n].
// No heap, real-time safe, bounded work.
template<typename T, int MAX_DEG>
inline RootsResult<T, MAX_DEG> find_roots_real_poly(
    const std::array<T, MAX_DEG+1>& coeffs, int degree,
    int maxBairstowIters = 24, T tol = T(1e-6))
{
    RootsResult<T, MAX_DEG> out;
    int n = degree;
    if (n <= 0) return out;

    // Working polynomial (deflated as we go)
    std::array<T, MAX_DEG+1> c = coeffs;

    // Normalize leading coefficient to 1 for stability
    if (std::abs(c[0]) > T(0)) {
        T inv = T(1) / c[0];
        for (int i = 0; i <= n; ++i) c[i] *= inv;
    }

    std::array<T, MAX_DEG+1> b{};
    std::array<T, MAX_DEG+1> d{};

    while (n > 2) {
        // Initial guesses (good generic starting point for LPC-ish polynomials)
        T r = T(-0.5);
        T s = T(0.25);

        bool ok = bairstow_factor<T, MAX_DEG>(c, n, r, s, b, d, maxBairstowIters, tol);

        // If failed, try a couple alternate seeds deterministically (still bounded)
        if (!ok) {
            r = T(0.0); s = T(0.5);
            ok = bairstow_factor<T, MAX_DEG>(c, n, r, s, b, d, maxBairstowIters, tol);
        }
        if (!ok) {
            r = T(-1.2); s = T(0.8);
            ok = bairstow_factor<T, MAX_DEG>(c, n, r, s, b, d, maxBairstowIters, tol);
        }

        // If still failed, break out with what we have (rare for typical LPC orders)
        if (!ok) break;

        // Extract quadratic roots
        std::complex<T> z1, z2;
        quadratic_roots(r, s, confirming(z1), confirming(z2));
        out.roots[out.n++] = z1;
        out.roots[out.n++] = z2;

        // Deflate: new polynomial is b[0..n-2] (synthetic division result)
        // b was computed so that:
        // P(z) = (z^2 + r z + s) * Q(z) + remainder
        // Q coefficients are b[0..n-2]
        std::array<T, MAX_DEG+1> cNew{};
        for (int i = 0; i <= n-2; ++i) cNew[i] = b[i];
        c = cNew;
        n -= 2;
    }

    // Handle remaining degree 2 or 1
    if (n == 2) {
        // c[0] z^2 + c[1] z + c[2] = 0 ; c[0] should be 1 after normalization/deflation
        T r = c[1] / c[0];
        T s = c[2] / c[0];
        std::complex<T> z1, z2;
        quadratic_roots(r, s, z1, z2);
        out.roots[out.n++] = z1;
        out.roots[out.n++] = z2;
    } else if (n == 1) {
        // c[0] z + c[1] = 0
        out.roots[out.n++] = std::complex<T>(-c[1] / c[0], T(0));
    }

    // Clamp count
    out.n = std::min(out.n, MAX_DEG);
    return out;
}

} // namespace lpc
