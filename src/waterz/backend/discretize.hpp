#ifndef WATERZ_DISCRETIZE_H__
#define WATERZ_DISCRETIZE_H__

template <typename To, typename From, typename LevelsType>
inline To discretize(From value, LevelsType levels) {

	// clamp before the conversion, which is undefined for a value that does not
	// fit into `To` (and for NaN, which goes to the last level)
	if (!(value < 1))
		return (To)(levels-1);
	if (value <= 0)
		return 0;

	return std::min((To)(value*levels), (To)(levels-1));
}

template <typename To, typename From, typename LevelsType>
inline To undiscretize(From value, LevelsType levels) {

	return ((To)value + 0.5)/levels;
}

#endif // WATERZ_DISCRETIZE_H__

