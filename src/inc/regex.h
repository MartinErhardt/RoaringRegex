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
        PseudoNFA& operator=(PseudoNFA& other){size=other.size;initial_state=other.initial_state; return *this;}
        PseudoNFA& operator=(PseudoNFA&& other){size=other.size;other.size=0; initial_state=other.initial_state; return *this;}
        PseudoNFA& operator|=(PseudoNFA&& other){return *this|=other;};     //Union
        PseudoNFA& operator*=(PseudoNFA&& other){
            //std::cout<<"CALL  BASE1"<<std::endl;
            return *this*=other;};    //Concatenation
        PseudoNFA& operator|=(PseudoNFA& other){size+=other.size;return *this;};    //Union
        PseudoNFA& operator*=(PseudoNFA& other){size+=other.size;return *this;};        //Concatenation
        PseudoNFA& operator* (unsigned int n){return *this;};                     //Kleene operator
    };
    class Executable{
        void* memory_pool=nullptr;
        size_t memory_pool_size=0;
        bool moved_from=false;
        char* cur_s_fwd=nullptr;
        char* cur_s_bwd=nullptr;
        char* begin_s=nullptr;
        #define FLAG_ACCEPTING   (1<<1)
        #define FLAG_INITIAL     (1<<2)
        virtual uint8_t operator*()            = 0;
        virtual Executable& operator<<(char c) = 0;            //process forward
        virtual Executable& operator>>(char c) = 0;            //process in reverse order
        virtual void reset()                   = 0;
    public: //only for debug purposes
        size_t states_n=0;
    protected:
        uint32_t*  gather_for_fastunion=nullptr;                // owned by memory_pool
        Roaring**   select_for_fastunion=nullptr;                // owned by memory_pool
    public: //TODO make private
            Executable(){};
            virtual void print() = 0;
            void init(void* memory_pool_arg, size_t memory_pool_size_arg, size_t states_n_arg){
                if(states_n>256){
                    gather_for_fastunion=reinterpret_cast<uint32_t*>(static_cast<char*>(memory_pool_arg)+sizeof(Roaring)*states_n);
                    select_for_fastunion=reinterpret_cast<Roaring**>(static_cast<char*>(memory_pool_arg)+sizeof(Roaring)*states_n+sizeof(Roaring*)*states_n);
                }
                memory_pool=memory_pool_arg;
                memory_pool_size=memory_pool_size_arg;
                states_n=states_n_arg;
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
                //nfa=std::move(other.nfa);
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
            //void printf(){print();};
            char* operator<<(char* s){
                if(!begin_s){
                    begin_s=s;
                    cur_s_fwd=begin_s;
                    *this<<'\0';
                }
                while(!(*(*this<<*(cur_s_fwd++))&FLAG_ACCEPTING)&&*cur_s_fwd);
                if(!(*cur_s_fwd)&&**this&FLAG_ACCEPTING){
                    char* intermediate=cur_s_fwd;
                    cur_s_fwd=cur_s_bwd=nullptr;
                    return intermediate-1; //minus null character
                } else if(!*cur_s_fwd){
                    cur_s_fwd=cur_s_bwd=nullptr;
                    return nullptr;
                }
                else{
                    cur_s_bwd=cur_s_fwd;
                    return cur_s_fwd;
                }
            };
            char* operator>>(char* s){
                if(cur_s_bwd==cur_s_fwd) reset();
                while(!(*(*this>>*(cur_s_bwd--))&FLAG_INITIAL)&&cur_s_bwd>begin_s);
                if(begin_s==cur_s_bwd&&**this&FLAG_INITIAL){
                    cur_s_bwd=cur_s_fwd;
                    return begin_s;
                }
                else if(begin_s==cur_s_bwd){
                    cur_s_bwd=cur_s_fwd;
                    *this>>'\0';
                    if(**this&FLAG_INITIAL) return begin_s;
                    else return nullptr;
                } else return cur_s_bwd;
            };
            virtual ~Executable(){
                //std::cout<<"moved_from: "<<moved_from<<"states_n: "<<states_n<<std::endl;
                if(!moved_from&&memory_pool){
                    //if(states_n>256){
                        Roaring* states=(Roaring*)memory_pool;
                        Roaring* cur_state=states;
                        for(;cur_state<states+states_n*0x100;cur_state++){
                        //    std::cout<<"state: "<<cur_state<<"sates+states_n"<<states+states_n<<std::endl;
                            cur_state->~Roaring();
                        }
                    //}
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
        StateSet* states;                                 //owned first by Parser then Executable
        Roaring    current_states[2];
        Roaring    final_states;
        NFA(){};
        NFA(const NFA& to_copy);
        NFA(NFA&& to_move);
        NFA(uint32_t cur_n,uint32_t states_n,StateSet* states);         //create empty NFA
        NFA(uint32_t cur_n,char c,uint32_t states_n,StateSet* states);  //create NFA accepting just c
        void reset(){current_states[1]=final_states;};
        NFA& operator=(NFA& other);
        NFA& operator=(NFA&& other);
        NFA& operator|=(NFA&& other){return *this|=other;};     //Union
        NFA& operator*=(NFA&& other){return *this*=other;};     //Concatenation
        NFA& operator|=(NFA& other);                            //Union
        NFA& operator*=(NFA& other);                            //Concatenation
        NFA& operator* (unsigned int n);    //Kleene operator
        uint8_t operator*();
        NFA& operator>>(char c);
        NFA& operator<<(char c);
        void print(){std::cout<<*this;}
    };
    std::ostream& operator<<(std::ostream& out, NFA<Roaring> const& nfa){
        out<<"initial state: "<<nfa.initial_state<<std::endl;
        out<<"final states: ";
        nfa.final_states.printf();
        out<<std::endl;
        for(uint32_t i=0;i<nfa.size;i++){
            out<<"state: "<<i+nfa.initial_state<<std::endl;
            out<<"forward transitions: "<<std::endl;
            for(unsigned char c=0; c<0x80;c++){
                const Roaring& r=nfa.states[0x100*i+(int)c];
                //std::cout<<(int) c<<std::endl;
                if(!r.cardinality()) continue;
                out<<c<<": ";
                r.printf();
                out<<std::endl;
            }
            out<<"backward transitions: "<<std::endl;
            for(unsigned char c=0; c<0x80;c++){
                const Roaring& r=nfa.states[0x100*i+0x80+((int)c)];
                //std::cout<<(int) c<<std::endl;
                if(!r.cardinality()) continue;
                out<<c<<": ";
                r.printf();
                out<<std::endl;
            }
        }
        return out;
    }
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
    class Lexer{
        uint32_t states_n=0;
        template<class NFA_t, class StateSet>
        NFA_t bracket_expression(const char* start, int* i,uint32_t new_initial, int ps,const char *p,StateSet* tr_table);
        template<class NFA_t,class StateSet>
        NFA_t build_NFA(const char* p,StateSet* tr_table);
    public:
        std::unique_ptr<Executable> exec;
        Lexer(const char* p){
            states_n=build_NFA<PseudoNFA,void>(p,nullptr).size;
            size_t memory_pool_size=sizeof(Roaring)*0x100*states_n+sizeof(Roaring*)*states_n+sizeof(uint32_t*)*states_n;
            //std::cout<<"size of automaton: "<<states_n<<std::endl;
            //std::shared_ptr<void> memory_pool=std::shared_ptr<void>(malloc(memory_pool_size),free);
            void* memory_pool=malloc(memory_pool_size);
            memset(static_cast<char*>(memory_pool),0,memory_pool_size); // conforms to strict-aliasing; initializes all Roaring bitmaps
            
            //std::cout<<"memory pool: "<<memory_pool<<"\tsize"<<memory_pool_size<<std::endl;
            Roaring* states=reinterpret_cast<Roaring*>(memory_pool);
            exec=std::make_unique<NFA<Roaring>>(build_NFA<NFA<Roaring>,Roaring>(p,states));
            exec->init(memory_pool, memory_pool_size, states_n);
        }
        //~Lexer(){
        //    exec.~unique_ptr<Executable>();
        //}
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
#include"../NFA.cc"
