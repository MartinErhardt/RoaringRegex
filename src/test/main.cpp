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
    
    
    auto start_time = std::chrono::high_resolution_clock::now();
    auto acceptance_iter = r.get_acceptance_iter(text)++;
    bool is_match = (*acceptance_iter).has_value();
    auto end_time = std::chrono::high_resolution_clock::now();
    acceptance_iter.print();
    std::cout << "is match? " << is_match << std::endl;
    std::cout<<"time: " <<std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count() << "ms\n";
    free(text);
    free(pattern);
    return 0;
}
