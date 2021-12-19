/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#include<stdio.h>
#include<stdbool.h>
#include<string.h>
#include"../CRoaring/roaring.hh"
#include"inc/regex.h"
#include<stack>
#include<iostream>
#include<chrono>
template<class NFA_t,class StateSet>
NFA_t Lexer::bracket_expression(const char* start, int* i,uint32_t new_initial,int ps,const char *p,StateSet* states){
    bool escaped=false;
    while(*i<ps&&!(!(escaped^=(p[*i]=='\\'))&&p[(*i)++]==']'));
    if(*i==ps) throw std::runtime_error("invalid expression!");
    return NFA_t(new_initial,states_n,states);
}
std::ostream& operator<<(std::ostream& out, PseudoNFA const& pnfa){
    out<<"size: "<<pnfa.size<<std::endl;
    return out;
}
PseudoNFA operator+(PseudoNFA& input,int rotate){
    return PseudoNFA(input.initial_state+rotate,input.size);
}
#define next_initial(stack) (stack.empty()?0:stack.top().initial_state+stack.top().size)
template<class NFA_t,class StateSet>
NFA_t Lexer::build_NFA(const char* p, StateSet* states){
    bool escaped=false;
    std::stack<NFA_t> nfas;
    std::stack<operation> ops;
    ops.push(CONCATENATION);
    int ps=strlen(p);
    auto clear_stack=[&](){
        NFA_t cur_nfa(std::move(nfas.top())); //FIXME not in printable state
        //std::cout<<"clear stack"<< nfas.top()<<std::endl;
        if(ops.size()){
            ops.pop();
            //std::cout<<"ops top: "<<ops.top()<<"\tnfas size: "<<nfas.size()<<std::endl;
            while(nfas.size()>1&&ops.top()!=BRACKETS){
                //std::cout<<"################################# intermediate NFA: "<<std::endl;
                //std::cout<<cur_nfa;
                //std::cout<<"op: "<<ops.top()<<std::endl;
                if(ops.top()==CONCATENATION){   
                    nfas.pop();
                    ops.pop();
                    //std::cout<<"size now: "<<nfas.top().size<<std::endl;
                    nfas.top()*=cur_nfa;
                    cur_nfa=std::move(nfas.top());
                }
                else if(ops.top()==OR){
                    NFA_t intermediate(std::move(cur_nfa));
                    ops.pop();
                    ops.pop();
                    nfas.pop();
                    cur_nfa=std::move(nfas.top());
                    while(nfas.size()>1&&ops.top()==CONCATENATION){
                        //std::cout<<"pop"<<std::endl;
                        nfas.pop();
                        ops.pop();
                        nfas.top()*=cur_nfa;
                        cur_nfa=std::move(nfas.top());
                    }
                    cur_nfa|=intermediate;
                }
            }
            //std::cout<<"################################# intermediate final NFA: "<<std::endl;
            //std::cout<<cur_nfa;
        }
        ops.push(CONCATENATION);
        nfas.pop();
        nfas.push(std::move(cur_nfa));
    };
    auto repeat=[&](){
        nfas.push(std::move(nfas.top()+(((int32_t)nfas.top().size))));
        ops.push(CONCATENATION);
    };
    for(int i=0;i<ps;){
        if(p[i]=='\\'){
            escaped^=escaped;
            i++;
        } else switch(p[i]*(!escaped)) {
            case '[':
                nfas.push(Lexer::bracket_expression<NFA_t>(&p[i],&i,nfas.top().initial_state+nfas.top().size,ps,p,states));
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
                //std::cout<<nfas.top();
                //std::cout<<"Top"<<std::endl;
                //
                nfas.top()*1;
                //std::cout<<nfas.top();
                i++;
                break;
            case '?':
                nfas.top()|=NFA_t(next_initial(nfas),states_n,states);
                //nfas.top().final_states.add(nfas.top().initial_state);
                i++;
                break;
            case '{':
            {
                int j=i++;
                NFA_t repeated(nfas.top());
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
                        repeated|=NFA_t(next_initial(nfas),states_n,states);
                        for(int k=m;k<n;k++){
                            nfas.push(repeated);
                            ops.push(CONCATENATION);
                        }
                    }
                }
                break;
            }
            case '^':
            case '$':
                nfas.push(NFA_t(next_initial(nfas),'\0',states_n,states));
                ops.push(CONCATENATION); //states
                i++;
                break;
            default:
                //uint32_t next_s=next_initial(nfas);
                //if(!nfas.empty())std::cout<<"TOP initial"<<nfas.top().initial_state<<"TOP size"<<nfas.top().size<<"\tcur_n: "<<next_s<<std::endl;
                nfas.push(NFA_t(next_initial(nfas),p[i++],states_n,states));
                ops.push(CONCATENATION);
                //std::cout<<"TOP2 initial"<<nfas.top().initial_state<<"TOP2 size"<<nfas.top().size<<"\tcur_n: "<<next_s<<std::endl;
                //std::cout<<"################################# intermediate NFA: "<<p[i-1]<<std::endl;
                //std::cout<<nfas.top();
                break;
        }
    }
    clear_stack();
    if(nfas.size()!=1) throw std::runtime_error("invalid expression");
    NFA_t new_nfa=nfas.top();
    nfas.pop();
    return new_nfa;
} //push_back

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
    //std::cout<<"size: "<<sizeof(my_nfa)<<std::endl;
    free(text);
    free(pattern);
    //l.~Lexer();
    return 0;
}
