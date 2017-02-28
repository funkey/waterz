#ifndef WATERZ_VECTOR_QUANTILE_PROVIDER_H__
#define WATERZ_VECTOR_QUANTILE_PROVIDER_H__

/**
 * A quantile provider using std::vector and std::nth_element to find the exact 
 * quantile.
 */
template <int Q, typename Precision>
class VectorQuantileProvider {

public:

	void add(Precision value) {

		_values.push_back(value);
	}

	Precision value() {

		auto quantile = getQuantileIterator(_values.begin(), _values.end(), Q);
		std::nth_element(_values.begin(), quantile, _values.end());

		return _values[*quantile];
	}

	template <int OtherQ, typename OtherPrecision>
	void merge(const VectorQuantileProvider<OtherQ, OtherPrecision>& other) {

		_values.reserve(_values.size() + other._values.size());

		auto otherQuantile = getQuantileIterator(other._values.begin(), other._values.end(), OtherQ);
		_values.insert(_values.begin(), other._values.begin(), otherQuantile);
		_values.insert(_values.end(), otherQuantile, other._values.end());
	}

	void clear() {

		_values.clear();
	}

private:

	template <typename It>
	inline It getQuantileIterator(It begin, It end, int q) {

		return begin + (end-begin-1)*q/100;
	}

	std::vector<Precision> _values;
};

#endif // WATERZ_VECTOR_QUANTILE_PROVIDER_H__

