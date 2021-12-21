/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#include"BitSet.cc"
using namespace roaring;
using namespace Regex;

#define idx(state, c, fwd) ([&]{                                   \
          if constexpr (fwd) return state+2*states_n*((size_t)c);       \
          else return               state+states_n*(1+2*((size_t)c));\
        }())
Roaring operator+(Roaring& set,int32_t rotate){
    Roaring ret;
    for(Roaring::const_iterator i = set.begin(); i != set.end(); i++) ret.add(*i+rotate);
    return ret;
}
//NFA intances are lightweight objects now => TODO remove move semantics
template<class StateSet>
NFA<StateSet>::NFA(NFA<StateSet>&& to_move): PseudoNFA(to_move),Executable(to_move.states_n),states(to_move.states),
                        current_states(std::move(to_move.current_states)){
    current_states[0]=std::move(to_move.current_states[0]);
    current_states[1]=std::move(to_move.current_states[1]);
    final_states=std::move(to_move.final_states);
}
template<class StateSet>
NFA<StateSet>::NFA(const NFA<StateSet>& to_copy):PseudoNFA(to_copy),Executable(to_copy.states_n), states(to_copy.states){
    current_states[0]=to_copy.current_states[0];
    current_states[1]=to_copy.current_states[1];
    final_states=to_copy.final_states;
}
template<class StateSet>
NFA<StateSet>& NFA<StateSet>::operator=(NFA<StateSet>&other){
    this->PseudoNFA::operator=(other);
    states           =other.states;
    final_states     =other.final_states;
    states_n         =other.states_n;
    current_states[0]=other.current_states[0];
    current_states[1]=other.current_states[1];
    return *this;
}
template<class StateSet>
NFA<StateSet>& NFA<StateSet>::operator=(NFA<StateSet>&& other){
    this->PseudoNFA::operator=(other);
    states           =other.states;
    other.states     =nullptr;
    states_n         =other.states_n;
    current_states[0]=std::move(other.current_states[0]);
    current_states[1]=std::move(other.current_states[1]);
    final_states     =std::move(other.final_states);
    if constexpr(std::is_same<StateSet, Roaring>::value){
        other.final_states=std::move(Roaring::bitmapOf(1,other.initial_state));
    }else{
        StateSet s;
        s.add(other.initial_state);
        other.final_states=s;
    }
    return *this;
}
template<class StateSet>
NFA<StateSet>::NFA(uint32_t cur_n,uint32_t states_n_arg,StateSet*states):PseudoNFA(cur_n,1,states),Executable(states_n_arg),states(states){
    if constexpr(std::is_same<StateSet, Roaring>::value) final_states=std::move(Roaring::bitmapOf(1,cur_n));
    else{
        final_states=StateSet();
        final_states.add(cur_n);
    }
}
template<class StateSet>
NFA<StateSet>::NFA(uint32_t cur_n,char c,uint32_t states_n_arg, StateSet* states):PseudoNFA(cur_n,c,2,states),Executable(states_n_arg), states(states){
    //std::cout<<"single character: "<<c<<"\tcur_n: "<<cur_n<<std::endl;
    states[idx(cur_n,(int)c,true)].add(cur_n+1);
    states[idx(cur_n+1,(int)c,false)].add(cur_n);
    if constexpr(std::is_same<StateSet, Roaring>::value) final_states=std::move(Roaring::bitmapOf(1,cur_n+1));
    else{
        final_states=StateSet();
        final_states.add(cur_n+1);
    }
}
template<class StateSet>
NFA<StateSet> operator+(NFA<StateSet>& input,int rotate){
    NFA<StateSet> ret(input.initial_state+rotate,input.states_n,input.states);
    ret.size=input.size;
    for (uint32_t i=input.initial_state;i<input.initial_state+input.size; i++){
        for(uint32_t c=0;c<0x80;c++) ret.states[i+rotate+input.states_n*(2*c)]=std::move(input.states[i+input.states_n*(2*c)]+rotate);
        for(uint32_t c=0;c<0x80;c++) ret.states[i+rotate+input.states_n*(1+2*c)]=std::move(input.states[i+input.states_n*(1+2*c)]+rotate);
    }
    ret.final_states=std::move(input.final_states+rotate);
    return ret;
}
template<class StateSet>
template<bool fwd>
NFA<StateSet>& NFA<StateSet>::shift(char c){
    if constexpr(std::is_same<StateSet, Roaring>::value){
        size_t current_states_cardinality=current_states[fwd].cardinality();
        current_states[fwd].toUint32Array(gather_for_fastunion);
        for(uint32_t i=0;i<current_states_cardinality;i++)
            select_for_fastunion[i]=&states[idx(gather_for_fastunion[i],c,fwd)];
        current_states[fwd] = Roaring::fastunion(current_states_cardinality,
                                            const_cast<const Roaring**>(select_for_fastunion));
    }else{
        typename StateSet::iterator i(current_states[fwd]);
        while(++i>=0) current_states[fwd]|=states[idx(*i,c,fwd)];
    }
    return *this;
}
template<class StateSet>
NFA<StateSet>& NFA<StateSet>::operator<<(char c)
{
    return shift<true>(c);
}
template<class StateSet>
NFA<StateSet>& NFA<StateSet>::operator>>(char c)
{
    return shift<false>(c);
}
template<class StateSet>
uint8_t NFA<StateSet>::operator*(){
    return (current_states[0].contains(initial_state)<<1)|(current_states[1].and_cardinality(final_states)>0);
};
template<class StateSet>
template<bool fwd>
void NFA<StateSet>::skip(uint32_t n,uint32_t k){
    uint32_t to_skip=fwd?k:n;
    uint32_t fixed=fwd?n:k;
    //std::cout<<"to_skip: "<<to_skip<<"\tfixed: "<<fixed<<std::endl;
    for(unsigned char c=0;c<0x80;c++){
        states[idx(fixed,c,fwd)] |=states[idx(to_skip,c,fwd)];
        if constexpr(std::is_same<StateSet, Roaring>::value){
            for(Roaring::const_iterator j =states[idx(to_skip,c,fwd)].begin();
                                        j!=states[idx(to_skip,c,fwd)].end();j++){
                //std::cout<<"inbound: "<<*j;
                states[idx(*j,c,!fwd)].add(fixed);
            }
        }else{
            typename StateSet::iterator i(states[idx(to_skip,c,fwd)]);
            while(++i>=0) states[idx(*i,c,!fwd)].add(fixed);
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
    if constexpr(std::is_same<StateSet, Roaring>::value){
        for(Roaring::const_iterator i = final_states.begin(); i != final_states.end(); i++) skip<false>(*i,other.initial_state);
    } else{
        typename StateSet::iterator i(final_states);
        while(++i>=0) skip<false>(*i,other.initial_state);
    }
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
    //std::cout<<"################################# merged to NFA: "<<std::endl;
    //std::cout<<*this;
    return *this;
}
template<class StateSet>
NFA<StateSet>& NFA<StateSet>::operator*(unsigned int n){
    if(!n) return *this;
    if constexpr(std::is_same<StateSet, Roaring>::value){
        for(Roaring::const_iterator i = final_states.begin(); i != final_states.end(); i++) skip<false>(*i,initial_state);
    } else{
        typename StateSet::iterator i(final_states);
        while(++i>=0) skip<false>(*i,initial_state);
    }
    final_states.add(initial_state);
    return *this;
}
