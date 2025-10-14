#include "caesar.h"

static char rot_alpha(char c, int k){
    if(c>='A' && c<='Z') return 'A'+(c-'A'+k)%26;
    if(c>='a' && c<='z') return 'a'+(c-'a'+k)%26;
    return c;
}
static char rot_digit(char c, int k){
    if(c>='0' && c<='9') return '0'+(c-'0'+(k%10))%10;
    return c;
}
void caesar_encrypt(const char *in, char *out, size_t n, uint8_t s){
    for(size_t i=0;i<n;i++){
        char c=in[i], d=rot_alpha(c,s);
        if(d==c) d=rot_digit(c,s);
        out[i]=d;
    }
}
void caesar_decrypt(const char *in, char *out, size_t n, uint8_t s){
    uint8_t s26=(26-(s%26))%26, s10=(10-(s%10))%10;
    for(size_t i=0;i<n;i++){
        char c=in[i], d=c;
        if(c>='A'&&c<='Z') d='A'+(c-'A'+s26)%26;
        else if(c>='a'&&c<='z') d='a'+(c-'a'+s26)%26;
        else if(c>='0'&&c<='9') d='0'+(c-'0'+s10)%10;
        out[i]=d;
    }
}
