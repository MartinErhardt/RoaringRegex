/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#include<stdio.h>
#include <stdbool.h>
#include <string.h>
long long nfa[64][27];
bool isMatch(char * s, char * p){
    long long new_s,states,j,s2,b=0;                                              //bits encode reachable states in the various sub-automata        
    int lp=strlen(p);
    int ls=strlen(s);
    memset(nfa, 0, sizeof(nfa[0][0])*64*27);
    for(int i=0;i<lp;b++,i++)                                                         //create eps-NFA
        if(p[i]!='*'&&p[i+1]=='*')for(char c=0;c<27;c++)nfa[b][c]=(c+'a'-1==p[i]||p[i]=='.'||!c)?nfa[b][c]|1llu<<(b+(c==0))  :nfa[b][c];
        else if(p[i]!='*'&&p[i+1]!='*')for(char c=1;c<27;c++)  nfa[b][c]    =(c+'a'-1==p[i]||p[i]=='.')?nfa[b][c]|1llu<<(b+1):nfa[b][c]; 
        else b--;
    long long final_s=(1<<b);                                                     //final states
    for(long long ch=1;ch;) for(j=0,ch=0;j<b;j++) for(int i=j;i<=b;i++) if((nfa[j][0]>>i)&1) for(char c=0;c<27;c++){ 
        ch|=(!nfa[j][c])&nfa[i][c];
        nfa[j][c]|=nfa[i][c];                                                     //remove eps-transitions
        if(i==b) final_s|=(1llu<<j);
    }
    states=(nfa[0][0]|1);                                                         //all states reachable from eps transition are initial
    for(int i=0;i<ls;states=new_s,new_s=0,i++)for(s2=states,j=0;s2;j++,s2>>=1llu) new_s=(s2&1llu)?new_s|nfa[j][s[i]-'a'+1]:new_s;
    return states&final_s;
}
int main(){
    char* text=NULL;
    char* pattern=NULL;
    size_t len;
    if (getline(&text, &len, stdin) == -1||getline(&pattern, &len, stdin)==-1)
            return -1;
    text[strlen(text)-1]='\0';
    pattern[strlen(pattern)-1]='\0';
    if(isMatch(text,pattern)) printf("Text matches string!\n");
    else printf("no match!\n");
    return 0;
}
