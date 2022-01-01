/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#include"regex.h"
using namespace Regex;
char* RRegex::LazyIterator::operator<<(char* s){
    if(!*cur_s) return nullptr;
    while(!((*(*exe<<*(cur_s++)))&FLAG_ACCEPTING)&&*cur_s);// std::cout<<"welp: "<<(int)(ret&FLAG_ACCEPTING)<<std::endl;
    if(!(*cur_s)&&**exe&FLAG_ACCEPTING) return cur_s; //minus null character
    else if(!*cur_s){
        *exe<<'\0';
        if(**exe&FLAG_ACCEPTING) return cur_s;
        else return nullptr;
    }else return cur_s;
};
char* RRegex::LazyIterator::operator>>(char* s){
    while(!(*(*exe>>*(--cur_s))&FLAG_INITIAL)&&cur_s>limit);
    if(begin_s>=cur_s&&**exe&FLAG_INITIAL) return begin_s;
    else if(begin_s==cur_s){
        *exe>>'\0';
        if(**exe&FLAG_INITIAL) return begin_s;
        else return nullptr;
    }else return cur_s;
};
void RRegex::LazyIterator::operator++(){
    if(++cur>=matches.size()){
        Match m;
        cur_s=matches[matches.size()-1].end;
        m.end=(*this)<<matches[matches.size()-1].end;
        m.start=nullptr;
        matches.push_back(m);
        //std::cout<<"end: "<<m.end-begin_s;
    }
};
std::string RRegex::LazyIterator::operator*(){
    if(!matches[cur].start){
        exe->reset();
        cur_s=matches[cur].end;
        matches[cur].start=(*this)>>matches[cur].end;
        //std::cout<<"start: "<<matches[cur].start-begin_s;
    }
    return matches[cur].str();
};
