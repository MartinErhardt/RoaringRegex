/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#include"BitSet.h"
#include<immintrin.h>
#include <iostream>
// uncompressed bitmap implementation for small state spaces
template<int words_n>
BitSet<words_n>& BitSet<words_n>::operator|=(BitSet<words_n> other){
    if constexpr(words_n==1) words[0]|=other.words[0];
    else if(words_n==2){
        __m128i s1 = _mm_loadu_si128((__m128i const *)&words[0]);
        __m128i s2 = _mm_loadu_si128((__m128i const *)&other.words[0]);
        _mm_store_si128((__m128i *)&words[0], _mm_or_si128(s1,s2));
    }else if(words_n==4){
        __m256i s1 = _mm256_loadu_si256((__m256i const *)&words[0]);
        __m256i s2 = _mm256_loadu_si256((__m256i const *)&other.words[0]);
        _mm256_store_si256((__m256i *)&words[0], _mm256_or_si256 (s1, s2));
    }
    return *this;
}
template<int words_n>
BitSet<words_n>& BitSet<words_n>::operator&=(BitSet<words_n> other){
    if constexpr(words_n==1) words[0]&=other.words[0];
    else if(words_n==2){
        __m128i s1 = _mm_loadu_si128((__m128i const *)&words[0]);
        __m128i s2 = _mm_loadu_si128((__m128i const *)&other.words[0]);
        _mm_store_si128((__m128i *)&words[0], _mm_and_si128(s1,s2));
    }else if(words_n==4){
        __m256i s1 = _mm256_loadu_si256((__m256i const *)&words[0]);
        __m256i s2 = _mm256_loadu_si256((__m256i const *)&other.words[0]);
        _mm256_store_si256((__m256i *)&words[0], _mm256_and_si256 (s1, s2));
    }
    return *this;
}
template<int words_n>
uint32_t BitSet<words_n>::cardinality() const{
    uint64_t ret_val=0;
    for(size_t i=0;i<words_n;i++) ret_val+=_popcnt64(words[i]);
    return ret_val;
}
template<int words_n>
void BitSet<words_n>::complement(){
    alignas(words_n*8) uint64_t mask[words_n];
    for(int i=0;i<words_n;i++) mask[i]=(uint64_t)-1ll;
    if constexpr(words_n==1) words[0]^=mask[0];
    else if(words_n==2){
        __m128i m = _mm_loadu_si128((__m128i const *)&mask);
        __m128i s1 = _mm_loadu_si128((__m128i const *)&words[0]);
        _mm_store_si128((__m128i *)&words[0], _mm_xor_si128(s1,m));
    }else if(words_n==4){
        __m256i s1 = _mm256_loadu_si256((__m256i const *)&words[0]);
        __m256i m = _mm256_loadu_si256((__m256i const *)&mask);
        _mm256_store_si256((__m256i *)&words[0], _mm256_xor_si256(s1,m));
    }
}
template<int words_n>
BitSet<words_n>::iterator::iterator(BitSet<words_n> s){
    for(size_t i=0;i<words_n;i++) words[i]=s.words[i];
}
template<int words_n>
BitSet<words_n>::iterator::iterator(uint64_t* w){
    for(size_t i=0;i<words_n;i++) words[i]=w[i];
}
template<int words_n>
void BitSet<words_n>::iterator::reinit(uint64_t* other_words){
    for(size_t i=0;i<words_n;i++) words[i]=other_words[i];
}
template<int words_n>
bool BitSet<words_n>::iterator::operator!=(BitSet<words_n>::iterator i2){
    bool ret =false;
    for(int i=0;i<words_n;i++) ret=ret||(words[i]!=i2.words[i]);
    return ret;
}
template<int words_n>
int32_t BitSet<words_n>::iterator::operator++(){
    uint64_t* cur_w=&words[cur_b>>6];
    while(cur_w<&words[0]+words_n-1&&!*cur_w) cur_w++;
    if(cur_w==&words[0]+words_n-1&&!*cur_w) return -1;
    uint64_t highest_set_bit = (*cur_w) & -(*cur_w);
    cur_b = (cur_w-&words[0])*sizeof(uint64_t)*8+((uint8_t) __builtin_ctzl(*cur_w));
    (*cur_w)^=highest_set_bit;
    return (int32_t) cur_b;
}
template<int words_n>
int32_t BitSet<words_n>::iterator::operator*(){
    return (int32_t) cur_b;
}
template<int words_n>
void BitSet<words_n>::add(uint32_t t){
    uint64_t t64=(uint64_t) t;
    int w_n=t>>6;
    words[w_n]|=(1ULL<<(t64&0x3fULL));
}
template<int words_n>
size_t BitSet<words_n>::and_cardinality(BitSet<words_n> new_s){
    BitSet<words_n> plus_s(new_s);
    plus_s&=*this;
    return plus_s.cardinality();
}
template<int words_n>
bool BitSet<words_n>::contains(uint32_t t){
    uint64_t t64=(uint64_t) t;
    int w_n=t>>6;
    return (words[w_n]&(1ULL<<(t64&0x3f)))>>(t64&0x3f);
}
template<int words_n>
typename BitSet<words_n>::iterator BitSet<words_n>::begin(){
    return BitSet<words_n>::iterator(&words[0]);
}
template<int words_n>
typename BitSet<words_n>::iterator BitSet<words_n>::end(){
    uint64_t zero_words[words_n]={0};
    return BitSet<words_n>::iterator(&zero_words[0]);
}
//https://stackoverflow.com/questions/17610696/shift-a-m128i-of-n-bits
#define SHL128(v, n)                        \
({                                          \
    __m128i v1,v2;                          \
    if((n)>=64){                            \
        v1=_mm_slli_si128(v,8);             \
        v1=_mm_slli_epi64(v1,(n)-64);       \
    }else{                                  \
        v1=_mm_slli_epi64(v,n);             \
        v2=_mm_slli_si128(v,8);             \
        v2=_mm_srli_epi64(v2,64-(n));       \
        v1=_mm_or_si128(v1,v2);             \
    }                                       \
    v1;                                     \
})
//https://stackoverflow.com/questions/20775005/8-bit-shift-operation-in-avx2-with-shifting-in-zeros
template  <unsigned int N> 
__m256i _mm256_shift_left(__m256i a)
{
    __m256i mask=_mm256_permute2x128_si256(a,a,_MM_SHUFFLE(0,0,3,0));
    return _mm256_alignr_epi8(a,mask,16-N);
}
#define SHL256(v, n)                        \
({                                          \
    __m256i v1,v2;                          \
    if(n>=192){                             \
        v1=_mm256_shift_left<16>(v);        \
        v1=_mm256_shift_left<8>(v1);        \
        v1= _mm256_slli_epi64(v1,(n)-192);  \
    }else if(n>=128){                       \
        v1=_mm256_shift_left<16>(v);        \
        v1=_mm256_slli_epi64(v1,(n)-128);   \
        v2=_mm256_shift_left<16>(v);        \
        v2=_mm256_shift_left<8>(v2);        \
        v2=_mm256_srli_epi64(v2,192-(n));   \
        v1=_mm256_or_si256(v1,v2);          \
    }else if(n>=64){                        \
        v1=_mm256_shift_left<8>(v);         \
        v1=_mm256_slli_epi64(v1,(n)-64);    \
        v2=_mm256_shift_left<16>(v);        \
        v2=_mm256_srli_epi64(v2,128-(n));   \
        v1=_mm256_or_si256(v1,v2);          \
    }else{                                  \
        v1=_mm256_slli_epi64(v,n);          \
        v2=_mm256_shift_left<8>(v);         \
        v2=_mm256_srli_epi64(v2,64-(n));    \
        v1=_mm256_or_si256(v1,v2);          \
    }                                       \
    v1;                                     \
})

template<int words_n>
BitSet<words_n> BitSet<words_n>::operator+(int32_t rotate){
    BitSet<words_n> ret;
    if constexpr(words_n==1) ret.words[0]=(words[0]<<rotate);
    else if(words_n==2){
        __m128i s=_mm_loadu_si128((__m128i const *)&words[0]);
        _mm_store_si128((__m128i *)&ret.words[0], SHL128(s, rotate));
    }else if(words_n==4){
        __m256i s=_mm256_loadu_si256((__m256i const *)&words[0]);
        __m256i a=SHL256(s, rotate);
        _mm256_store_si256((__m256i *)&ret.words[0], a);
    }
    return ret;
}
template<int words_n>
void BitSet<words_n>::printf() const {
    BitSet<words_n>::iterator i(*this);
    std::cout<<"{";
    while(++i>=0) std::cout<<*i<<",";
    std::cout<<"\b}";
}
template class BitSet<1>::iterator;
template class BitSet<2>::iterator;
template class BitSet<4>::iterator;
template class BitSet<1>;
template class BitSet<2>;
template class BitSet<4>;
