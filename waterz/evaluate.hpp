#include <iostream>
#include <tuple>
#include <map>
#include <math.h> 

using namespace std;

template <typename V1, typename V2>
std::tuple<double,double,double,double>
compare_volumes(
                 const V1& gt,
                 const V2& ws, size_t dimX, size_t dimY, size_t dimZ ){
    double rand_split = 0;
    double rand_merge = 0;

    double t_sq = 0;
    double s_sq = 0;

    double total = 0;
    std::map<uint64_t, std::map<uint64_t, std::size_t>> p_ij;

    std::map<uint64_t, std::size_t> s_i, t_j;

    for ( std::ptrdiff_t z = 0; z < dimZ; ++z )
        for ( std::ptrdiff_t y = 0; y < dimY; ++y )
            for ( std::ptrdiff_t x = 0; x < dimX; ++x )
            {
                uint64_t wsv = ws[x][y][z];
                uint64_t gtv = gt[x][y][z];

                if ( gtv )
                {
                    ++total;

                    ++p_ij[gtv][wsv];
                    ++s_i[wsv];
                    ++t_j[gtv];
                }
            }

    double sum_p_ij = 0;
    for ( auto& a: p_ij )
        for ( auto& b: a.second )
            sum_p_ij += b.second * b.second;
        
    

    double sum_t_k = 0;
    for ( auto& a: t_j )
        sum_t_k += a.second * a.second;
    

    double sum_s_k = 0;
    for ( auto& a: s_i )
        sum_s_k += a.second * a.second;
    

    double sum_log_p_ij = 0;
    for ( auto& a: p_ij )
        for ( auto& b: a.second )
            if(b.second)
                sum_log_p_ij += b.second * log2(b.second);
        
    
    
    double sum_log_t = 0;
    for ( auto& a: t_j )
        if(a.second)
            sum_log_t -= a.second * log2(a.second);
    

    double sum_log_s = 0;
    for ( auto& a: s_i )
        if(a.second)
            sum_log_s -= a.second * log2(a.second);
    
    double info = sum_log_p_ij + sum_log_s + sum_log_t;
    cout << "info " << info << endl;
    cout << "sum_log_p " << sum_log_p_ij << endl;
    cout << "sum_log_t " << sum_log_t << endl;
    cout << "sum_log_s " << sum_log_s << endl;
    
    //std::cout << sum_p_ij << "\n";
    std::cout << "\tRand Split: " << (sum_p_ij/sum_t_k) << "\n";
    std::cout << "\tRand Merge: " << (sum_p_ij/sum_s_k) << "\n";
    

    return std::make_tuple(sum_p_ij/sum_t_k,
                          sum_p_ij/sum_s_k,info/sum_log_s,info/sum_log_t);
}

