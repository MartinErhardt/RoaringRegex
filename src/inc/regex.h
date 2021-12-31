/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#pragma once
#include<vector>
#include<memory>
#include "../../CRoaring/roaring.hh"
#include<iostream>
#include"BitSet.h"
namespace Regex{
    enum operation{CONCATENATION,BRACKETS,OR};
    class PseudoNFA{
    public:
        uint32_t size;
        uint32_t initial_state;
        PseudoNFA(){};
        PseudoNFA(const PseudoNFA& to_copy){size=to_copy.size;initial_state=to_copy.initial_state;}
        PseudoNFA(PseudoNFA&& to_move){size=to_move.size; initial_state=to_move.initial_state; to_move.size=0;}
        PseudoNFA(uint32_t cur_n,uint32_t states_n):size(states_n),initial_state(cur_n){};
        PseudoNFA(uint32_t cur_n,uint32_t states_n,void* tr_table){size=1;initial_state=cur_n;};         //create empty NFA
        PseudoNFA(uint32_t cur_n,char c,uint32_t states_n,void* tr_table){size=2;initial_state=cur_n;};  //create NFA accepting just c
        PseudoNFA(uint32_t cur_n,BitSet<2>& cs,uint32_t states_n,void* states){size=2;initial_state=cur_n;};
        PseudoNFA& operator=(PseudoNFA& other){size=other.size;initial_state=other.initial_state; return *this;}
        PseudoNFA& operator=(PseudoNFA&& other){size=other.size;other.size=0; initial_state=other.initial_state; return *this;}
        PseudoNFA& operator|=(PseudoNFA&& other){return *this|=other;};             //Union
        PseudoNFA& operator*=(PseudoNFA&& other){return *this*=other;};             //Concatenation
        PseudoNFA& operator|=(PseudoNFA& other){size+=other.size;return *this;};    //Union
        PseudoNFA& operator*=(PseudoNFA& other){size+=other.size;return *this;};    //Concatenation
        PseudoNFA& operator* (unsigned int n){return *this;};                       //Kleene operator
    };
    class Executable{
        void* memory_pool=nullptr;
        size_t memory_pool_size=0;
        bool moved_from = false;
        #define FLAG_ACCEPTING   (1<<0)
        #define FLAG_INITIAL     (1<<1)
    public: 
        size_t states_n=0;
        virtual uint8_t operator*()            = 0;
        virtual Executable& operator<<(char c) = 0;            //process forward
        virtual Executable& operator>>(char c) = 0;            //process in reverse order
        virtual void reset()                   = 0;
    protected:
        uint32_t*  gather_for_fastunion=nullptr;                // owned by memory_pool
        Roaring**   select_for_fastunion=nullptr;               // owned by memory_pool
    public: //TODO make private
            Executable(){}
            Executable(size_t states_n_arg){states_n=states_n_arg;};
            virtual void print() = 0;
            void init(void* memory_pool_arg, size_t memory_pool_size_arg, size_t states_n_arg){
                states_n=states_n_arg;
                if(states_n>256){
                    gather_for_fastunion=reinterpret_cast<uint32_t*>(static_cast<char*>(memory_pool_arg)+sizeof(Roaring)*states_n);
                    select_for_fastunion=reinterpret_cast<Roaring**>(static_cast<char*>(memory_pool_arg)+sizeof(Roaring)*states_n+sizeof(Roaring*)*states_n);
                }
                memory_pool=memory_pool_arg;
                memory_pool_size=memory_pool_size_arg;
            };
            Executable& operator=(Executable& other)
            /*{
                nfa=other.nfa;
                memory_pool=malloc(other.memory_pool_size);
                memcpy(memory_pool,other.memory_pool,memory_pool_size);
                gather_for_fastunion=other.gather_for_fastunion;
                select_for_fastunion=other.select_for_fastunion;
                return *this;
            }*/
            =delete;
            Executable& operator=(Executable&& other){
                memory_pool=other.memory_pool;
                memory_pool_size=other.memory_pool_size;
                states_n=other.states_n;
                gather_for_fastunion=other.gather_for_fastunion;
                select_for_fastunion=other.select_for_fastunion;
                other.memory_pool=nullptr;
                other.gather_for_fastunion=nullptr;
                other.select_for_fastunion=nullptr;
                other.moved_from=true;
                return *this;
            };
            Executable(Executable& exe)
            =delete;
            Executable(Executable&& exe){(*this)=std::move(exe);};
            virtual ~Executable(){
                if(!moved_from&&memory_pool){
                    if(states_n>256){
                        Roaring* states=(Roaring*)memory_pool;
                        Roaring* cur_state=states;
                        for(;cur_state<states+states_n*0x100;cur_state++) cur_state->~Roaring();
                    }
                    free(memory_pool);
                }
            }
    };
    template<class StateSet>
    class NFA;
    std::ostream& operator<<(std::ostream& out, NFA<Roaring> const& nfa);
    template<class StateSet>
    class NFA: public PseudoNFA,public Executable{
        template<bool fwd> void skip(uint32_t n,uint32_t k);
        template<bool fwd> NFA& shift(char c);
    public:
        StateSet*  states;                                      //owned first by Parser then Executable
        StateSet    current_states[2];
        StateSet    final_states;
        NFA(){};
        NFA(const NFA& to_copy);
        NFA(NFA&& to_move);
        NFA(uint32_t cur_n,uint32_t states_n,void* states);             //create empty NFA
        NFA(uint32_t cur_n,char c,uint32_t states_n,void* states);      //create NFA accepting just c
        NFA(uint32_t cur_n,BitSet<2>& cs,uint32_t states_n,void* states);//create NFA accepting all characters in cs
        void reset(){current_states[0]=final_states;};
        NFA& operator=(NFA& other);
        NFA& operator=(NFA&& other);
        NFA& operator|=(NFA&& other){return *this|=other;};     //Union
        NFA& operator*=(NFA&& other){return *this*=other;};     //Concatenation
        NFA& operator|=(NFA& other);                            //Union
        NFA& operator*=(NFA& other);                            //Concatenation
        NFA& operator* (unsigned int n);                        //Kleene operator
        uint8_t operator*();
        NFA& operator>>(char c);
        NFA& operator<<(char c);
        void print();
    };
    PseudoNFA operator+(PseudoNFA& input,int32_t rotate);
    std::ostream& operator<<(std::ostream& out, PseudoNFA const& pnfa);
    template<class StateSet>
    NFA<StateSet> operator+(NFA<StateSet>& input,int32_t rotate);
    std::ostream& operator<<(std::ostream& out, PseudoNFA const& nfa);
    template<class StateSet>
    std::ostream& operator<<(std::ostream& out, NFA<StateSet>& nfa);
    class Match{
    public:
        char* start;
        char* end;
        std::string str(){return std::string(start,end-start);}
    };
    class RRegex{
        uint32_t states_n=0;
        template<typename NFA_t>
        NFA_t bracket_expression(const char* start, const char** cp2,uint32_t new_initial, int ps,const char *p,void* tr_table);
        template<typename NFA_t>
        NFA_t build_NFA(const char* p,void* tr_table);
        template<unsigned int k>
        void alloc_BitSetNFA(const char* p);
        size_t memory_pool_size;
        void* memory_pool;
    public:
        std::unique_ptr<Executable> exec;
        RRegex(const char* p);
        class iterator{
            char* cur_s     = nullptr;
            char* begin_s   = nullptr;
            size_t cur;
            std::vector<Match> matches;
            RRegex* r=nullptr;
        public:
            iterator(){
                cur=0;
                Match m;
                m.end=nullptr;
                m.start=nullptr;
                matches.push_back(m);
            }
            iterator(char* s_begin, RRegex& r_arg): begin_s(s_begin),r(&r_arg){
                cur=0;
                Match m;
                m.end=begin_s;
                m.start=nullptr;
                matches.push_back(m);
                *(r->exec)<<'\0';
                (*this)++;
                //cur--;
            };
            bool operator!=(RRegex::iterator i2){
                return matches[cur].end!=i2.matches[i2.cur].end;
            };
            char* operator<<(char* s);
            char* operator>>(char* s);
            void operator++(){
                //std::cout<<"hello!!!"<<std::endl;
                if(++cur>=matches.size()){
                    Match m;
                    cur_s=matches[matches.size()-1].end;
                    m.end=(*this)<<matches[matches.size()-1].end;
                    //std::cout<<"m.end"<<m.end-matches[0].end;
                    m.start=nullptr;
                    matches.push_back(m);
                }
            };
            void operator++(int){++(*this);};
            std::string operator*(){
                if(!matches[cur].start){
                    r->exec->reset();
                    cur_s=matches[cur].end-1;
                    matches[cur].start=(*this)>>matches[cur].end;
                }
                //std::cout<<"start: "<<matches[cur].start<<std::endl;
                return matches[cur].str();
            };
        };
        
    };
}
