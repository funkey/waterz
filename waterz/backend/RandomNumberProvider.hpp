#ifndef WATERZ_BACKEND_RANDOM_NUMBER_PROVIDER_H__
#define WATERZ_BACKEND_RANDOM_NUMBER_PROVIDER_H__

#include <random>
#include "StatisticsProvider.hpp"

class RandomNumberProvider : public StatisticsProvider {

public:

	typedef float ValueType;

	template <typename RegionGraphType>
	RandomNumberProvider(RegionGraphType&) {}

	ValueType operator()() const {

		return ValueType(rand())/RAND_MAX;
	}
};

#endif // WATERZ_BACKEND_RANDOM_NUMBER_PROVIDER_H__

