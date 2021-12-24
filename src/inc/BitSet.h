/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#pragma once
#include<immintrin.h>
#include<cstdint>
// uncompressed bitmap implementation for small state spaces

template<int words_n>
class BitSet{
    alignas(words_n*8) uint64_t words[words_n]={0};
public:
    BitSet(){};
    BitSet<words_n>& operator |=(BitSet<words_n> other);
    BitSet<words_n>& operator &=(BitSet<words_n> other);
    uint32_t cardinality() const;
    class iterator{
        alignas(words_n*8) uint64_t words[words_n]={0}; //TODO align whole memory_pool
        uint8_t cur_b=0;
    public:
        iterator(uint64_t* s);
        iterator(BitSet<words_n> s);
        void reinit(uint64_t* other_words);
        bool operator!=(BitSet<words_n>::iterator i2);
        int32_t operator++();
        int32_t operator*();
    };
    void add(uint32_t t);
    size_t and_cardinality(BitSet<words_n> new_s);
    bool contains(uint32_t t);
    void complement();
    BitSet<words_n>::iterator begin();
    BitSet<words_n>::iterator end();
    BitSet<words_n> operator+(int32_t rotate);
    void printf() const ;
};
