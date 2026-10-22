#include <omp.h>
#define HD 


#include <cstdint>
#include <cstdio>
#include <vector>
#include <exception>
#include <string_view>
namespace probe {
struct options {
  char fill[4]={' '}; unsigned fill_size=1;
  char alignment=0,sign=0,type=0;bool alternate=false,zero=false;
  int width=0,precision=-1,width_index=-1,precision_index=-1;
  std::string_view width_name{},precision_name{};
};
struct sink {
  char* out;std::size_t limit,written=0,required=0;unsigned error=0;
  void push(char c){++required;if(written<limit){*out++=c;++written;}}
  void repeat(char c,std::size_t n){required+=n;auto amount=n<limit-written?n:limit-written;for(std::size_t i=0;i<amount;++i)*out++=c;written+=amount;}
};
struct iterator {
  sink* destination;
  iterator operator*()const{return*this;}
  iterator&operator++(){return*this;}
  iterator operator++(int){return*this;}
  iterator&operator=(char c){destination->push(c);return*this;}
  void repeat(char c,std::size_t n){destination->repeat(c,n);}
};
struct context {
  iterator current;unsigned&status;
  iterator out()const{return current;}
  void advance_to(iterator i){current=i;}
  unsigned error()const{return status;}
  int lookup(int){status=4;return 0;}
  int lookup(std::string_view){status=4;return 0;}
};
template<class C>void put(C&ctx,char c){auto it=ctx.out();*it++=c;ctx.advance_to(it);}
template<class C>void repeat(C&ctx,char c,std::size_t n){auto it=ctx.out();it.repeat(c,n);ctx.advance_to(it);}
template<class C>void append(C&ctx,const char*data,std::size_t n){for(std::size_t i=0;i<n;++i)put(ctx,data[i]);}
template<class C>options normalized(C&ctx,options s){
 if(s.width_index>=0)s.width=ctx.lookup(s.width_index);
 if(!s.width_name.empty())s.width=ctx.lookup(s.width_name);
 if(s.precision_index>=0)s.precision=ctx.lookup(s.precision_index);
 if(!s.precision_name.empty())s.precision=ctx.lookup(s.precision_name);
 return s;
}
template<class C,class Body>void outer(C&ctx,options spec,std::size_t columns,Body body,char default_align='<'){
 auto s=normalized(ctx,spec);if(ctx.error())return;
 std::size_t padding=s.width>0&&std::size_t(s.width)>columns?std::size_t(s.width)-columns:0;
 char align=s.alignment?s.alignment:default_align;
 std::size_t left=align=='>'?padding:align=='^'?padding/2:0;
 auto fill=[&](std::size_t n){if(s.fill_size==1)repeat(ctx,s.fill[0],n);else for(std::size_t i=0;i<n;++i)append(ctx,s.fill,s.fill_size);};
 fill(left);body(ctx);fill(padding-left);
}
template<class C,class Body>void invoke(C&ctx,options s,std::size_t digits,char sign,const char*prefix,std::size_t prefix_size,bool finite,Body body){
 s=normalized(ctx,s);if(ctx.error())return;
 std::size_t size=digits+prefix_size+(sign?1:0);
 std::size_t zeros=s.zero&&!s.alignment&&finite&&s.width>0&&std::size_t(s.width)>size?std::size_t(s.width)-size:0;
 outer(ctx,s,size+zeros,[&](auto&out){if(sign)put(out,sign);append(out,prefix,prefix_size);repeat(out,'0',zeros);body(out);},'>');
}
}

struct Input {std::uint64_t bits; unsigned kind;};
struct Output {unsigned written=0, required=0, error=0; char text[768]{};};
template<class T,class U>HD T from_bits(U bits){return __builtin_bit_cast(T,bits);}
template<class T>HD void format_record_value(T value,Output&out){probe::sink sink{out.text,sizeof(out.text)};probe::context ctx{{&sink},sink.error};probe::invoke(ctx,probe::options{},1,0,"",0,true,[](auto&ctx){probe::put(ctx,'7');});out.written=unsigned(sink.written);out.required=unsigned(sink.required);out.error=sink.error;}
HD void format_record(Input in,Output& out){
  if(in.kind==32)format_record_value(from_bits<float>(std::uint32_t(in.bits)),out);
  else format_record_value(from_bits<double>(in.bits),out);
}
std::vector<Input> inputs(){
  std::vector<Input> values;
  const std::uint64_t special64[]={0,0x8000000000000000ULL,1,0x000fffffffffffffULL,
    0x0010000000000000ULL,0x3ff0000000000000ULL,0x7fefffffffffffffULL,
    0x7ff0000000000000ULL,0xfff0000000000000ULL,0x7ff8000000000001ULL,
    0xfff8000000000001ULL,0x7ff0000000000001ULL};
  const std::uint32_t special32[]={0,0x80000000U,1,0x007fffffU,0x00800000U,
    0x3f800000U,0x7f7fffffU,0x7f800000U,0xff800000U,0x7fc00001U,0xffc00001U,0x7f800001U};
  for(unsigned i=0;i<12;++i){values.push_back({special64[i],64});values.push_back({special32[i],32});}
  std::uint64_t state=0x4d595df4d0f33173ULL;
  for(unsigned i=0;i<4096;++i){
    state^=state<<13;state^=state>>7;state^=state<<17;
    values.push_back({state,64});values.push_back({std::uint32_t(state),32});
  }
  return values;
}
int main(){
  auto in=inputs();std::vector<Output> out(in.size());unsigned n=unsigned(in.size());
  Input* di=in.data();Output* dout=out.data();int initial=1;
  #pragma omp target map(from:initial)
  {initial=omp_is_initial_device();}
  if(initial)return 77;
  #pragma omp target teams distribute parallel for map(to:di[0:n]) map(from:dout[0:n])
  for(unsigned i=0;i<n;++i)format_record(di[i],dout[i]);
  for(unsigned i=0;i<n;++i)if(out[i].written!=1||out[i].required!=1||out[i].error!=0||out[i].text[0]!='7'){std::fprintf(stderr,"mismatch at %u\n",i);return 1;}std::puts("PASS8216");return 0;
}
