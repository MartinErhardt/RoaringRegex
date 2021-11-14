/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#include<stdio.h>
#include <stdbool.h>
#include <string.h>
#include "../CRoaring/roaring.hh"
#include "inc/regex.h"
#include<stack>
#include <iostream>
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
NFA& NFA::operator=(NFA&other){
    states          =other.states;
    initial_state   =other.initial_state;
    final_states    =other.final_states;
    current_states  =other.current_states;
    if(other.gather_for_fastunion){
        gather_for_fastunion=new (std::align_val_t{ALIGN}) uint32_t[states.size()];
        select_for_fastunion=new (std::align_val_t{ALIGN}) Roaring*[states.size()];
        memcpy(gather_for_fastunion, other.gather_for_fastunion, states.size()*sizeof(uint32_t));
        memcpy(select_for_fastunion, other.select_for_fastunion, states.size()*sizeof(Roaring*));
    }
    return *this;
}
NFA& NFA::operator=(NFA&& other){
    states          =std::move(other.states);
    current_states  =std::move(other.current_states);
    final_states    =std::move(other.final_states);
    other.states.push_back(state{});
    //std::cout<<"initial state: "<<std::endl;
    other.final_states=Roaring::bitmapOf(1,other.initial_state);
    //std::cout<<"Success!"<<std::endl;
    initial_state=other.initial_state;
    gather_for_fastunion=other.gather_for_fastunion;
    select_for_fastunion=other.select_for_fastunion;
    other.gather_for_fastunion=nullptr;
    other.select_for_fastunion=nullptr;
    return *this;
}
NFA::NFA(uint32_t cur_n):final_states(std::move(Roaring::bitmapOf(1,cur_n))),
                         initial_state(cur_n){
    states.push_back(state{});
}
NFA::NFA(uint32_t cur_n,char c):
                        final_states(Roaring::bitmapOf(1,cur_n+1)),
                        initial_state(cur_n){
    std::cout<<"single character: "<<c<<"\tcur_n: "<<cur_n<<std::endl;
    //std::cout<<"test: ";
    //Roaring::bitmapOf(1,2,3,4,5,6).printf();
    //std::cout<<std::endl;
    state new_state1{};
    state new_state2{};
    new_state1.tr[(int)c]=Roaring::bitmapOf(1,cur_n+1);
    new_state2.tr[0x100+(int)c]=Roaring::bitmapOf(1,cur_n);
    states.push_back(std::move(new_state1));
    states.push_back(std::move(new_state2));
}

NFA::~NFA(){
    if(gather_for_fastunion){
        //std::cout<<"gather: "<<gather_for_fastunion<<std::endl;
        //std::cout<<"select: "<<select_for_fastunion<<std::endl;
        operator delete[](gather_for_fastunion, std::align_val_t{ALIGN});
        operator delete[](select_for_fastunion, std::align_val_t{ALIGN});
    }
}
NFA NFA::bracket_expression(const char* start, int* i,uint32_t* cur_n,int ps,const char *p){
    bool escaped=false;
    while(*i<ps&&!(!(escaped^=(p[*i]=='\\'))&&p[(*i)++]==']'));
    if(*i==ps) throw std::runtime_error("invalid expression!");
    return NFA((*cur_n)++);
}
std::ostream& operator<<(std::ostream& out, NFA const& nfa){
    out<<"initial state: "<<nfa.initial_state<<std::endl;
    out<<"final states: ";
    nfa.final_states.printf();
    out<<std::endl;
    for(uint32_t i=0;i<nfa.states.size();i++){
        out<<"state: "<<i+nfa.initial_state<<std::endl;
        out<<"forward transitions: "<<std::endl;
        for(unsigned char c=0; c<0xFF;c++){
            const Roaring& r=nfa.states[i].tr[(int)c];
            //std::cout<<(int) c<<std::endl;
            if(!r.cardinality()) continue;
            out<<c<<": ";
            r.printf();
            out<<std::endl;
        }
        out<<"backward transitions: "<<std::endl;
        for(unsigned char c=0; c<0xFF;c++){
            const Roaring& r=nfa.states[i].tr[0x100+((int)c)];
            //std::cout<<(int) c<<std::endl;
            if(!r.cardinality()) continue;
            out<<c<<": ";
            r.printf();
            out<<std::endl;
        }
    }
    return out;
}
Roaring operator+(Roaring& set,int32_t rotate){
    Roaring ret;
    for(Roaring::const_iterator i = set.begin(); i != set.end(); i++)
        ret.add(*i+rotate);
    return ret;
}
NFA operator+(NFA& input,int rotate){
    NFA ret(input.initial_state+rotate);
    ret.states.reserve(input.states.size());
    for (uint32_t i=0;i<input.states.size(); i++){
        ret.states.push_back(state{});
        for(uint32_t c=0;c<0x200;c++) ret.states[i].tr[c]=std::move(input.states[i].tr[c]+rotate);
    }
    ret.final_states=input.final_states+rotate;
    return ret;
}
NFA NFA::build_NFA(const char* p){
    bool escaped=false;
    std::stack<NFA> nfas;
    std::stack<operation> ops;
    ops.push(CONCATENATION);
    int ps=strlen(p);
    uint32_t cur_n=0;
    auto clear_stack=[&](){
        NFA cur_nfa(std::move(nfas.top()));
        ops.pop();
        while(nfas.size()>1&&ops.top()!=BRACKETS){
            cur_n-=nfas.top().states.size();
            std::cout<<"################################# intermediate NFA: "<<std::endl;
            std::cout<<cur_nfa;
            std::cout<<"op: "<<ops.top()<<std::endl;
            if(ops.top()==CONCATENATION){   
                nfas.pop();
                ops.pop();
                nfas.top()*=cur_nfa;
                cur_nfa=std::move(nfas.top());
            }
            else if(ops.top()==OR){
                NFA intermediate(std::move(cur_nfa));
                ops.pop();
                nfas.pop();
                cur_nfa=std::move(nfas.top());
                while(nfas.size()>1&&ops.top()==CONCATENATION){
                    std::cout<<"pop"<<std::endl;
                    nfas.pop();
                    ops.pop();
                    nfas.top()*=cur_nfa;
                    cur_nfa=std::move(nfas.top());
                }
                cur_nfa|=intermediate;
                //std::cout<<"################################# WTF? "<<std::endl;
                //std::cout<<cur_nfa;
            }
            //std::cout<<"################################# WTF? "<<std::endl;
            //std::cout<<cur_nfa;
        }
        std::cout<<"################################# intermediate final NFA: "<<std::endl;
        std::cout<<cur_nfa;
        nfas.pop();
        ops.pop();
        nfas.push(std::move(cur_nfa));
    };
    auto repeat=[&](){
        //std::cout<<"rotate PLS!"<<std::endl;
        nfas.push(nfas.top()+(int32_t)nfas.top().states.size());
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
                //nfas.push(NFA(cur_n+1));
                //nfas.top().final_states.add(cur_n-1);
                ops.push(BRACKETS);
                i++;
                break;
            case '|':
                ops.push(OR);
                i++;
                break;
            case ')':
                clear_stack();
                i++;
                break;
            case '*':
                nfas.top()*1;
                i++;
                break;
            case '+':
                repeat();
                //std::cout<<"Top"<<std::endl;
                //std::cout<<nfas.top();
                nfas.top()*1;
                i++;
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
                nfas.push(NFA(cur_n,'\0'));
                ops.push(CONCATENATION);
                cur_n+=2;
                break;
            default:
                nfas.push(NFA(cur_n,p[i++]));
                ops.push(CONCATENATION);
                //std::cout<<"################################# intermediate NFA: "<<p[i-1]<<std::endl;
                //std::cout<<nfas.top();
                //std::cout<<nfas.top();
                cur_n+=2;
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
#define idx(c,fwd) ([&]{                         \
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
                                        const_cast<const Roaring**>(select_for_fastunion));
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
    //std::cout<<"to_skip: "<<to_skip<<"\tfixed: "<<fixed<<std::endl;
    for(unsigned char c=0;c<0xFF;c++){
        states[fixed].tr[idx(c,fwd)] |=states[to_skip].tr[idx(c,fwd)];
        for(Roaring::const_iterator j =states[to_skip].tr[idx(c,fwd)].begin();
                                    j!=states[to_skip].tr[idx(c,fwd)].end();j++){
            //std::cout<<"inbound: "<<*j;
            states[*j-initial_state].tr[idx(c,!fwd)].add(fixed+initial_state);
        }
    }
}
NFA& NFA::operator*=(NFA& other){
    std::cout<<"################################# merge NFA: "<<std::endl;
    std::cout<<other;
    std::cout<<"################################# merge into NFA: "<<std::endl;
    std::cout<<*this;
    uint32_t next_initial=other.initial_state;
    //std::cout<<"concat1"<<std::endl;
    states.insert(states.end(),other.states.begin(),other.states.end());
    //std::cout<<"concat2"<<std::endl;
    for(Roaring::const_iterator i = final_states.begin(); i != final_states.end(); i++)
        skip<false>(*i,next_initial);
    final_states=other.final_states;
    if(final_states.contains(initial_state)) skip<true>(initial_state,next_initial);
    return *this;
}
NFA& NFA::operator|=(NFA& other){
    std::cout<<"################################# union NFA: "<<std::endl;
    std::cout<<other;
    std::cout<<"################################# union into NFA: "<<std::endl;
    std::cout<<*this;
    int next_initial=other.initial_state;
    final_states|=other.final_states;
    states.insert(states.end(),other.states.begin(),other.states.end());
    skip<true>(initial_state,next_initial);
    if(other.final_states.contains(next_initial))  final_states.add(initial_state);
    //std::cout<<"################################# merged to NFA: "<<std::endl;
    //std::cout<<*this;
    return *this;
}
NFA& NFA::operator*(unsigned int n){
    if(!n) return *this;
    //final_states.printf();
    for(Roaring::const_iterator i = final_states.begin(); i != final_states.end(); i++){
        //final_states.printf();
        skip<false>(*i,initial_state);
    }
    final_states.add(initial_state);
    return *this;
}
int main(){
    char* text=NULL;
    char* pattern=NULL;
    size_t len;
    if (getline(&text, &len, stdin) == -1||getline(&pattern, &len, stdin)==-1)
            return -1;
    text[strlen(text)-1]='\0';
    pattern[strlen(pattern)-1]='\0';
    NFA my_nfa=NFA::build_NFA(pattern);
    std::cout<<"################################# final NFA: "<<std::endl;
    std::cout<<my_nfa;
    //std::cout<<"size: "<<sizeof(my_nfa)<<std::endl;
    free(text);
    free(pattern);
    return 0;
}
