"""Generated device-memory numeric corpus and binary record validation."""
import struct

MAGIC = 0x46464D54
SEED = 0x4D595DF4D0F33173


def parse_records(data):
    if len(data) < 8:
        raise ValueError("missing memory-suite header")
    magic, count = struct.unpack_from("<II", data)
    if magic != MAGIC:
        raise ValueError("incorrect memory-suite magic")
    offset, records = 8, []
    for _ in range(count):
        if offset + 24 > len(data):
            raise ValueError("missing or truncated memory record")
        kind, bits, written, required, error = struct.unpack_from("<IQIII", data, offset)
        offset += 24
        if written > 768 or offset + written > len(data):
            raise ValueError("invalid or truncated memory record payload")
        records.append(dict(kind=kind, bits=bits, written=written, required=required,
                            error=error, output=data[offset:offset + written]))
        offset += written
    if offset != len(data):
        raise ValueError("extra/duplicated bytes after memory records")
    return records


def compare_records(expected_data, actual_data):
    expected, actual = parse_records(expected_data), parse_records(actual_data)
    if len(expected) != len(actual):
        raise ValueError("missing or duplicated memory records")
    failures = []
    for i, (e, a) in enumerate(zip(expected, actual)):
        if e != a:
            failures.append(dict(index=i, kind=e["kind"], bits=hex(e["bits"]),
                                 expected=e["output"].hex(), actual=a["output"].hex(),
                                 expected_required=e["required"], required=a["required"], error=a["error"]))
    return dict(status="pass" if not failures else "mismatch", input_cases=len(expected),
                passed=len(expected) - len(failures), failed=len(failures),
                seed=hex(SEED), formats_per_input=5, failures=failures)


def source(route, oracle=False, samples=4096):
    kokkos = route.startswith("kokkos-") and not oracle
    cuda, hip = route in ("cuda", "nvcc", "kokkos-cuda"), route in ("hip", "chipstar", "kokkos-hip")
    sycl, omp = route in ("sycl", "acpp", "kokkos-sycl"), route.startswith("omp-")
    headers = "#include <cuda_runtime.h>\n" if cuda and not oracle else "#include <hip/hip_runtime.h>\n" if hip and not oracle else "#include <sycl/sycl.hpp>\n" if sycl and not oracle else "#include <omp.h>\n" if omp and not oracle else ""
    hd = "__host__ __device__" if (cuda or hip) and not oracle else ""
    text = headers + "#define HD " + hd + "\n"
    if kokkos:
        text += "#include <Kokkos_Core.hpp>\n"
    text += "#include <fmt/format.h>\n#define ORACLE 1\n" if oracle else "#include <ffmt/base.hpp>\n"
    text += r"""
#include <cstdint>
#include <cstdio>
#include <vector>
#include <exception>
struct Input {std::uint64_t bits; unsigned kind;};
struct Output {unsigned written=0, required=0, error=0; char text[768]{};};
template<class T,class U>HD T from_bits(U bits){return __builtin_bit_cast(T,bits);}
template<class T>HD void format_record_value(T value,Output& out){
#ifdef ORACLE
  auto s=fmt::format("{}|{:.6f}|{:.5e}|{:.17g}|{:#020.5a}",value,value,value,value,value);
  out.required=unsigned(s.size());out.written=out.required;
  if(out.written>sizeof(out.text)){out.error=1;out.written=sizeof(out.text);}
  for(unsigned i=0;i<out.written;++i)out.text[i]=s[i];
#else
  auto r=ffmt::format_to_n(out.text,sizeof(out.text),"{}|{:.6f}|{:.5e}|{:.17g}|{:#020.5a}",value,value,value,value,value);
  out.written=unsigned(r.written);out.required=unsigned(r.required);out.error=unsigned(r.error);
#endif
}
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
  for(unsigned i=0;i<SAMPLE_COUNT;++i){
    state^=state<<13;state^=state>>7;state^=state<<17;
    values.push_back({state,64});values.push_back({std::uint32_t(state),32});
  }
  return values;
}
void put32(std::uint32_t v){unsigned char b[4];for(int i=0;i<4;++i)b[i]=v>>(8*i);std::fwrite(b,1,4,stdout);}
void put64(std::uint64_t v){put32(std::uint32_t(v));put32(std::uint32_t(v>>32));}
void serialize(const std::vector<Input>& in,const std::vector<Output>& out){
  put32(0x46464d54U);put32(unsigned(in.size()));
  for(unsigned i=0;i<in.size();++i){
    put32(in[i].kind);put64(in[i].bits);put32(out[i].written);put32(out[i].required);put32(out[i].error);
    std::fwrite(out[i].text,1,out[i].written,stdout);
  }
}
""".replace("SAMPLE_COUNT", str(samples))
    if kokkos:
        expected_space = {"kokkos-serial": "Serial", "kokkos-cuda": "Cuda", "kokkos-hip": "HIP", "kokkos-sycl": "SYCL"}[route]
        return text + r'''
#include <cstring>
int main(int argc,char** argv){
  Kokkos::initialize(argc,argv);
  if(std::strcmp(Kokkos::DefaultExecutionSpace::name(),"EXPECTED_SPACE")!=0){Kokkos::finalize();return 77;}
  std::fprintf(stderr,"FFMT_DEVICE kokkos memory space=%s\n",Kokkos::DefaultExecutionSpace::name());
  {
    auto in=inputs();std::vector<Output> out(in.size());unsigned n=unsigned(in.size());
    Kokkos::View<Input*> din("input",n);Kokkos::View<Output*> dout("output",n);
    auto hin=Kokkos::create_mirror_view(din);
    for(unsigned i=0;i<n;++i)hin(i)=in[i];Kokkos::deep_copy(din,hin);
    Kokkos::parallel_for("ffmt-memory",Kokkos::RangePolicy<>(0,n),KOKKOS_LAMBDA(int i){format_record(din(i),dout(i));});
    auto hout=Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace{},dout);
    for(unsigned i=0;i<n;++i)out[i]=hout(i);serialize(in,out);
  }
  Kokkos::finalize();return 0;
}
'''.replace("EXPECTED_SPACE", expected_space)
    if oracle or not (cuda or hip or sycl or omp):
        return text + """
int main(){
  auto in=inputs();std::vector<Output> out(in.size());
  for(unsigned i=0;i<in.size();++i)format_record(in[i],out[i]);
  serialize(in,out);return 0;
}
"""
    if cuda or hip:
        prefix = "cuda" if cuda else "hip"
        return text + r"""
__global__ void memory_kernel(const Input* in,Output* out,unsigned n){
  unsigned i=blockIdx.x*blockDim.x+threadIdx.x;if(i<n)format_record(in[i],out[i]);
}
int main(){
  auto in=inputs();std::vector<Output> out(in.size());Input* di=nullptr;Output* dout=nullptr;
  if(PREFIXMalloc((void**)&di,in.size()*sizeof(Input))!=PREFIXSuccess)return 81;
  if(PREFIXMalloc((void**)&dout,out.size()*sizeof(Output))!=PREFIXSuccess)return 82;
  if(PREFIXMemcpy(di,in.data(),in.size()*sizeof(Input),PREFIXMemcpyHostToDevice)!=PREFIXSuccess)return 83;
  if(PREFIXMemset(dout,0,out.size()*sizeof(Output))!=PREFIXSuccess)return 84;
  LAUNCH;
  if(PREFIXGetLastError()!=PREFIXSuccess||PREFIXDeviceSynchronize()!=PREFIXSuccess)return 85;
  if(PREFIXMemcpy(out.data(),dout,out.size()*sizeof(Output),PREFIXMemcpyDeviceToHost)!=PREFIXSuccess)return 86;
  PREFIXFree(di);PREFIXFree(dout);serialize(in,out);return 0;
}
""".replace("PREFIX", prefix).replace("LAUNCH", "memory_kernel<<<(in.size()+63)/64,64>>>(di,dout,unsigned(in.size()))" if cuda else "hipLaunchKernelGGL(memory_kernel,dim3((in.size()+63)/64),dim3(64),0,0,di,dout,unsigned(in.size()))")
    if sycl:
        return text + r"""
int main(){
  try{
    auto in=inputs();std::vector<Output> out(in.size());sycl::queue q{sycl::gpu_selector_v};
    Input* di=sycl::malloc_device<Input>(in.size(),q);Output* dout=sycl::malloc_device<Output>(out.size(),q);
    if(!di||!dout)return 81;
    q.memcpy(di,in.data(),in.size()*sizeof(Input)).wait_and_throw();
    q.memset(dout,0,out.size()*sizeof(Output)).wait_and_throw();
    unsigned n=unsigned(in.size());
    q.parallel_for(sycl::range<1>(n),[=](sycl::id<1> id){format_record(di[id[0]],dout[id[0]]);}).wait_and_throw();
    q.memcpy(out.data(),dout,out.size()*sizeof(Output)).wait_and_throw();
    sycl::free(di,q);sycl::free(dout,q);serialize(in,out);return 0;
  }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 80;}
}
"""
    return text + r"""
int main(){
  auto in=inputs();std::vector<Output> out(in.size());unsigned n=unsigned(in.size());
  Input* di=in.data();Output* dout=out.data();int initial=1;
  #pragma omp target map(from:initial)
  {initial=omp_is_initial_device();}
  if(initial)return 77;
  #pragma omp target teams distribute parallel for map(to:di[0:n]) map(from:dout[0:n])
  for(unsigned i=0;i<n;++i)format_record(di[i],dout[i]);
  serialize(in,out);return 0;
}
"""
