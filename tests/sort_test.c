#include "../mi_lib.c"
int main(void){
  const char *names[]={"A1","After","After EP - EP","After EP 2 - EP","Ania D\xc4\x85" "browska","Anna Dabrowska","Anna Wyszkoni","Anna Wyszkoni & Video","Baroque","Close Your Eyes","Cold","Cold - Single","Deep Diving","Deep Diving - Single","Dla Naiwnych Marzycieli","Dove","Dove - Single","Element","Element - Single","Ever","Field","Graham Epstein, Jeff Hazin","Here We Come","In a city called Garden",NULL};
  int secs[]={0,0,0,0,0,0,0,0,2,3,3,3,5,5,5,5,5,6,6,6,8,9,10,11};
  int i,bad=0;
  for(i=0;names[i+1];i++) if(coll_cmp(names[i],names[i+1])>=0){printf("ORDER BAD %s | %s\n",names[i],names[i+1]);bad++;}
  for(i=0;names[i];i++) if(name_section_of(names[i])!=secs[i]){printf("SEC BAD %s %d want %d\n",names[i],name_section_of(names[i]),secs[i]);bad++;}
  if(name_section_of("Pop")!=20||name_section_of("Summertime of Our Lives")!=23||name_section_of("300 dreams")!=34||name_section_of("\xd0\x9a\xd0\xb0\xd1\x82\xd1\x8f")!=34||name_section_of("\xc5\x81" "ukasz")!=15||name_section_of("\xc5\xbb" "ubr")!=33||name_section_of("Zebra")!=31||name_section_of("T")!=25) {printf("SEC2 BAD\n");bad++;}
  { unsigned char k[64]; int n=make_group_key("After",k,64); const unsigned char w[]={0x27,0x31,0x4d,0x2f,0x49}; if(n!=5||memcmp(k,w,5)){printf("KEY BAD\n");bad++;}
    n=make_group_key("Here We Come",k,64); const unsigned char w2[]={0x35,0x2f,0x49,0x2f,4,0x53,0x2f,4,0x2b,0x43,0x3f,0x2f}; if(n!=12||memcmp(k,w2,12)){printf("KEY2 BAD\n");bad++;} }
  if(coll_cmp("25 najcz","300 dreams")>=0||coll_cmp("300 dreams","\xd0\x9a")>=0||coll_cmp("Zebra","300 dreams")>=0){printf("OTHER BAD\n");bad++;}
  if(ms_to_samples(188616,48000)!=9053568LL){printf("SAMPLES %lld\n",ms_to_samples(188616,48000));bad++;}
  printf("bad=%d\n",bad);return bad;}
