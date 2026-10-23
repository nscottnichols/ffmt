#pragma once
namespace ffmt::detail {
// An output iterator whose copies share a sink. It permits nested formatter
// calls to count bytes exactly without allocating or losing the outer count.
template <typename Sink> struct sink_iterator {
  using iterator_category = std::output_iterator_tag;
  using value_type = void;
  using difference_type = std::ptrdiff_t;
  using pointer = void;
  using reference = void;
  Sink *sink;
  FMT_HD auto operator*() const -> sink_iterator { return *this; }
  FMT_HD auto operator++() -> sink_iterator & { return *this; }
  FMT_HD auto operator++(int) -> sink_iterator { return *this; }
  FMT_HD auto operator=(typename Sink::char_type c) -> sink_iterator & {
    sink->push(c);
    return *this;
  }
  FMT_HD void repeat(typename Sink::char_type c, std::size_t n) {
    sink->repeat(c, n);
  }

};
template <typename OutputIt, typename Char> struct iterator_sink {
  using char_type = Char;
  OutputIt out;
  std::size_t limit, written = 0, required = 0;
  format_error error = format_error::none;
  FMT_HD void push(Char c) {
    if (required == std::numeric_limits<std::size_t>::max()) {
      error = format_error::output_overflow;
      return;
    }
    ++required;
    if (written < limit) {
      *out++ = c;
      ++written;
    }
  }
  FMT_HD void repeat(Char c, std::size_t count) {
    if (count > std::numeric_limits<std::size_t>::max() - required) {
      error = format_error::output_overflow;
      return;
    }
    required += count;
    auto amount = count < limit - written ? count : limit - written;
    for (std::size_t i = 0; i < amount; ++i)
      *out++ = c;
    written += amount;
  }
};


}
