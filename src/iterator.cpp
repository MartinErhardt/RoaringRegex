/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#include"regex.h"
using namespace Regex;
char* RRegex::iterator::operator<<(char* s){
    if(!*cur_s) return nullptr;
    while(!((*(*(r->exec)<<*(cur_s++)))&FLAG_ACCEPTING)&&*cur_s);// std::cout<<"welp: "<<(int)(ret&FLAG_ACCEPTING)<<std::endl;
    if(!(*cur_s)&&**(r->exec)&FLAG_ACCEPTING){
        return cur_s; //minus null character
    }else if(!*cur_s){
        *(r->exec)<<'\0';
        if(**(r->exec)&FLAG_ACCEPTING) return --cur_s;
        else return nullptr;
    }else{
        return cur_s;
    }
};
char* RRegex::iterator::operator>>(char* s){
    while(!(*(*(r->exec)>>*(cur_s--))&FLAG_INITIAL)&&cur_s>begin_s);
    if(begin_s==++cur_s&&**(r->exec)&FLAG_INITIAL){
        return begin_s;
    }else if(begin_s==cur_s){
        *(r->exec)>>'\0';
        if(**(r->exec)&FLAG_INITIAL) return begin_s;
        else return nullptr;
    }else return cur_s;
};
