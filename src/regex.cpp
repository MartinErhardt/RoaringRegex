/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#include<stdio.h>
#include<stdbool.h>
#include<string.h>
//#include "../../CRoaring/roaring.hh"
#include "../../CRoaring/roaring.c"
#include"regex.h"
#include<stack>
#include<iostream>
#include<chrono>
using namespace Regex;
using namespace roaring;
template<typename NFA_t>
NFA_t Lexer::bracket_expression(const char* start, const char** cp2,uint32_t new_initial,int ps,const char *p,void* states){
    bool escaped=false;
    while(*cp2-start<ps&&!(!(escaped^=(**cp2=='\\'))&&*((*cp2)++)==']'));
    if(*cp2-start==ps) throw std::runtime_error("invalid expression!");
    return NFA_t(new_initial,states_n,states);
}
/*
extern template class NFA<BitSet<1>>;
extern template class NFA<BitSet<2>>;
extern template class NFA<BitSet<4>>;
extern template class NFA<Roaring>;
*/
#define next_initial(stack) (stack.empty()?0:stack.top().initial_state+stack.top().size)
template<typename NFA_t>
NFA_t Lexer::build_NFA(const char* p, void* states){
    bool escaped=false;
    std::stack<NFA_t> nfas;
    std::stack<operation> ops;
    ops.push(BRACKETS);
    int ps=strlen(p);
    const char* cp=p;
    auto clear_stack=[&](){
        NFA_t cur_nfa(std::move(nfas.top())); //FIXME not in printable state
        if(ops.size()){
            ops.pop();
            while(ops.size()>1&&ops.top()!=BRACKETS){
                if(ops.top()==CONCATENATION){   
                    nfas.pop();
                    ops.pop();
                    nfas.top()*=cur_nfa;
                    cur_nfa=std::move(nfas.top());
                }else if(ops.top()==OR){
                    NFA_t intermediate(std::move(cur_nfa));
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
    auto repeat=[&](){
        nfas.push(std::move(nfas.top()+(((int32_t)nfas.top().size))));
        ops.push(CONCATENATION);
    };
    do{
        if(!escaped&&*cp=='\\'){
            escaped=true;
            continue;
        }
        switch((*cp)*(!escaped)) {
            case '[':
                nfas.push(Lexer::bracket_expression<NFA_t>(cp,&cp,nfas.top().initial_state+nfas.top().size,ps-(cp-p),p,states));
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
            case '*':
                nfas.top()*1;
                break;
            case '+':
                repeat();
                nfas.top()*1;
                break;
            case '?':
                nfas.top()|=NFA_t(next_initial(nfas),states_n,states);
                break;
            case '{':
            {
                char* cn1;
                NFA_t repeated(nfas.top());
                int m=strtol(cp,&cn1,10);
                for(int i=0;i<m;i++) repeat();
                if(*cn1!='}'){
                    char* cn2;
                    int n=strtol(cn1,&cn2,10);
                    if(!n){
                        repeat();
                        nfas.top()*1;
                    } else if(n>m){
                        repeated|=NFA_t(next_initial(nfas),states_n,states);
                        for(int k=m;k<n;k++){
                            nfas.push(repeated);
                            ops.push(CONCATENATION);
                        }
                    }
                }
                --cp;
                break;
            }
            case '^':
            case '$':
                nfas.push(NFA_t(next_initial(nfas),'\0',states_n,states));
                ops.push(CONCATENATION); //states
                break;
            default:
                nfas.push(NFA_t(next_initial(nfas),*cp,states_n,states));
                ops.push(CONCATENATION);
                break;
        }
        escaped=false;
    }while(++cp-p<ps);
    clear_stack();
    if(nfas.size()!=1) throw std::runtime_error("invalid expression");
    NFA_t new_nfa=nfas.top();
    nfas.pop();
    return new_nfa;
}
Lexer::Lexer(const char* p){
    states_n=build_NFA<PseudoNFA>(p,nullptr).size;
    void* memory_pool;
    size_t memory_pool_size;
    if(states_n>256){
        memory_pool_size=sizeof(Roaring)*0x100*states_n+sizeof(Roaring*)*states_n+sizeof(uint32_t*)*states_n;
        memory_pool=malloc(memory_pool_size);
        memset(static_cast<char*>(memory_pool),0,memory_pool_size); // conforms to strict-aliasing; initializes all Roaring bitmaps
        
        Roaring* states=reinterpret_cast<Roaring*>(memory_pool);
        exec=std::make_unique<NFA<Roaring>>(build_NFA<NFA<Roaring>>(p,states));
    }
#define alloc(k){                                                                               \
        memory_pool_size=sizeof(BitSet<k>)*0x100*states_n;                                      \
        memory_pool=malloc(memory_pool_size);                                                   \
        memset(static_cast<char*>(memory_pool),0,memory_pool_size);                             \
        BitSet<k>* states=reinterpret_cast<BitSet<k>*>(memory_pool);                            \
        exec=std::make_unique<NFA<BitSet<k>>>(build_NFA<NFA<BitSet<k>>>(p,states));}
    else if(states_n>128) alloc(4)
    else if(states_n>64)  alloc(2)
    else alloc(1)
    exec->init(memory_pool, memory_pool_size, states_n);
}
int main(){
    char* text=NULL;
    char* pattern=NULL;
    size_t len;
    if (getline(&text, &len, stdin) == -1||getline(&pattern, &len, stdin)==-1)
            return -1;
    text[strlen(text)-1]='\0';
    pattern[strlen(pattern)-1]='\0';
    auto start_time = std::chrono::high_resolution_clock::now();
    Lexer l(pattern);
    auto end_time = std::chrono::high_resolution_clock::now();
    std::cout<<"################################# final NFA: "<<std::endl;
    l.exec->print();
    std::cout<<"time: " <<(end_time - start_time)/std::chrono::milliseconds(1) << "ms\n";
    free(text);
    free(pattern);
    return 0;
}

