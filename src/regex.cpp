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
using namespace roaring;
using namespace Regex;
#define ALIGN 128
NFA::NFA(NFA&& to_move): PseudoNFA(to_move),states(to_move.states),
                        current_states(std::move(to_move.current_states)){
    current_states[0]=std::move(to_move.current_states[0]);
    current_states[1]=std::move(to_move.current_states[1]);
    final_states=std::move(to_move.final_states);
}
NFA::NFA(const NFA& to_copy):PseudoNFA(to_copy), states(to_copy.states){
    current_states[0]=to_copy.current_states[0];
    current_states[1]=to_copy.current_states[1];
    final_states=to_copy.final_states;
}
NFA& NFA::operator=(NFA&other){
    this->PseudoNFA::operator=(other);
    states           =other.states;
    final_states     =other.final_states;
    current_states[0]=other.current_states[0];
    current_states[1]=other.current_states[1];
    return *this;
}
NFA& NFA::operator=(NFA&& other){
    this->PseudoNFA::operator=(other);
    states          =other.states;
    other.states=nullptr;
    current_states[0]=std::move(other.current_states[0]);
    current_states[1]=std::move(other.current_states[1]);
    final_states     =std::move(other.final_states);
    other.final_states=Roaring::bitmapOf(1,other.initial_state);
    return *this;
}
NFA::NFA(uint32_t cur_n,uint32_t states_n,state*states):PseudoNFA(cur_n,states_n,states),states(states),final_states(std::move(Roaring::bitmapOf(1,cur_n))){
}
NFA::NFA(uint32_t cur_n,char c,uint32_t states_n, state* states):PseudoNFA(cur_n,c,states_n,states), states(states),
                        final_states(Roaring::bitmapOf(1,cur_n+1)){
    //std::cout<<"single character: "<<c<<"\tcur_n: "<<cur_n<<std::endl;
    states[cur_n].tr[(int)c].add(cur_n+1);
    states[cur_n+1].tr[0x100+(int)c].add(cur_n);
}
template<class NFA_t>
NFA_t Lexer::bracket_expression(const char* start, int* i,uint32_t new_initial,int ps,const char *p){
    bool escaped=false;
    while(*i<ps&&!(!(escaped^=(p[*i]=='\\'))&&p[(*i)++]==']'));
    if(*i==ps) throw std::runtime_error("invalid expression!");
    return NFA_t(new_initial,states_n,states);
}//std::ostream& operator<<(std::ostream& out, Executable const& exec){
//    out<<exec
//    return out;
//}
std::ostream& operator<<(std::ostream& out, PseudoNFA const& pnfa){
    out<<"size: "<<pnfa.size<<std::endl;
    return out;
}
Roaring operator+(Roaring& set,int32_t rotate){
    Roaring ret;
    for(Roaring::const_iterator i = set.begin(); i != set.end(); i++)
        ret.add(*i+rotate);
    return ret;
}
NFA operator+(NFA& input,int rotate){
    NFA ret(input.initial_state+rotate,input.size,input.states);
    for (uint32_t i=input.initial_state;i<input.initial_state+input.size; i++){
        for(uint32_t c=0;c<0x200;c++) ret.states[i].tr[c]=std::move(input.states[i].tr[c]+rotate);
    }
    ret.final_states=std::move(input.final_states+rotate);
    return ret;
}
PseudoNFA operator+(PseudoNFA& input,int rotate){
    return PseudoNFA(input.initial_state+rotate,input.size);
}
#define next_initial(stack) (stack.empty()?0:stack.top().initial_state+stack.top().size)
template<class NFA_t>
NFA_t Lexer::build_NFA(const char* p){
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
        nfas.push(nfas.top()+(int32_t)nfas.top().size);
        ops.push(CONCATENATION);
    };
    for(int i=0;i<ps;){
        if(p[i]=='\\'){
            escaped^=escaped;
            i++;
        } else switch(p[i]*(!escaped)) {
            case '[':
                nfas.push(Lexer::bracket_expression<NFA_t>(&p[i],&i,nfas.top().initial_state+nfas.top().size,ps,p));
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
                std::cout<<nfas.size()<<std::endl;
                nfas.top()*1;
                std::cout<<nfas.size()<<std::endl;
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
#define idx(c,fwd) ([&]{                         \
          if constexpr (fwd) return ((size_t)c); \
          else return ((size_t)c)+0x100;         \
        }())
template<bool fwd>
NFA& NFA::shift(char c){
    size_t current_states_cardinality=current_states[fwd].cardinality();
    current_states[fwd].toUint32Array(gather_for_fastunion);
    for(uint32_t i=0;i<current_states_cardinality;i++)
        select_for_fastunion[i]=&states[gather_for_fastunion[i]].tr[idx(c,fwd)];
    current_states[fwd] = Roaring::fastunion(current_states_cardinality,
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
    return (current_states[0].contains(initial_state)<<1)|(current_states[1].and_cardinality(final_states)>0);
};
template<bool fwd>
void NFA::skip(uint32_t n,uint32_t k){
    uint32_t to_skip=fwd?k:n;
    uint32_t fixed=fwd?n:k;
    //std::cout<<"to_skip: "<<to_skip<<"\tfixed: "<<fixed<<std::endl;
    for(unsigned char c=0;c<0xFF;c++){
        states[fixed].tr[idx(c,fwd)] |=states[to_skip].tr[idx(c,fwd)];
        for(Roaring::const_iterator j =states[to_skip].tr[idx(c,fwd)].begin();
                                    j!=states[to_skip].tr[idx(c,fwd)].end();j++){
            //std::cout<<"inbound: "<<*j;
            states[*j].tr[idx(c,!fwd)].add(fixed);
        }
    }
}
NFA& NFA::operator*=(NFA& other){
    this->PseudoNFA::operator*=(other);
    
    //std::cout<<"################################# merge NFA: "<<std::endl;
    //std::cout<<other;
    //std::cout<<"################################# merge into NFA: "<<std::endl;
    //std::cout<<*this;
    //std::cout<<"concat1"<<std::endl;
    //std::cout<<"concat2"<<std::endl;
    for(Roaring::const_iterator i = final_states.begin(); i != final_states.end(); i++)
        skip<false>(*i,other.initial_state);
    if(final_states.contains(initial_state)) skip<true>(initial_state,other.initial_state);
    if(final_states.contains(initial_state)&&other.final_states.contains(other.initial_state)){
        final_states=other.final_states;
        final_states.add(initial_state);
    } else final_states=other.final_states;
    return *this;
}
NFA& NFA::operator|=(NFA& other){
    this->PseudoNFA::operator|=(other);
    //std::cout<<"################################# union NFA: "<<std::endl;
    //std::cout<<other;
    //std::cout<<"################################# union into NFA: "<<std::endl;
    //std::cout<<*this;
    final_states|=other.final_states;
    skip<true>(initial_state,other.initial_state);
    if(other.final_states.contains(other.initial_state))  final_states.add(initial_state);
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
