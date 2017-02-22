#ifndef HISTOGRAM_H__
#define HISTOGRAM_H__

#include <vector>

template <int Bins, typename T = int>
class Histogram {

public:

	Histogram operator+(const Histogram& other) {

		Histogram result(*this);
		result += other;
		return result;
	}

	Histogram& operator+=(const Histogram& other) {

		for (int i = 0; i < Bins; i++)
			_bins[i] += other._bins[i];
		_sum += other._sum;
		return *this;
	}

	void inc(int i) { _bins[i]++; _sum++; }

	const T& operator[](int i) const { return _bins[i]; }

	T sum() const { return _sum; }

private:

	T _bins[Bins];
	T _sum;
};

#endif // HISTOGRAM_H__

