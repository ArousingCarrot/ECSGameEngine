#pragma once
#include <vector>
#include <algorithm>
#include <cstdint>
#include <numeric>
#include <cmath>

namespace diag {

    template <typename T>
    class RollingWindow {
    public:
        explicit RollingWindow(size_t cap = 600) : cap_(cap) { data_.reserve(cap_); }
        void clear() { data_.clear(); }
        void push(T v) {
            if (data_.size() < cap_) data_.push_back(v);
            else { head_ = (head_ + 1) % cap_; data_[head_] = v; }
            if (data_.size() == cap_) filled_ = true;
        }
        // Returns a copy in logical order
        std::vector<T> snapshot() const {
            std::vector<T> out;
            if (data_.empty()) return out;
            out.reserve(size());
            if (!filled_) { out = data_; return out; }
            // head_ points at the oldest replaced; logical start is (head_+1) % cap
            size_t start = (head_ + 1) % cap_;
            for (size_t i = 0; i < cap_; ++i) out.push_back(data_[(start + i) % cap_]);
            return out;
        }
        size_t size() const { return filled_ ? cap_ : data_.size(); }
        bool empty() const { return size() == 0; }
        size_t capacity() const { return cap_; }
    private:
        size_t cap_;
        std::vector<T> data_;
        size_t head_ = 0;
        bool filled_ = false;
    };

    struct Percentiles {
        double p50 = 0, p95 = 0, p99 = 0, q1 = 0, q3 = 0, iqr = 0;
    };

    inline Percentiles compute_percentiles(const std::vector<double>& xs) {
        Percentiles p{};
        if (xs.empty()) return p;
        std::vector<double> v = xs;
        std::sort(v.begin(), v.end());
        auto at = [&](double q)->double {
            if (v.empty()) return 0;
            double idx = q * (v.size() - 1);
            size_t i = (size_t)idx;
            size_t j = std::min(i + 1, v.size() - 1);
            double t = idx - i;
            return v[i] * (1.0 - t) + v[j] * t;
            };
        p.p50 = at(0.50);
        p.p95 = at(0.95);
        p.p99 = at(0.99);
        p.q1 = at(0.25);
        p.q3 = at(0.75);
        p.iqr = p.q3 - p.q1;
        return p;
    }

    inline bool is_tukey_outlier(double x, const Percentiles& p) {
        double lo = p.q1 - 1.5 * p.iqr;
        double hi = p.q3 + 1.5 * p.iqr;
        return (x < lo) || (x > hi);
    }

}
