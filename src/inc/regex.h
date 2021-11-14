/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#pragma once
#include<vector>
#include "../../CRoaring/roaring.hh"
#include "../../CRoaring/roaring.c"
namespace Regex{
    typedef struct State{
        Roaring tr[512]={};         //0-0xFF         forward transition
                                    //0x100-0x1FF    backward transition(relevant for FRegex iterator dereference) Also relevant for "quick" NFA collocation
        State (){}
        State& operator=(const State& s){
            for(int c=0;c<512;c++) tr[c]=s.tr[c];
            return *this;
        }
        State& operator=(State&& s){
            for(int c=0;c<512;c++) tr[c]=std::move(s.tr[c]);
            return *this;
        }
        State (const State& s){
            for(int c=0;c<512;c++) tr[c]=s.tr[c];
        }
        State (State&& s){
            for(int c=0;c<512;c++) tr[c]=std::move(s.tr[c]);
        }
    }state;
    enum operation{CONCATENATION,BRACKETS,OR};
    class NFA{
        template<bool fwd> NFA& shift(char c);
        template<bool fwd> void skip(uint32_t n,uint32_t k);
    public:
        bool start_early=false;
        std::vector<state>  states;
        Roaring    current_states;
        Roaring    final_states;
        uint32_t   initial_state;
        uint32_t*  gather_for_fastunion=nullptr;       //TODO make class
        Roaring**  select_for_fastunion=nullptr;       //TODO make struct
        /*
        const std::unordered_map<operation, nfa_operation> op_eval {
                {CONCATENATION, this.operator*=},
                {OR, this.operator|=}
        };*/
        NFA(const NFA& to_copy);
        NFA(NFA&& to_move);
        NFA(uint32_t cur_n);                                    //create empty NFA
        NFA(uint32_t cur_n,char c);                             //create NFA accepting just c
        NFA& operator=(NFA& other);
        NFA& operator=(NFA&& other);
        static NFA build_NFA(const char* p);
        static NFA bracket_expression(const char* start, int* i,uint32_t* cur_n,int ps,const char *p);
        NFA& operator|=(NFA&& other){return *this|=other;};     //Union
        NFA& operator*=(NFA&& other){return *this*=other;};     //Concatenation
        NFA& operator|=(NFA& other);                            //Union
        NFA& operator*=(NFA& other);                            //Concatenation
        NFA& operator* (unsigned int n);    //Kleene operator
        #define FLAG_ACCEPTING   (1<<1)
        #define FLAG_INITIAL     (1<<2)
        uint8_t operator*();
        NFA& operator<<(char c);            //process forward
        NFA& operator>>(char c);            //process in reverse order
        ~NFA();
    };

    //NFA operator+(NFA& nfa,int rotate);
    /*
    class FRegexIterator{
        NFA nfa;
        unsigned int cur=0;
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = size_t;
        using value_type        = char*;
        using pointer           = char**;  // or also value_type*
        using reference         = (char*)&;  // or also value_type&
    };*/
}
