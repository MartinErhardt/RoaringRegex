
char* operator<<(char* s){
//                 std::cout<<"waiit: "<<s;
    if(!begin_s){
        begin_s=s;
        cur_s_fwd=begin_s;
        //std::cout<<"test1";
        *this<<'\0';
//                     std::cout<<"test2";
    }
    //uint8_t ret;
    while(!((*(*this<<*(cur_s_fwd++)))&FLAG_ACCEPTING)&&*(cur_s_fwd-1));// std::cout<<"welp: "<<(int)(ret&FLAG_ACCEPTING)<<std::endl;
    //std::cout<<"val: "<<cur_s_fwd-begin_s<<std::endl;
    if(!(*(--cur_s_fwd))&&**this&FLAG_ACCEPTING){
        char* intermediate=cur_s_fwd;
        cur_s_fwd=cur_s_bwd=nullptr;
        //std::cout<<intermediate-1<<std::endl;
        return intermediate-1; //minus null character
    }else if(!*cur_s_fwd){
        cur_s_fwd=cur_s_bwd=nullptr;
        return nullptr;
    }else{
        cur_s_bwd=cur_s_fwd;
        //std::cout<<cur_s_bwd<<std::endl;
        return cur_s_fwd;
    }
};
char* operator>>(char* s){
    if(cur_s_bwd==cur_s_fwd) reset();
    while(!(*(*this>>*(cur_s_bwd--))&FLAG_INITIAL)&&cur_s_bwd+1>begin_s);
    if(begin_s==++cur_s_bwd&&**this&FLAG_INITIAL){
        cur_s_bwd=cur_s_fwd;
        return begin_s;
    }else if(begin_s==cur_s_bwd){
        cur_s_bwd=cur_s_fwd;
        *this>>'\0';
        if(**this&FLAG_INITIAL) return begin_s;
        else return nullptr;
    }else return cur_s_bwd;
};
