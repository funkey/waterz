#ifndef WATERZ_CONSTRAINT_PROVIDER_H__
#define WATERZ_CONSTRAINT_PROVIDER_H__

#include <cstdint>

/**
 * Base class for statistics providers with fallback implementations.
 */
class ConstraintProvider {
public:
	virtual inline bool notifyNodeMerge(uint64_t from, uint64_t to) = 0;
	virtual inline bool isConstrained(uint64_t from, uint64_t to, float score) const = 0;
};

#endif // WATERZ_CONSTRAINT_PROVIDER_H__
