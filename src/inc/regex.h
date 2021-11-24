/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#pragma once
#include<vector>
#include<memory>
#include "../../CRoaring/roaring.hh"
#include "../../CRoaring/roaring.c"
#include<iostream>
namespace Regex{
    typedef struct State{
        Roaring tr[512];         //0-0xFF         forward transition
                                    //0x100-0x1FF    backward transition(relevant for FRegex iterator dereference) Also relevant for "quick" NFA collocation
        //State (){
        //}
        State& operator=(const State& s)=delete;
        State& operator=(State&& s)=delete;
        State (const State& s)=delete;
        State (State&& s)=delete;
        ~State(){
            for(int c=0;c<512;c++){
                //std::cout<<"release"<<std::endl;
                tr[c].~Roaring();
            }
        };
    }state;
    
    enum operation{CONCATENATION,BRACKETS,OR};
    class PseudoNFA{
    public:
        uint32_t size;
        uint32_t initial_state;
        PseudoNFA(){};
        PseudoNFA(const PseudoNFA& to_copy){size=to_copy.size;initial_state=to_copy.initial_state;}
        PseudoNFA(PseudoNFA&& to_move){size=to_move.size; initial_state=to_move.initial_state; to_move.size=0;}
        PseudoNFA(uint32_t cur_n,uint32_t states_n):size(states_n),initial_state(cur_n){};
        PseudoNFA(uint32_t cur_n,uint32_t states_n,state* states){size=1;initial_state=cur_n;};         //create empty NFA
        PseudoNFA(uint32_t cur_n,char c,uint32_t states_n,state* states){size=2;initial_state=cur_n;};  //create NFA accepting just c
        PseudoNFA& operator=(PseudoNFA& other){size=other.size;initial_state=other.initial_state; return *this;}
        PseudoNFA& operator=(PseudoNFA&& other){size=other.size;other.size=0; initial_state=other.initial_state; return *this;}
        PseudoNFA& operator|=(PseudoNFA&& other){return *this|=other;};     //Union
        PseudoNFA& operator*=(PseudoNFA&& other){
            //std::cout<<"CALL  BASE1"<<std::endl;
            return *this*=other;};    //Concatenation
        PseudoNFA& operator|=(PseudoNFA& other){size+=other.size;return *this;};    //Union
        PseudoNFA& operator*=(PseudoNFA& other){
            std::cout<<"CALL  BASE2this size "<<size<<"\tother size: "<<other.size<<std::endl;size+=other.size;return *this;};        //Concatenation
        PseudoNFA& operator* (unsigned int n){return *this;};                     //Kleene operator
    };
    class NFA: public PseudoNFA{
        template<bool fwd> void skip(uint32_t n,uint32_t k);
    public:
        state* states;                                 //owned first by Parser then Executable
        Roaring    current_states;
        Roaring    final_states;
        NFA(){};
        NFA(const NFA& to_copy);
        NFA(NFA&& to_move);
        NFA(uint32_t cur_n,uint32_t states_n,state* states);         //create empty NFA
        NFA(uint32_t cur_n,char c,uint32_t states_n,state* states);  //create NFA accepting just c
        NFA& operator=(NFA& other);
        NFA& operator=(NFA&& other);
        NFA& operator|=(NFA&& other){return *this|=other;};     //Union
        NFA& operator*=(NFA&& other){return *this*=other;};     //Concatenation
        NFA& operator|=(NFA& other);                            //Union
        NFA& operator*=(NFA& other);                            //Concatenation
        NFA& operator* (unsigned int n);    //Kleene operator
    };
    /*
    template<int states_n>
    class mempool_ptr:public std::shared_ptr<void>{
        int states_n;
        mempool_ptr(void* ptr,)
        ~mempool_ptr(){
            for(state* states,cur_state=states=reinterpret_cast<state*>(memory_pool.get());cur_state<states+states_n;states++) states->~State();
            this->~std::shared_ptr<void>();
        }
    }*/
    class Executable{
        void* memory_pool;
        size_t memory_pool_size;
        bool moved_from=false;
    public: //only for debug purposes
        size_t states_n;
        NFA nfa;
    private:
        uint32_t*  gather_for_fastunion=nullptr;                // owned by memory_pool
        Roaring**   select_for_fastunion=nullptr;                // owned by memory_pool
        template<bool fwd> Executable& shift(char c);
        public: //TODO make private
            Executable():nfa(){};
            Executable(void* memory_pool, size_t memory_pool_size,size_t states_n, NFA&& nfa,uint32_t* gather_for_fastunion,Roaring** select_for_fastunion):
            memory_pool(memory_pool),memory_pool_size(memory_pool_size),states_n(states_n),nfa(nfa),
            gather_for_fastunion(gather_for_fastunion),
            select_for_fastunion(select_for_fastunion){};
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
                nfa=std::move(other.nfa);
                memory_pool=other.memory_pool;
                memory_pool_size=other.memory_pool_size;
                states_n=other.states_n;
                gather_for_fastunion=other.gather_for_fastunion;
                select_for_fastunion=other.select_for_fastunion;
                other.memory_pool=nullptr;
                other.gather_for_fastunion=nullptr;
                other.select_for_fastunion=nullptr;
                other.moved_from=true;
                //std::cout<<"move"<<std::endl;
                return *this;
            };/*
            Executable& operator=(Executable other){
                return (*this)=std::move(other);
            };*/
            Executable(Executable& exe) //{(*this)=exe;}
            =delete;
            Executable(Executable&& exe){(*this)=std::move(exe);};
            #define FLAG_ACCEPTING   (1<<1)
            #define FLAG_INITIAL     (1<<2)
            uint8_t operator*();
            Executable& operator<<(char c);            //process forward
            Executable& operator>>(char c);            //process in reverse order
            ~Executable(){
                //std::cout<<"moved_from: "<<moved_from<<"states_n: "<<states_n<<std::endl;
                if(!moved_from){
                    state* states=(state*)memory_pool;
                    state* cur_state=states;
                    for(;cur_state<states+states_n;cur_state++){
                    //    std::cout<<"state: "<<cur_state<<"sates+states_n"<<states+states_n<<std::endl;
                        cur_state->~State();
                    }
                    free(memory_pool);
                }
            }
    };
    class Lexer{
        uint32_t states_n=0;
        state* states;
        template<class NFA_t>
        NFA_t bracket_expression(const char* start, int* i,uint32_t new_initial,int ps,const char *p);
        template<class NFA_t>
        NFA_t build_NFA(const char* p);
    public:
        Executable exec;
        Lexer(const char* p){
            states_n=build_NFA<PseudoNFA>(p).size;
            size_t memory_pool_size=sizeof(state)*states_n+sizeof(Roaring*)*states_n+sizeof(uint32_t*)*states_n;
            //std::cout<<"size of automaton: "<<states_n<<std::endl;
            //std::shared_ptr<void> memory_pool=std::shared_ptr<void>(malloc(memory_pool_size),free);
            void* memory_pool=malloc(memory_pool_size);
            memset(static_cast<char*>(memory_pool),0,memory_pool_size); // conforms to strict-aliasing; initializes all Roaring bitmaps
            
            //std::cout<<"memory pool: "<<memory_pool<<"\tsize"<<memory_pool_size<<std::endl;
            states=reinterpret_cast<state*>(memory_pool);
            NFA nfa=build_NFA<NFA>(p);
            uint32_t* gather_for_fastunion=reinterpret_cast<uint32_t*>(static_cast<char*>(memory_pool)+sizeof(state)*states_n);
            Roaring** select_for_fastunion=reinterpret_cast<Roaring**>(static_cast<char*>(memory_pool)+sizeof(state)*states_n+sizeof(Roaring*)*states_n);
            exec=std::move(Executable(memory_pool,memory_pool_size,states_n,std::move(nfa),gather_for_fastunion,select_for_fastunion));
            //std::cout<<"exec states_n: "<<exec.states_n<<std::endl;
        }
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
