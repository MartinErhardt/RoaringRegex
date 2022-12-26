/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#pragma once
#include<vector>
#include<memory>

#include "../../CRoaring/roaring.hh"
using namespace roaring;
#include<iostream>
#include"BitSet.h"
#include<stack>
#include<optional>
namespace Regex{
    enum operation{CONCATENATION,BRACKETS,OR};
    template<class StateSet>
    class MemoryPoolBase{
    public:
        std::shared_ptr<void> memory_pool;
        StateSet* states;
        uint32_t states_n;
        size_t memory_pool_size;
        MemoryPoolBase(unsigned int states_n_arg){
            states_n = states_n_arg;
            if(states_n){
                memory_pool_size=get_size(states_n_arg);
                memory_pool=std::shared_ptr<void>(malloc(memory_pool_size), [this](void*p){deleter(p);});
                char* memory_pool_raw = static_cast<char*>(memory_pool.get());
                memset(static_cast<char*>(memory_pool_raw),0,memory_pool_size);
                states=reinterpret_cast<StateSet*>(memory_pool_raw);
            }
        }
        virtual size_t get_size(unsigned int states_n){
            return sizeof(StateSet)*0x100*states_n;
        }
        virtual void deleter(void *p){
            free(p);
        }
    };
    template<class StateSet>
    class MemoryPool: public MemoryPoolBase<StateSet>{
    public:
        MemoryPool(unsigned int states_n) : MemoryPoolBase<StateSet>(states_n){};
        MemoryPool(const MemoryPool<StateSet>& other) = default;
        MemoryPool(MemoryPool<StateSet>&& other) = default;
        MemoryPool<StateSet>& operator=(MemoryPool<StateSet>& other) = default;
        MemoryPool<StateSet>& operator=(MemoryPool<StateSet>&& other) = default;
    };
    template <>
    class MemoryPool<Roaring> : public MemoryPoolBase<Roaring>{
    public:
        uint32_t*  gather_for_fastunion;               // owned by memory_pool
        Roaring**  select_for_fastunion;               // owned by memory_pool
        MemoryPool(unsigned int states_n_arg): MemoryPoolBase(states_n_arg){
            char* memory_pool_raw = static_cast<char*>(memory_pool.get());
            states=reinterpret_cast<Roaring*>(memory_pool_raw);
            gather_for_fastunion=reinterpret_cast<uint32_t*>(memory_pool_raw+sizeof(Roaring)*states_n*0x100);
            select_for_fastunion=reinterpret_cast<Roaring**>(memory_pool_raw+sizeof(Roaring)*states_n*0x100+sizeof(Roaring*)*states_n);
        }
        size_t get_size(unsigned int ){
            return sizeof(Roaring)*0x100*states_n
                  +sizeof(Roaring*)*states_n
                  +sizeof(uint32_t*)*states_n;
        }
        void deleter(void* p){
            for(Roaring* cur_state=states;
                cur_state<states+states_n*0x100;cur_state++) 
                cur_state->~Roaring();
            free(p);
        }
        MemoryPool(const MemoryPool<Roaring>& other) = default;
        MemoryPool(MemoryPool<Roaring>&& other) = default;
        MemoryPool<Roaring>& operator=(MemoryPool<Roaring>& other) = default;
        MemoryPool<Roaring>& operator=(MemoryPool<Roaring>&& other) = default;
    };
    
    class NoStateSet{};
    class PseudoNFA{
    public:
        uint32_t size;
        uint32_t initial_state;
        PseudoNFA(){};
        PseudoNFA(const PseudoNFA& to_copy){size=to_copy.size;initial_state=to_copy.initial_state;}
        PseudoNFA(PseudoNFA&& to_move){size=to_move.size; initial_state=to_move.initial_state; to_move.size=0;}
        //PseudoNFA(uint32_t cur_n,MemoryPool<PseudoSet> mem_pool):size(mem_pool.states_n),initial_state(cur_n){};
        PseudoNFA(uint32_t cur_n){size=1;initial_state=cur_n;};         //create empty NFA
        PseudoNFA(uint32_t cur_n,char c){size=2;initial_state=cur_n;};  //create NFA accepting just c
        PseudoNFA(uint32_t cur_n,BitSet<2>& cs){size=2;initial_state=cur_n;};
        PseudoNFA& operator=(PseudoNFA& other){size=other.size;initial_state=other.initial_state; return *this;}
        PseudoNFA& operator=(PseudoNFA&& other){size=other.size;other.size=0; initial_state=other.initial_state; return *this;}
        PseudoNFA& operator|=(PseudoNFA&& other){return *this|=other;};             //Union
        PseudoNFA& operator*=(PseudoNFA&& other){return *this*=other;};             //Concatenation
        PseudoNFA& operator|=(PseudoNFA& other){size+=other.size;return *this;};    //Union
        PseudoNFA& operator*=(PseudoNFA& other){size+=other.size;return *this;};    //Concatenation
        PseudoNFA& operator*(unsigned int n){return *this;};                        //Kleene operator
    };
    template<class StateSet>
    class NFA;
    std::ostream& operator<<(std::ostream& out, NFA<Roaring> const& nfa);
    class Match{
    public:
        char* start;
        char* end;
        std::string str(){return std::string(start,end-start);}
    };
    class RegexIterator{
        public: 
        virtual RegexIterator& operator++(int) = 0;//process forward
        virtual RegexIterator* create_copy() = 0;
        virtual std::optional<Match> operator*() = 0;
        virtual void print() = 0;
    };
    class IteratorWrapper : public RegexIterator{
        std::unique_ptr<RegexIterator> concrete;
    public: 
        IteratorWrapper(RegexIterator* ptr): concrete(std::unique_ptr<RegexIterator>(ptr)){};
        IteratorWrapper(IteratorWrapper& to_copy): concrete(std::unique_ptr<RegexIterator>(to_copy.create_copy())){};
        IteratorWrapper& operator++(int){(*concrete)++; return *this;};
        std::optional<Match> operator*(){return **concrete;};
        void print(){concrete->print();}
        RegexIterator* create_copy(){return concrete->create_copy();};
    };
    class IterFactoryBase{
        public: //TODO make private
            virtual IteratorWrapper get_acceptance_iter(char* c) = 0;
    };
    template<class StateSet>
    class NFA: public PseudoNFA {
        class Processor{
            #define FLAG_ACCEPTING   (1<<0)
            #define FLAG_INITIAL     (1<<1)
            StateSet    current_states;
            NFA<StateSet>& to_exec;
        public:
            Processor(NFA<StateSet>& nfa): to_exec(nfa){
                if constexpr(std::is_same<StateSet, Roaring>::value){
                    current_states=std::move(Roaring::bitmapOf(1,to_exec.initial_state));
                }else{
                    StateSet s;
                    s.add(to_exec.initial_state);
                    current_states=s;
                }
            };
            template<bool fwd> NFA& shift(char c);
            NFA<StateSet>& operator>>(char c){return shift<true>(c);};
            NFA<StateSet>& operator<<(char c){return shift<true>(c);};
            uint8_t operator*();
            void print(){to_exec.print();}
        };
        class AcceptanceIterator: public RegexIterator{
            Processor proc;
            char* current_char;
            char* initial;
        public:
            AcceptanceIterator(char* to_process, NFA& nfa) : proc(nfa), current_char(to_process), initial(to_process) {};
            RegexIterator& operator++(int){
                while(*current_char) proc<<*(current_char++);
                return *this;
            };
            std::optional<Match> operator*(){
                return *proc&FLAG_ACCEPTING ? std::optional(Match{initial,current_char}) : std::nullopt;
            };
            RegexIterator* create_copy(){return new AcceptanceIterator(*this);}
            void print() {proc.print();}
        };
        template<bool fwd> void skip(uint32_t n,uint32_t k);
    public:
        class IterFactory: public IterFactoryBase{
            NFA<StateSet> compiled_regex;
        public: //TODO make private
            IterFactory(NFA&& get_nfa):compiled_regex(get_nfa){};
            IteratorWrapper get_acceptance_iter(char* c){
                return IteratorWrapper(new AcceptanceIterator(c,compiled_regex));
            };
        };
        StateSet*   states;                                      //owned first by Parser then Executable
        StateSet    final_states;
        MemoryPool<StateSet> memory_pool;
        
        NFA(const NFA<StateSet>& other) = default;
        NFA(NFA<StateSet>&& other) = default;
        
        NFA(uint32_t cur_n,MemoryPool<StateSet>& mem_pool);              //create empty NFA
        NFA(uint32_t cur_n,char c,MemoryPool<StateSet>& mem_pool);       //create NFA accepting just c
        NFA(uint32_t cur_n,BitSet<2>& cs,MemoryPool<StateSet>& mem_pool);//create NFA accepting all characters in cs
        NFA<StateSet>& operator=(NFA<StateSet>& other) = default;
        NFA<StateSet>& operator=(NFA<StateSet>&& other) = default;
        NFA& operator|=(NFA&& other){return *this|=other;};     //Union
        NFA& operator*=(NFA&& other){return *this*=other;};     //Concatenation
        NFA& operator|=(NFA& other);                            //Union
        NFA& operator*=(NFA& other);                            //Concatenation
        NFA& operator* (unsigned int n);                        //Kleene operator
        void print();
    };
    //NFA<NoStateSet> operator+(NFA<NoStateSet>& input,int rotate);
    template<>
    class NFA<NoStateSet>: public PseudoNFA{
    public:
        NFA(uint32_t cur_n,MemoryPool<NoStateSet>& mem_pool)
        :PseudoNFA(cur_n){};              //create empty NFA
        NFA(uint32_t cur_n,char c,MemoryPool<NoStateSet>& mem_pool)
        :PseudoNFA(cur_n,c){};       //create NFA accepting just c
        NFA(uint32_t cur_n,BitSet<2>& cs,MemoryPool<NoStateSet>& mem_pool)
        :PseudoNFA(cur_n,cs){};
    };
    std::ostream& operator<<(std::ostream& out, PseudoNFA const& pnfa);
    template<class StateSet>
    NFA<StateSet> operator+(NFA<StateSet>& input,int32_t rotate);
    std::ostream& operator<<(std::ostream& out, PseudoNFA const& nfa);
    template<class StateSet>
    std::ostream& operator<<(std::ostream& out, NFA<StateSet>& nfa);
    class RRegex{
        uint32_t states_n=0;
        template<class StateSet>
        NFA<StateSet> bracket_expression(const char* start,const char** cp2,uint32_t new_initial,int ps,const char *p,MemoryPool<StateSet>& mem_pool);
        template<class StateSet>
        NFA<StateSet> build_NFA(const char* p,unsigned int tr_table);
        //template<class StateSet>
        //NFA<StateSet> build_IterFactory(const char* p,unsigned int tr_table){
        //    return typename NFA<StateSet>::IterFactory<StateSet>(std::move(build_NFA<StateSet>(p,tr_table)));
        //};
    public:
        std::unique_ptr<IterFactoryBase> iter_factory;
        RRegex(const char* p);
        IteratorWrapper get_acceptance_iter(char* c){
            return iter_factory->get_acceptance_iter(c);
        }
    };
}
