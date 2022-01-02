/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#include"regex.h"
#include<chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <cstdlib>
#include <ostream>

using namespace Regex;
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
