#ifndef WATERZ_HISTOGRAM_QUANTILE_PROVIDER_H__
#define WATERZ_HISTOGRAM_QUANTILE_PROVIDER_H__

#include "StatisticsProvider.hpp"
#include "Histogram.hpp"
#include "discretize.hpp"

/**
 * A quantile provider using histograms to find an approximate quantile. This 
 * assumes that all values are in the range [0,1].
 */
template <typename RegionGraphType, int Q, typename Precision, int Bins = 256, bool InitWithMax = true>
class HistogramQuantileProvider : public StatisticsProvider {

public:

	typedef Precision ValueType;
	typedef typename RegionGraphType::EdgeIdType EdgeIdType;

	HistogramQuantileProvider(RegionGraphType& regionGraph) :
		_histograms(regionGraph) {}

	void addAffinity(EdgeIdType e, ValueType affinity) {

		auto edge = _histograms.getRegionGraph().edge(e);
		if (edge.u == 2 && edge.v == 10)
			std::cout << "edge " << edge.u << " " << edge.v << " add aff " << affinity << std::endl;

		int bin = discretize<int>(affinity, Bins);

		if (edge.u == 2 && edge.v == 10)
			std::cout << "    bin " << bin << std::endl;

		if (InitWithMax && _histograms[e].lowestBin() != Bins) {

			if (bin > _histograms[e].lowestBin()) {

				if (edge.u == 2 && edge.v == 10)
					std::cout << "    higher than previous" << std::endl;

				_histograms[e].clear();

			} else
				return;
		}

		if (edge.u == 2 && edge.v == 10)
			std::cout << "    inc bin " << bin << std::endl;

		_histograms[e].inc(bin);
	}

	void notifyEdgeMerge(EdgeIdType from, EdgeIdType to) {

		_histograms[to] += _histograms[from];
		_histograms[from].clear();
	}

	ValueType operator[](EdgeIdType e) const {

		auto edge = _histograms.getRegionGraph().edge(e);

		// pivot element, 1-based index
		int pivot = Q*_histograms[e].sum()/100 + 1;

		if (edge.u == 2 && edge.v == 10)
			std::cout << "querying quantile " << Q << " at pivot " << pivot << std::endl;

		int sum = 0;
		int bin = 0;
		for (bin = 0; bin < Bins; bin++) {

			sum += _histograms[e][bin];

			if (sum >= pivot)
				break;
		}

		if (edge.u == 2 && edge.v == 10)
			std::cout << "    bin is " << bin << std::endl;
		if (edge.u == 2 && edge.v == 10)
			std::cout << "    score is " << undiscretize<Precision>(bin, Bins) << std::endl;

		return undiscretize<Precision>(bin, Bins);
	}

private:

	typename RegionGraphType::template EdgeMap<Histogram<Bins>> _histograms;
};

#endif // WATERZ_HISTOGRAM_QUANTILE_PROVIDER_H__

