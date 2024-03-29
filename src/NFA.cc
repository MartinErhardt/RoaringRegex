/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#include"BitSet.h"
#include"regex.h"
using namespace Regex;

namespace Regex{
#define idx(state, c, fwd) ([&]{                                     \
          if constexpr (fwd) return ((uint8_t) state)+2*memory_pool.states_n*((size_t)c);    \
          else return               ((uint8_t)state)+memory_pool.states_n*(1+2*((size_t)c));\
        }())

template<class StateSet>
void NFA<StateSet>::print(){
    std::cout<<"initial state: "<<initial_state<<std::endl;
    std::cout<<"buffer_size: "<<memory_pool.states_n<<std::endl;
    std::cout<<"size: "<<size<<std::endl;
    std::cout<<"final states: ";
    final_states.printf();
    std::cout<<std::endl;
    for(uint32_t i=initial_state;i<initial_state+size;i++){
        std::cout<<"state: "<<i<<std::endl;
        std::cout<<"forward transitions: "<<std::endl;
        for(unsigned char c=0; c<0x80;c++){
            const StateSet& r=memory_pool.states[i+2*memory_pool.states_n*((int)c)];
            if(!r.cardinality()) continue;
            std::cout<<c<<": ";
            r.printf();
            std::cout<<std::endl;
        }
        std::cout<<"backward transitions: "<<std::endl;
        for(unsigned char c=0; c<0x80;c++){
            const StateSet& r=memory_pool.states[i+memory_pool.states_n*(1+2*((int)c))];
            if(!r.cardinality()) continue;
            std::cout<<c<<": ";
            r.printf();
            std::cout<<std::endl;
        }
    }
}
template<class StateSet>
NFA<StateSet>::NFA(uint32_t cur_n, MemoryPool<StateSet>& mem_pool):PseudoNFA(cur_n), memory_pool(mem_pool){
    if constexpr(std::is_same<StateSet, Roaring>::value) final_states=std::move(Roaring::bitmapOf(1,cur_n));
    else{
        final_states=StateSet();
        final_states.add(cur_n);
    }
}
template<class StateSet>
NFA<StateSet>::NFA(uint32_t cur_n,char c, MemoryPool<StateSet>& mem_pool):PseudoNFA(cur_n,c), memory_pool(mem_pool){
    mem_pool.states[idx(cur_n,(int)c,true)].add(cur_n+1);
    mem_pool.states[idx(cur_n+1,(int)c,false)].add(cur_n);
    if constexpr(std::is_same<StateSet, Roaring>::value) final_states=std::move(Roaring::bitmapOf(1,cur_n+1));
    else {
        final_states=StateSet();
        final_states.add(cur_n+1);
    }
}
template<class StateSet>
NFA<StateSet>::NFA(uint32_t cur_n,BitSet<2>& cs,MemoryPool<StateSet>& mem_pool):PseudoNFA(cur_n,'a'), memory_pool(mem_pool){
    for(typename BitSet<2>::const_iterator i = cs.begin(); i < cs.end();++i){
        mem_pool.states[idx(cur_n,*i,true)].add(cur_n+1);
        mem_pool.states[idx(cur_n+1,*i,false)].add(cur_n);
    }
    if constexpr(std::is_same<StateSet, Roaring>::value) final_states=std::move(Roaring::bitmapOf(1,cur_n+1));
    else{
        final_states=StateSet();
        final_states.add(cur_n+1);
    }
}
template<class StateSet>
template<bool fwd>
NFA<StateSet>& NFA<StateSet>::Processor::shift(char c){
    MemoryPool<StateSet> memory_pool=to_exec.memory_pool;
    StateSet* state_table=to_exec.memory_pool.states;
    if constexpr(std::is_same<StateSet, Roaring>::value){
        size_t current_states_cardinality=current_states.cardinality();
        Roaring** select_for_fastunion=to_exec.memory_pool.select_for_fastunion;
        uint32_t* gather_for_fastunion=to_exec.memory_pool.gather_for_fastunion;
        current_states.toUint32Array(memory_pool.gather_for_fastunion);
        for(uint32_t i=0;i<current_states_cardinality;i++)
            select_for_fastunion[i]=&state_table[idx(gather_for_fastunion[i],c,fwd)];
        current_states = Roaring::fastunion(current_states_cardinality,
                                            const_cast<const Roaring**>(select_for_fastunion));
    }else{
        StateSet new_current_states=StateSet();
        for(typename StateSet::const_iterator i = current_states.begin(); 
            i < current_states.end();++i)
            /*{
            std::cout<< "current_states[fwd]: ";
            std::cout<< "unite from i: "<< *i << ": " << std::endl;
            for(typename StateSet::const_iterator k = memory_pool.states[idx((uint8_t)*i,c,fwd)].begin(); 
                k < memory_pool.states[idx((uint8_t)*i,c,fwd)].end();++k)
                    std::cout<< *k << " ";
            std::cout<< std::endl;*/
            new_current_states |= state_table[idx((uint8_t)*i,c,fwd)];
        //}
        current_states = new_current_states;
    }
    return to_exec;
}
template<class StateSet>
uint8_t NFA<StateSet>::Processor::operator*(){
    return (current_states.contains(to_exec.initial_state)<<1)
          |(current_states.and_cardinality(to_exec.final_states)>0);
};
template<class StateSet>
template<bool fwd>
void NFA<StateSet>::skip(uint32_t n,uint32_t k){
    uint32_t to_skip=fwd?k:n;
    uint32_t fixed=fwd?n:k;
    for(unsigned char c=0;c<0x80;c++){
        memory_pool.states[idx(fixed,c,fwd)] |= memory_pool.states[idx(to_skip,c,fwd)];
        for(typename StateSet::const_iterator j = memory_pool.states[idx(to_skip,c,fwd)].begin();
                                    j!=memory_pool.states[idx(to_skip,c,fwd)].end();++j){
            //std::cout<< "j: "<< *j <<std::endl;
            memory_pool.states[idx(*j,c,!fwd)].add(fixed);
        }
    }
}
template<class StateSet>
NFA<StateSet>& NFA<StateSet>::operator*=(NFA& other){
    //std::cout<<"################################# merge NFA: "<<std::endl;
    //std::cout<<other;
    //std::cout<<"################################# merge into NFA: "<<std::endl;
    //std::cout<<*this;
    this->PseudoNFA::operator*=(other);
    for(typename StateSet::const_iterator i = final_states.begin(); i != final_states.end(); ++i)
        skip<false>(*i,other.initial_state);
    if(final_states.contains(initial_state)) skip<true>(initial_state,other.initial_state);
    if(final_states.contains(initial_state)&&other.final_states.contains(other.initial_state)){
        final_states=other.final_states;
        final_states.add(initial_state);
    } else final_states=other.final_states;
    return *this;
}
template<class StateSet>
NFA<StateSet>& NFA<StateSet>::operator|=(NFA& other){
    //std::cout<<"################################# union NFA: "<<std::endl;
    //std::cout<<other;
    //std::cout<<"################################# union into NFA: "<<std::endl;
    //std::cout<<*this;
    this->PseudoNFA::operator|=(other);
    final_states|=other.final_states;
    skip<true>(initial_state,other.initial_state);
    if(other.final_states.contains(other.initial_state))  final_states.add(initial_state);
    return *this;
}
template<class StateSet>
NFA<StateSet>& NFA<StateSet>::operator*(unsigned int n){
    if(!n) return *this;
    for(typename StateSet::const_iterator i = final_states.begin(); i != final_states.end(); ++i) 
        skip<false>(*i,initial_state);
    final_states.add(initial_state);
    return *this;
}
template class MemoryPool<BitSet<1>>;
template class MemoryPool<BitSet<2>>;
template class MemoryPool<BitSet<4>>;
template class MemoryPool<Roaring>;
template class MemoryPool<NoStateSet>;

template class NFA<BitSet<1>>;
template class NFA<BitSet<2>>;
template class NFA<BitSet<4>>;
template class NFA<Roaring>;
template class NFA<NoStateSet>;
/**
 * TODO don't copy NFAs
 */
Roaring operator+(Roaring& set,int rotate){
    Roaring ret;//TODO use fastunion
    for(Roaring::const_iterator i=set.begin(); i!=set.end(); i++) ret.add(*i+rotate);
    return ret;
}
template<class StateSet>
NFA<StateSet> operator+(NFA<StateSet>& input,int rotate){
    NFA<StateSet> ret(input.initial_state+rotate,input.memory_pool);
    ret.size=input.size;
    for(uint32_t i=input.initial_state;i<input.initial_state+input.size; i++)
        for(int16_t c=0;c<0x100;c++) ret.memory_pool.states[i+rotate+input.memory_pool.states_n*(2*c-(c>=0x80)*0xFF)]=std::move(input.memory_pool.states[i+input.memory_pool.states_n*(2*c-(c>=0x80)*0xFF)]+rotate);
    ret.final_states=std::move(input.final_states+rotate);
    return ret;
}
template<>
NFA<NoStateSet> operator+<NoStateSet>(NFA<NoStateSet>& input, int rotate){
    return NFA<NoStateSet>(input);
}
template NFA<Roaring> operator+(NFA<Roaring>& input,int rotate);
template NFA<BitSet<1>> operator+(NFA<BitSet<1>>& input,int rotate);
template NFA<BitSet<2>> operator+(NFA<BitSet<2>>& input,int rotate);
template NFA<BitSet<4>> operator+(NFA<BitSet<4>>& input,int rotate);

std::ostream& operator<<(std::ostream& out, PseudoNFA const& pnfa){
    out<<"size: "<<pnfa.size<<std::endl;
    return out;
}
template<class StateSet>
std::ostream& operator<<(std::ostream& out, NFA<StateSet>& nfa){nfa.print();return out;}
template std::ostream& operator<<(std::ostream& out, NFA<Roaring>& nfa);
template std::ostream& operator<<(std::ostream& out, NFA<BitSet<1>>& nfa);
template std::ostream& operator<<(std::ostream& out, NFA<BitSet<2>>& nfa);
template std::ostream& operator<<(std::ostream& out, NFA<BitSet<4>>& nfa);

} //Regex
