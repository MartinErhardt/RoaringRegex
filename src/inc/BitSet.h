/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#pragma once
#include<immintrin.h>
#include<cstdint>
// uncompressed bitmap implementation for small state spaces

template<int words_n>
class BitSet{
public:
    alignas(words_n*8) uint64_t words[words_n]={0};
    //BitSet(BitSet<words_n>& other){for(size_t i=0;i<words_n;i++) words[i]=other.words[i];};
    //BitSet(BitSet<words_n>&& other){for(size_t i=0;i<words_n;i++) words[i]=other.words[i];};
    BitSet(){};
    BitSet<words_n>& operator |=(BitSet<words_n> other);
    BitSet<words_n>& operator &=(BitSet<words_n> other);
    uint32_t cardinality() const;
    //template<int words_n>
    class const_iterator{
        const BitSet<words_n>& to_iterate;
        int cur_b;
        uint64_t  cur_w;
        
        //typedef typename BitSet<words_n>::const_iterator const_iter;
    public:
        const_iterator(const BitSet<words_n>& to_iterate, int cur_b): to_iterate(to_iterate), cur_b(cur_b){
            if(cur_b==-1) cur_w = 0;
            else cur_w = to_iterate.words[cur_b>>6];
        };
        bool operator<(const_iterator i2);
        bool operator!=(const_iterator i2);
        bool operator==(const_iterator i2);
        typename BitSet<words_n>::const_iterator& operator++();
        int operator*();
    };
    void add(uint32_t t);
    size_t and_cardinality(BitSet<words_n> new_s);
    bool contains(uint32_t t);
    void complement();
    typename BitSet<words_n>::const_iterator begin() const;
    typename BitSet<words_n>::const_iterator end() const;
    BitSet<words_n> operator+(int32_t rotate);
    void printf() const ;
};
