from .operators import ScoringFunction


class MinSize(ScoringFunction):
    '''The size of the smaller fragment to be merged.
    '''
    def __repr__(self):
        return 'MinSize<RegionGraphType>'


class MaxSize(ScoringFunction):
    '''The size of the larger fragment to be merged.
    '''
    def __repr__(self):
        return 'MaxSize<RegionGraphType>'


class MinAffinity(ScoringFunction):
    '''The minimal affinity value between fragments.
    '''
    def __repr__(self):
        return 'MinAffinity<RegionGraphType, ScoreValue>'


class MaxAffinity(ScoringFunction):
    '''The maximal affinity value between fragments.
    '''
    def __repr__(self):
        return 'MaxAffinity<RegionGraphType, ScoreValue>'


class MeanAffinity(ScoringFunction):
    '''The mean affinity value between fragments.
    '''
    def __repr__(self):
        return 'MeanAffinity<RegionGraphType, ScoreValue>'


class MeanMaxKAffinity(ScoringFunction):
    '''The mean affinity value of the k largest values between fragments.
    '''

    def __init__(self, k):
        self.k = k

    def __repr__(self):

        return f'MeanMaxKAffinity<RegionGraphType, {self.k}, ScoreValue>'


class QuantileAffinity(ScoringFunction):
    '''The q-th quantile of the affinity values between fragments.

    Args:

        q (``int``):

              The quantile as an integer between 0 (min) and 100 (max).

        init_with_max (``bool``, optional):

              Use the maximal affinity value to score initial edges (instead of
              computing the quantile between all values directly). True by
              default.

        histogram_bins (``int``, optional):

              If given, use a histogram to approximate the exact quantile
              value with that many bins. Default is to not use a histogram but
              a partially sorted vector to find the quantile (exact but
              slower).
    '''

    def __init__(self, q, init_with_max=True, histogram_bins=None):
        self.q = q
        self.histogram_bins = histogram_bins
        self.init_with_max = 'true' if init_with_max else 'false'

    def __repr__(self):

        if self.histogram_bins:
            return f'HistogramQuantileAffinity<RegionGraphType, {self.q}, ' \
                   f'ScoreValue, {self.histogram_bins}, {self.init_with_max}>'
        else:
            return f'QuantileAffinity<RegionGraphType, {self.q}, ' \
                   f'ScoreValue, {self.init_with_max}>'


class MedianAffinity(QuantileAffinity):
    '''The 50-th quantile of the affinity values between fragments. Equivalent
    to ``QuantileAffinity(50, histogram_bins=256)``.'''

    def __init__(self):
        super(MedianAffinity, self).__init__(50, histogram_bins=256)


class ContactArea(ScoringFunction):
    '''The contact area between fragments (number of affinity edges).'''

    def __repr__(self):
        return 'ContactArea<RegionGraphType>'


class Random(ScoringFunction):
    '''A random number between 0 and 1.'''

    def __repr__(self):
        return 'Random<RegionGraphType>'


class Constant(ScoringFunction):
    '''An integer constant.'''

    def __init__(self, value):
        self.value = value

    def __repr__(self):
        return f'Constant<RegionGraphType, {self.value}>'
