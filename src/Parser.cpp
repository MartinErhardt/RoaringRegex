/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#include<stdio.h>
#include<stdbool.h>
#include<string.h>
//#include "../../CRoaring/roaring.hh"
#include "../CRoaring/roaring.c"
#include"regex.h"
#include<stack>
#include<iostream>
#include"BitSet.h"
using namespace Regex;
using namespace roaring;

template<class StateSet>
NFA<StateSet> RRegex::bracket_expression(const char* start,const char** cp2,uint32_t new_initial,int ps,const char *p,MemoryPool<StateSet>& mem_pool){
    bool escaped=false;
    bool complement=*(++*cp2)=='^';
    BitSet<2> charset;
    while(*cp2+1<start+ps&&!(**cp2==']'&&!escaped)){
        if(!escaped){
            char next=*((*cp2)+1);
            if(next!=']'&&*cp2+2<start+ps){
                char nextnext=*((*cp2)+2);
                if(next=='-'&&nextnext!=']'){
                    for(char c=**cp2;c<=nextnext;c++) charset.add(c);
                    ++++++(*cp2);
                    continue;
                }
            }
        }
        escaped=!escaped&&(**cp2=='\\');
        charset.add(*((*cp2)++));
    }
    if(*cp2-start==ps) throw std::runtime_error("invalid expression!");
    if(complement) charset.complement();
    return NFA<StateSet>(new_initial,charset,mem_pool);
}
template<class StateSet>
NFA<StateSet> RRegex::build_NFA(const char* p, unsigned int states_n){
    bool escaped=false;
    std::stack<NFA<StateSet>> nfas;
    std::stack<operation> ops;
    ops.push(BRACKETS);
    int ps=strlen(p);
    const char* cp=p;
    MemoryPool<StateSet> new_mempool(states_n);
    auto clear_stack=[&nfas,&ops](){
        NFA<StateSet> cur_nfa(std::move(nfas.top()));
        if(ops.size()){
            ops.pop();
            while(ops.size()>1&&ops.top()!=BRACKETS){
                if(ops.top()==CONCATENATION){   
                    nfas.pop();
                    ops.pop();
                    nfas.top()*=cur_nfa;
                    cur_nfa=std::move(nfas.top());
                }else if(ops.top()==OR){
                    NFA<StateSet> intermediate(std::move(cur_nfa));
                    ops.pop();
                    ops.pop();
                    nfas.pop();
                    cur_nfa=std::move(nfas.top());
                    while(ops.size()>1&&ops.top()==CONCATENATION){
                        nfas.pop();
                        ops.pop();
                        nfas.top()*=cur_nfa;
                        cur_nfa=std::move(nfas.top());
                    }
                    cur_nfa|=intermediate;
                }
            }
        }
        ops.pop();
        ops.push(CONCATENATION);
        nfas.pop();
        nfas.push(std::move(cur_nfa));
    };
    auto repeat=[&nfas,&ops](){
        nfas.push(std::move(nfas.top()+(((int32_t)nfas.top().size))));
        ops.push(CONCATENATION);
    };
    auto next_initial=[](std::stack<NFA<StateSet>>& stack)->uint64_t{
        return (stack.empty()?0:stack.top().initial_state+stack.top().size);
    };
    do{
        if(!escaped&&*cp=='\\'){
            escaped=true;
            continue;
        }
        switch((*cp)*(!escaped)) {
            case '[':
                nfas.push(std::move(RRegex::bracket_expression<StateSet>(cp,&cp,next_initial(nfas),ps-(cp-p),p,new_mempool)));
                ops.push(CONCATENATION);
                break;
            case '(':
                ops.push(BRACKETS);
                break;
            case '|':
                ops.push(OR);
                break;
            case ')':
                clear_stack();
                break;
            case '.':{
                BitSet<2> charset;
                charset.complement();
                nfas.push(std::move(NFA<StateSet>(next_initial(nfas),charset,new_mempool)));
                ops.push(CONCATENATION);
                break;
            }
            case '*':
                nfas.top()*1;
                break;
            case '+':
                repeat();
                nfas.top()*1;
                break;
            case '?':
                nfas.top()|=NFA<StateSet>(next_initial(nfas),new_mempool);
                break;
            case '{':{
                char* cn1;
                int m=strtol(++cp,&cn1,10);
                for(int i=0;i<m-1;i++) repeat();
                if(*cn1!='}'){
                    char* cn2;
                    int n=strtol(++cn1,&cn2,10);
                    if(!n){
                        repeat();
                        nfas.top()*1;
                    } else if(n>m){
                        repeat();
                        nfas.top()|=NFA<StateSet>(next_initial(nfas),new_mempool);
                        for(int k=m+1;k<n;k++) repeat();
                    }
                    cp=cn2;
                } else cp=cn1;
                break;
            }
            case '^':
            case '$':
                nfas.push(NFA<StateSet>(next_initial(nfas),'\0',new_mempool));
                ops.push(CONCATENATION); //states
                break;
            default:
                nfas.push(NFA<StateSet>(next_initial(nfas),*cp,new_mempool));
                ops.push(CONCATENATION);
                break;
        }
        escaped=false;
    }while(++cp-p<ps);
    clear_stack();
    if(nfas.size()!=1) throw std::runtime_error("invalid expression");
    NFA<StateSet> new_nfa=nfas.top();
    nfas.pop();
    return new_nfa;
}

RRegex::RRegex(const char* p2){
    char* p = (char*)p2;
    states_n=build_NFA<NoStateSet>(p,0).size;
#define CREATE_ITER_FAC(type) std::make_unique<NFA<type>::IterFactory>( NFA<type>::IterFactory(std::move(build_NFA<type>(p,states_n))))
    if(states_n>256)      iter_factory=CREATE_ITER_FAC(Roaring);
    else if(states_n>128) iter_factory=CREATE_ITER_FAC(BitSet<4>);
    else if(states_n>64)  iter_factory=CREATE_ITER_FAC(BitSet<2>);
    else                  iter_factory=CREATE_ITER_FAC(BitSet<1>);
    //exec->init(memory_pool, memory_pool_size, states_n);
}
//template<unsigned int k>
//template<BitSet<k>>
//MemoryPool<k,BitSet<k>>::
