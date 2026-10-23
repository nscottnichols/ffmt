#pragma once
namespace ffmt {
template <typename OutputIt, typename Char, typename... Args>
FMT_HD auto format_to_n(OutputIt out, std::size_t size,
                        runtime_format_string<Char> pattern, Args &&...args)
    -> format_to_result<OutputIt> {
  detail::iterator_sink<OutputIt, Char> sink{out, size};
  detail::run_format(sink, pattern.str, static_cast<Args &&>(args)...);
  return {{sink.written, sink.required, sink.error}, sink.out};
}
template <typename OutputIt, typename... Args>
FMT_HD auto format_to_n(OutputIt out, std::size_t size,
                        format_string<Args...> pattern, Args &&...args)
    -> format_to_result<OutputIt> {
  return format_to_n(out, size, runtime(pattern.str),
                     static_cast<Args &&>(args)...);
}
template <typename OutputIt, typename... Args>
FMT_HD auto format_to_n(OutputIt out, std::size_t size,
                        wformat_string<Args...> pattern, Args &&...args)
    -> format_to_result<OutputIt> {
  return format_to_n(out, size, runtime(pattern.str),
                     static_cast<Args &&>(args)...);
}
template <typename OutputIt, typename Char, typename... Args>
FMT_HD auto format_to(OutputIt out, runtime_format_string<Char> pattern,
                      Args &&...args) -> format_to_result<OutputIt> {
  return format_to_n(out, std::numeric_limits<std::size_t>::max(), pattern,
                     static_cast<Args &&>(args)...);
}
template <typename OutputIt, typename... Args>
FMT_HD auto format_to(OutputIt out, format_string<Args...> pattern,
                      Args &&...args) -> format_to_result<OutputIt> {
  return format_to(out, runtime(pattern.str), static_cast<Args &&>(args)...);
}
template <typename OutputIt, typename... Args>
FMT_HD auto format_to(OutputIt out, wformat_string<Args...> pattern,
                      Args &&...args) -> format_to_result<OutputIt> {
  return format_to(out, runtime(pattern.str), static_cast<Args &&>(args)...);
}
template <typename Char, typename... Args>
FMT_HD auto formatted_size(runtime_format_string<Char> pattern, Args &&...args)
    -> format_result {
  // A null destination is safe at zero bound; no null pointer arithmetic
  // occurs.
  return format_to_n(static_cast<Char *>(nullptr), 0, pattern,
                     static_cast<Args &&>(args)...);
}
template <typename... Args>
FMT_HD auto formatted_size(format_string<Args...> pattern, Args &&...args)
    -> format_result {
  return formatted_size(runtime(pattern.str), static_cast<Args &&>(args)...);
}
template <typename... Args>
FMT_HD auto formatted_size(wformat_string<Args...> pattern, Args &&...args)
    -> format_result {
  return formatted_size(runtime(pattern.str), static_cast<Args &&>(args)...);
}

}
