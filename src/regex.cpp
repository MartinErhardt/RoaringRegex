/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#include<stdio.h>
#include <stdbool.h>
#include <string.h>
#include "../CRoaring/roaring.hh"
#include "inc/regex.h"
#include<stack>
using namespace roaring;
using namespace Regex;
#define ALIGN 128
NFA::NFA(NFA&& to_move):states(std::move(to_move.states)),
                        current_states(std::move(to_move.current_states)),
                        final_states(std::move(to_move.final_states)),
                        initial_state(to_move.initial_state){
    gather_for_fastunion=to_move.gather_for_fastunion;
    select_for_fastunion=to_move.select_for_fastunion;
    to_move.gather_for_fastunion=nullptr;
    to_move.select_for_fastunion=nullptr;
}
NFA::NFA(const NFA& to_copy): states(to_copy.states),
                        current_states(to_copy.current_states),
                        final_states(to_copy.final_states),
                        initial_state(to_copy.initial_state){
    if(to_copy.gather_for_fastunion){
        gather_for_fastunion=new (std::align_val_t{ALIGN}) uint32_t [states.size()];
        select_for_fastunion=new (std::align_val_t{ALIGN}) Roaring*[states.size()];
        memcpy(gather_for_fastunion, to_copy.gather_for_fastunion, states.size()*sizeof(uint32_t));
        memcpy(select_for_fastunion, to_copy.select_for_fastunion, states.size()*sizeof(Roaring*));
    }
}
NFA& NFA::operator=(NFA&&other){
    states=std::move(other.states);
    current_states=std::move(other.current_states);
    final_states=std::move(other.final_states);
    initial_state=other.initial_state;
    gather_for_fastunion=other.gather_for_fastunion;
    select_for_fastunion=other.select_for_fastunion;
    other.gather_for_fastunion=nullptr;
    other.select_for_fastunion=nullptr;
    return *this;
}
NFA::NFA(uint32_t cur_n):final_states(std::move(Roaring::bitmapOf(cur_n))),
                         initial_state(cur_n){}
NFA::NFA(uint32_t cur_n,char c):
                        final_states(std::move(Roaring::bitmapOf(cur_n+1))),
                        initial_state(cur_n){
    state new_state;
    new_state.tr[(int)c]=Roaring::bitmapOf(cur_n+1);
    states.push_back(std::move(new_state));
}

NFA::~NFA(){
    operator delete(gather_for_fastunion, std::align_val_t{ALIGN});
    operator delete(select_for_fastunion, std::align_val_t{ALIGN});
}
NFA NFA::bracket_expression(const char* start, int* i,uint32_t* cur_n,int ps,const char *p){
    bool escaped=false;
    while(*i<ps&&!(!(escaped^=(p[*i]=='\\'))&&p[(*i)++]==']'));
    if(*i==ps) throw std::runtime_error("invalid expression!");
    return NFA((*cur_n)++);
}
NFA NFA::build_NFA(const char* p){
    bool escaped=false;
    std::stack<NFA> nfas;
    std::stack<operation> ops;
    int ps=strlen(p);
    uint32_t cur_n=0;
    auto clear_stack=[&](){
        NFA& cur_nfa=nfas.top();
        while(nfas.size()>1&&ops.top()!=BRACKETS){
            //cur_n-=nfas.top().states.size();
            nfas.pop();
            ops.pop();
            if(ops.top()==CONCATENATION)   nfas.top()*=cur_nfa;
            else if(ops.top()==OR){
                NFA intermediate(std::move(nfas.top()));
                nfas.pop();
                ops.pop();
                cur_nfa=nfas.top();
                while(nfas.size()>1&&ops.top()==CONCATENATION){
                    nfas.pop();
                    ops.pop();
                    nfas.top()*=cur_nfa;
                    cur_nfa=nfas.top();
                }
                nfas.top()|=intermediate;
            }
            //nfas.top().op_eval(cur_op)(cur_nfa);
            cur_nfa=nfas.top();
        }
    };
    auto repeat=[&](){
        nfas.push(*nfas.top()+nfas.top().states.size());
        cur_n+=nfas.top().states.size();
        ops.push(CONCATENATION);
    };
    for(int i=0;i<ps;){
        if(p[i]=='\\'){
            escaped^=escaped;
            i++;
        } else switch(p[i]*(!escaped)) {
            case '[':
                nfas.push(NFA::bracket_expression(&p[i],&i,&cur_n,ps,p));
                ops.push(CONCATENATION);
                break;
            case '(':
                nfas.push(NFA(cur_n++));
                ops.push(BRACKETS);
                i++;
                break;
            case '|':
                nfas.push(NFA(cur_n++));
                ops.push(OR);
                i++;
                break;
            case ')':
                clear_stack();
                i++;
                break;
            case '*':
                nfas.top()*1;
                break;
            case '+':
                repeat();
                nfas.top()*1;
                ops.push(CONCATENATION);
                break;
            case '?':
                nfas.top()|=NFA(cur_n++);
                i++;
                break;
            case '{':
            {
                int j=i++;
                NFA repeated(nfas.top());
                while(i<ps&&!(p[i]!='}'||p[i]==',')) i++;
                if(i==ps) throw std::runtime_error("invalid expression!");
                int m=strtol(&p[j],nullptr,10);
                for(int i=0;i<m;i++) repeat();
                if(p[i]!='}'){
                    int j=i;
                    while(i<ps&&p[i++]!='}');
                    if(i==ps) throw std::runtime_error("invalid expression!");
                    int n=strtol(&p[j],nullptr,10);
                    if(!n){
                        repeat();
                        nfas.top()*1;
                    } else if(n>m){
                        repeated|=NFA(cur_n++);
                        for(int k=m;k<n;k++) nfas.push(repeated);
                    }
                }
                break;
            }
            case '^':
            case '$':
                nfas.push(NFA(cur_n++,'\0'));
                ops.push(CONCATENATION);
                break;
            default:
                nfas.push(NFA(cur_n++,p[i++]));
                ops.push(CONCATENATION);
                break;
        }
    }
    clear_stack();
    if(nfas.size()!=1) throw std::runtime_error("invalid expression");
    NFA new_nfa=nfas.top();
    nfas.pop();
    new_nfa.gather_for_fastunion=new(std::align_val_t{ALIGN}) uint32_t[new_nfa.states.size()];
    new_nfa.select_for_fastunion=new(std::align_val_t{ALIGN}) Roaring*[new_nfa.states.size()];
    return new_nfa;
}
#define idx(c,fwd) ([&]{                          \
          if constexpr (fwd) return ((size_t)c); \
          else return ((size_t)c)+0x100;         \
        }())
template<bool fwd>
NFA& NFA::shift(char c){
    size_t current_states_cardinality=current_states.cardinality();
    current_states.toUint32Array(gather_for_fastunion);
    for(uint32_t i=0;i<current_states_cardinality;i++)
        select_for_fastunion[i]=&states[gather_for_fastunion[i]-initial_state].tr[idx(c,fwd)];
    current_states = Roaring::fastunion(current_states_cardinality,
                                        const_cast<const roaring::Roaring**>(select_for_fastunion));
    return *this;
}
NFA& NFA::operator<<(char c)
{
    return shift<true>(c);
}
NFA& NFA::operator>>(char c)
{
    return shift<false>(c);
}
uint8_t NFA::operator*(){
    return (current_states.contains(initial_state)<<1)|(current_states.and_cardinality(final_states)>0);
};
template<bool fwd>
void NFA::skip(uint32_t n,uint32_t k){
    uint32_t to_skip=fwd?k-initial_state:n-initial_state;
    uint32_t fixed=fwd?n-initial_state:k-initial_state;
    for(char c=0;c<0xFF;c++){
        states[fixed].tr[idx(c,fwd)]|=states[to_skip].tr[idx(c,fwd)];
        for(Roaring::const_iterator j=states[to_skip].tr[idx(c,fwd)].begin();
                                    j!=states[to_skip].tr[idx(c,fwd)].end();j++){
            //constexpr if(!fwd) states[j].tr[idx(*j,!fwd)].remove(to_skip);
            states[*j-initial_state].tr[idx(c,!fwd)].add(fixed);
        }
    }
}
NFA& NFA::operator*=(NFA& other){
    uint32_t next_initial=states.size();
    final_states=other.final_states;
    states.insert( states.end(), other.states.begin(), other.states.end() );
    for(Roaring::const_iterator i = final_states.begin(); i != final_states.end(); i++)
        skip<false>(*i,next_initial);
    if(final_states.contains(initial_state)) skip<true>(initial_state,next_initial);
    return *this;
}
NFA& NFA::operator|=(NFA& other){
    int next_initial=states.size();
    final_states|=other.final_states;
    states.insert( states.end(), other.states.begin(), other.states.end() );
    skip<true>(initial_state,next_initial);
    return *this;
}
NFA& NFA::operator*(unsigned int n){
    if(!n) return *this;
    for(Roaring::const_iterator i = final_states.begin(); i != final_states.end(); i++)
        skip<false>(*i,initial_state);
    final_states.add(initial_state);
    return *this;
}
roaring::Roaring operator+(roaring::Roaring& set,int32_t rotate){
    roaring::Roaring ret;
    for(Roaring::const_iterator i = set.begin(); i != set.end(); i++)
        ret.add(*i+rotate);
    return ret;
}
NFA NFA::operator+(int32_t rotate){
    NFA ret(initial_state+rotate);
    ret.states.reserve(states.size());
    for (uint32_t i=0;i<states.size(); i++)
        for(uint32_t c=0;c<0x200;c++)  {
            state new_state;
            new_state.tr[c]=states[i].tr[c]+rotate;
            ret.states.push_back(std::move(new_state));
        }
    ret.final_states=final_states+rotate;
    return ret;
}
int main(){
    char* text=NULL;
    char* pattern=NULL;
    size_t len;
    if (getline(&text, &len, stdin) == -1||getline(&pattern, &len, stdin)==-1)
            return -1;
    text[strlen(text)-1]='\0';
    pattern[strlen(pattern)-1]='\0';
    return 0;
}
