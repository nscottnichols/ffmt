#pragma once
// Compatibility for instrumentation that used the former internal staging
// helper. Legacy emit hooks alone use this bounded compatibility fixture;
// the default transport uses byte-counted chunks.
namespace ffmt::detail {
struct fmt_buf {
  static constexpr int cap = FFMT_BUFFER_SIZE;
  char data[cap + 32]{};
  int len = 0;
};
namespace buffer_path {
FMT_HD inline void escape_percent_inplace(fmt_buf &out) {
  bool newline = out.len && out.data[out.len - 1] == '\n';
  int body = out.len - (newline ? 1 : 0), source = 0, target = 0;
  while (source < body) {
    int n = out.data[source] == '%' ? 2 : 1;
    if (target + n > fmt_buf::cap)
      break;
    target += n;
    ++source;
  }
  for (int i = source - 1, j = target - 1; i >= 0; --i) {
    char c = out.data[i];
    out.data[j--] = c;
    if (c == '%')
      out.data[j--] = '%';
  }
  out.len = target;
  if (newline)
    out.data[out.len++] = '\n';
}
} // namespace buffer_path
} // namespace ffmt::detail
