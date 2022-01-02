/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#pragma once
#include<vector>
#include<memory>
#include "../../CRoaring/roaring.hh"
#include<iostream>
#include"BitSet.h"
#include <stack>
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
        PseudoNFA& operator*(unsigned int n){return *this;};                       //Kleene operator
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
        virtual void reset_all()               = 0;
        virtual Executable* create_copy()      = 0;
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
                    gather_for_fastunion=reinterpret_cast<uint32_t*>(static_cast<char*>(memory_pool_arg)+sizeof(Roaring)*states_n*0x100);
                    select_for_fastunion=reinterpret_cast<Roaring**>(static_cast<char*>(memory_pool_arg)+sizeof(Roaring)*states_n*0x100+sizeof(Roaring*)*states_n);
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
        StateSet*   states;                                      //owned first by Parser then Executable
        StateSet    current_states[2];
        StateSet    final_states;
        NFA(){};
        NFA(const NFA& to_copy);
        NFA(NFA&& to_move);
        NFA(uint32_t cur_n,uint32_t states_n,void* states);             //create empty NFA
        NFA(uint32_t cur_n,char c,uint32_t states_n,void* states);      //create NFA accepting just c
        NFA(uint32_t cur_n,BitSet<2>& cs,uint32_t states_n,void* states);//create NFA accepting all characters in cs
        void reset(){
            current_states[0]=final_states;
        };
        void reset_all(){
            if constexpr(std::is_same<StateSet, Roaring>::value){
                current_states[1]=std::move(Roaring::bitmapOf(1,initial_state));
            }else{
                StateSet s;
                s.add(initial_state);
                current_states[1]=s;
            }
            current_states[0]=final_states;
        };
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
        NFA* create_copy(){return new NFA(*this);}
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
        class AbstractIterator{
        protected:
            char* cur_s     = nullptr;
            char* limit     = nullptr;
            char* begin_s   = nullptr;
            size_t cur;
            std::vector<Match> matches;
            Executable* exe=nullptr;
            char* operator<<(char* s);
            char* operator>>(char* s);
        public:
            AbstractIterator(){}
            //virtual void operator++()               =0;
            //virtual void operator++(int)            =0;
            //virtual std::string operator*();
            bool operator!=(RRegex::AbstractIterator& i2){
                return matches[cur].end!=i2.matches[i2.cur].end;
            };
            //~AbstractIterator(){}
        };
        class LazyIterator:public AbstractIterator{
            std::unique_ptr<Executable> exe_owning=nullptr;
        public:
            LazyIterator(){
                cur=0;
                Match m;
                m.end=nullptr;
                m.start=nullptr;
                limit=nullptr;
                matches.push_back(m);
            }
            LazyIterator(char* s_begin, RRegex& r_arg): AbstractIterator(), exe_owning(r_arg.exec->create_copy()) // TODO create copy of NFA
            {
                begin_s=s_begin;
                cur=0;
                Match m;
                limit=begin_s;
                m.end=begin_s;
                m.start=nullptr;
                matches.push_back(m);
                exe=&*exe_owning;
                *exe<<'\0';
                (*this)++;
            };
            void operator++();
            void operator++(int){++(*this);};
            std::string operator*();
        };
        class GreedyIterator:public AbstractIterator{
        public:
            GreedyIterator(){
                cur=0;
                Match m;
                m.end=nullptr;
                m.start=nullptr;
                limit=nullptr;
                matches.push_back(m);
            }
            GreedyIterator(char* s_begin, RRegex& r_arg): AbstractIterator()  // TODO create copy of NFA
            {
                begin_s=s_begin;
                Match m;
                m.end=nullptr;
                m.start=nullptr;
                matches.push_back(m);
                limit=begin_s;
                exe=&*r_arg.exec;
                *exe<<'\0';
                cur_s=begin_s;
                std::stack<char*> stack;
                while((cur_s=*this<<cur_s)!=nullptr){
                    stack.push(cur_s);
                }
                while(!stack.empty()){
                    Match m;
                    cur_s=m.end=stack.top();
                    exe->reset();
                    limit=begin_s;
                    cur_s=m.start=((*this)>>cur_s);
                    while(!stack.empty()&&cur_s<stack.top()) stack.pop();
                    limit=stack.empty()?begin_s:stack.top();
                    while(cur_s>limit){
                        cur_s=*this>>cur_s;
                        if(**exe&FLAG_INITIAL) m.start=cur_s;
                    }
                    matches.push_back(m);
                }
                cur=matches.size()-1;
                exe->reset_all();
            };
            void operator++(){cur--;};
            void operator++(int){++(*this);};
            std::string operator*(){return matches[cur].str();}
        };
    };
}
