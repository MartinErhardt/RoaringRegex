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
    //auto start_time = std::chrono::high_resolution_clock::now();
    RRegex r(pattern);
    
    r.exec->print();
    
    auto start_time = std::chrono::high_resolution_clock::now();
    bool is_match = r.is_match(text);
    auto end_time = std::chrono::high_resolution_clock::now();
    std::cout << "is match? " << is_match << std::endl;
    std::cout<<"time: " <<std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count() << "ms\n";
    
    free(text);
    free(pattern);
    return 0;
}
