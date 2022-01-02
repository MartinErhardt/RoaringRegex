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
#include"BitSet.h"
using namespace Regex;
using namespace roaring;
template<typename NFA_t>
NFA_t RRegex::bracket_expression(const char* start,const char** cp2,uint32_t new_initial,int ps,const char *p,void* states){
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
    return NFA_t(new_initial,charset,states_n,states);
}
template<typename NFA_t>
NFA_t RRegex::build_NFA(const char* p, void* states){
    bool escaped=false;
    std::stack<NFA_t> nfas;
    std::stack<operation> ops;
    ops.push(BRACKETS);
    int ps=strlen(p);
    const char* cp=p;
    auto clear_stack=[&nfas,&ops](){
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
    auto repeat=[&nfas,&ops](){
        nfas.push(std::move(nfas.top()+(((int32_t)nfas.top().size))));
        ops.push(CONCATENATION);
    };
    auto next_initial=[](std::stack<NFA_t>& stack)->uint64_t{
        return (stack.empty()?0:stack.top().initial_state+stack.top().size);
    };
    do{
        if(!escaped&&*cp=='\\'){
            escaped=true;
            continue;
        }
        switch((*cp)*(!escaped)) {
            case '[':
                nfas.push(std::move(RRegex::bracket_expression<NFA_t>(cp,&cp,next_initial(nfas),ps-(cp-p),p,states)));
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
                nfas.push(std::move(NFA_t(next_initial(nfas),charset,states_n,states)));
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
                nfas.top()|=NFA_t(next_initial(nfas),states_n,states);
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
                        nfas.top()|=NFA_t(next_initial(nfas),states_n,states);
                        for(int k=m+1;k<n;k++) repeat();
                    }
                    cp=cn2;
                } else cp=cn1;
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
template<unsigned int k>
void RRegex::alloc_BitSetNFA(const char* p){
    memory_pool_size=sizeof(BitSet<k>)*0x100*states_n;
    memory_pool=malloc(memory_pool_size);
    memset(static_cast<char*>(memory_pool),0,memory_pool_size);
    BitSet<k>* states=reinterpret_cast<BitSet<k>*>(memory_pool);
    exec=std::make_unique<NFA<BitSet<k>>>(build_NFA<NFA<BitSet<k>>>(p,states));
}
RRegex::RRegex(const char* p){
    states_n=build_NFA<PseudoNFA>(p,nullptr).size;
    if(states_n>256){
        memory_pool_size=sizeof(Roaring)*0x100*states_n+sizeof(Roaring*)*states_n+sizeof(uint32_t*)*states_n;
        memory_pool=malloc(memory_pool_size);
        memset(static_cast<char*>(memory_pool),0,memory_pool_size); // conforms to strict-aliasing; initializes all Roaring bitmaps
        Roaring* states=reinterpret_cast<Roaring*>(memory_pool);
        exec=std::make_unique<NFA<Roaring>>(build_NFA<NFA<Roaring>>(p,states));
    }
    else if(states_n>128)   alloc_BitSetNFA<4>(p);
    else if(states_n>64)    alloc_BitSetNFA<2>(p);
    else                    alloc_BitSetNFA<1>(p);
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
    RRegex r(pattern);
    RRegex::GreedyIterator cur(text,r);
    RRegex::GreedyIterator end;
    for(;cur!=end;cur++) *cur;
    auto end_time = std::chrono::high_resolution_clock::now();
    std::cout<<"################################# final NFA: "<<std::endl;
    r.exec->print();
    RRegex::GreedyIterator cur2(text,r);
    for(;cur2!=end;cur2++) std::cout<<"match: "<<*cur2<<std::endl;
    std::cout<<"time: " <<std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count() << "ms\n";
    free(text);
    free(pattern);
    return 0;
}
