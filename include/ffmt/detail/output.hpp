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
  // Family adapters can align a single-pass body in a bounded pointer buffer.
  // These hooks are absent for transports that cannot reorder prior output.
  FMT_HD auto ffmt_padding_mark() const
    requires requires(Sink &s) { s.ffmt_padding_mark(); }
  {
    return sink->ffmt_padding_mark();
  }
  template <typename Mark>
  FMT_HD void ffmt_apply_padding(Mark mark, std::size_t body_units,
                                 std::size_t left,
                                 const typename Sink::char_type *fill,
                                 std::size_t fill_size) const
    requires requires(Sink &s) {
      s.ffmt_apply_padding(mark, body_units, left, fill, fill_size);
    }
  {
    sink->ffmt_apply_padding(mark, body_units, left, fill, fill_size);
  }
  FMT_HD auto &ffmt_base_out() const
    requires requires(Sink &s) {
      s.out;
      requires(!std::is_pointer_v<decltype(s.out)>);
    }
  {
    return sink->out;
  }
};
template <typename OutputIt, typename Char> struct iterator_sink {
  using char_type = Char;
  OutputIt out;
  std::size_t limit, written = 0, required = 0;
  format_error error = format_error::none;
  struct padding_mark {
    OutputIt out;
    std::size_t written, required;
  };
  FMT_HD auto ffmt_padding_mark() const -> padding_mark
    requires std::is_same_v<OutputIt, Char *>
  {
    return {out, written, required};
  }
  // The body and all padding have already passed through push/repeat, so
  // counters and outer forwarding iterators are correct. Only reorder the
  // retained prefix; output beyond the caller's bound needs no storage.
  FMT_HD void ffmt_apply_padding(padding_mark mark, std::size_t body_units,
                                 std::size_t left, const Char *fill,
                                 std::size_t fill_size)
    requires std::is_same_v<OutputIt, Char *>
  {
    if (error != format_error::none || !left)
      return;
    if (!fill_size ||
        left > std::numeric_limits<std::size_t>::max() / fill_size) {
      error = format_error::output_overflow;
      return;
    }
    auto left_units = left * fill_size;
    auto available = limit - mark.written;
    auto keep = available > left_units ? available - left_units : 0;
    if (keep > body_units)
      keep = body_units;
    for (std::size_t i = keep; i > 0; --i)
      mark.out[left_units + i - 1] = mark.out[i - 1];
    auto retained_fill = left_units < available ? left_units : available;
    for (std::size_t i = 0; i < retained_fill; ++i)
      mark.out[i] = fill[i % fill_size];
  }
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
