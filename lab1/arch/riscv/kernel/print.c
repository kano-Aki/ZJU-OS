#include "defs.h"
extern struct sbiret sbi_call(uint64_t ext, uint64_t fid, uint64_t arg0,
                              uint64_t arg1, uint64_t arg2, uint64_t arg3,
                              uint64_t arg4, uint64_t arg5);

int puts(char *str) {
  // TODO
  uint64_t asc;
  while(*str!='\0')
  {
    asc=*str;
    sbi_call(1,0,*str,0,0,0,0,0);
    str++;
  }
  return 0;
}
void num_to_str(uint64_t n,char* str,int num_bit)
{
  char tmp[num_bit];
  int i=0;
  tmp[i]='0';
  while(n>0)
  {
    tmp[i++]=n%10+48;//48为0的ASCII码
    n/=10;
  }
  for(int j=num_bit-1;j>=0;j--) *(str+num_bit-1-j)=tmp[j];
}

int put_num(uint64_t n) {
  // TODO
  int temp=1,num_bit=0;
  if(n==0) num_bit=1;
  while(temp<=n)//求出数字有几位
  {
    temp*=10;
    num_bit++;
  }
  char str[num_bit];
  num_to_str(n,str,num_bit);
  puts(str);
  return 0;
}