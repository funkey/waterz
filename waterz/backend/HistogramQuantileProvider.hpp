#ifndef WATERZ_HISTOGRAM_QUANTILE_PROVIDER_H__
#define WATERZ_HISTOGRAM_QUANTILE_PROVIDER_H__

#include "Histogram.hpp"

/**
 * A quantile provider using histograms to find an approximate quantile. This 
 * assumes that all values are in the range [0,1].
 */
template <int Q, typename Precision, int Bins = 256>
class HistogramQuantileProvider {

public:

	void add(Precision value) {

		_histogram.inc((int)(value*(Bins-1)));
	}

	Precision value() {

		// pivot element, 1-based index
		int pivot = Q*_histogram.sum()/100 + 1;

		int sum = 0;
		int bin = 0;
		for (bin = 0; bin < Bins; bin++) {

			sum += _histogram[bin];

			if (sum >= pivot)
				break;
		}

		return (Precision)bin/(Bins-1);
	}

	template <int OtherQ, typename OtherPrecision>
	void merge(const HistogramQuantileProvider<OtherQ, OtherPrecision, Bins>& other) {

		_histogram += other._histogram;
	}

	void clear() {

		_histogram.clear();
	}

private:

	Histogram<Bins> _histogram;
};

template <int Bins>
struct HistogramQuantileProviderSelect {

	template <int Q, typename Precision>
	using Value = HistogramQuantileProvider<Q, Precision, Bins>;
};

#endif // WATERZ_HISTOGRAM_QUANTILE_PROVIDER_H__

