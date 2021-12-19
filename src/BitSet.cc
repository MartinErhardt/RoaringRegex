/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#include<immintrin.h>
// uncompressed bitmap implementation for small state spaces
template<int words_n>
class BitSet{
    uint64_t words[words_n]={0};
public:
    BitSet<words_n>& operator |=(BitSet<words_n> other){
        if constexpr(words_n==1) words[0]|=other.words[0];
        else if(words_n==2){
            __m128i s1 = _mm_load_ps(&words[0]);
            __m128i s2 = _mm_load_ps(&other.words[0]);
            _mm_store_ps(&words[0], _mm_or_si128(s1,s2));
        }else if(words_n==4){
            __m256i s1 = _mm256_lddqu_si256(&words[0]);
            __m256i s2 = _mm256_lddqu_si256(&other.words[0]);
            _mm256_store_si256(&words[0], _mm256_or_si256 (s1, s2));
        }
        return *this;
    }
    BitSet<words_n>& operator &=(BitSet<words_n> other){
        if constexpr(words_n==1) words[0]&=other.words[0];
        else if(words_n==2){
            __m128i s1 = _mm_load_ps(&words[0]);
            __m128i s2 = _mm_load_ps(&other.words[0]);
            _mm_store_ps(&words[0], _mm_and_si128(s1,s2));
        }else if(words_n==4){
            __m256i s1 = _mm256_lddqu_si256(&words[0]);
            __m256i s2 = _mm256_lddqu_si256(&other.words[0]);
            _mm256_store_si256(&words[0], _mm256_and_si256 (s1, s2));
        }
        return *this;
    }
    uint32_t cardinality(){
        uint32_t ret_val=0;
        for(size_t i=0;i<words_n;i++) ret_val+=__builtin_popcount(words[i]);
        return ret_val;
    }
    class iterator{
        uint64_t words[words_n]={0};
        uint8_t cur_b=0;
    public:
        iterator(uint64_t* other_words){
            for(size_t i=0;i<words_n;i++) words[i]=other_words[i];
        }
        int32_t operator++(){
            uint64_t* cur_w=&words[cur_b>>6];
            while(cur_w<&words[0]+words_n&&!*cur_w) cur_w++;
            if(cur_w==&words[0]+words_n&&!*cur_w) return -1;
            uint64_t highest_set_bit = (*cur_w) & -(*cur_w);
            uint32_t cur_b = __builtin_ctzl(cur_w);
            *cur_w^=highest_set_bit;
            return (int32_t) cur_b;
        }
    };
};
