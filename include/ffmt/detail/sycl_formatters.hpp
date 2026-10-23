#pragma once
#if defined(SYCL_LANGUAGE_VERSION) || FFMT_COMPILER_ACPP

namespace ffmt {
namespace detail {

// Build "{}", "{}x{}", "{}x{}x{}", ... with a chosen separator.
template <int N, char Sep> consteval auto make_sep_fmt() {
  static_assert(N >= 1, "make_sep_fmt requires N >= 1");
  // Output size: N copies of "{}" + (N-1) separators + '\0'
  constexpr size_t Sz = static_cast<size_t>(N) * 2 + (N - 1) + 1;
  fixed_string<Sz> r{};
  size_t p = 0;
  for (int i = 0; i < N; i++) {
    if (i > 0)
      r.data[p++] = Sep;
    r.data[p++] = '{';
    r.data[p++] = '}';
  }
  r.data[p] = '\0';
  return r;
}

// Build "({})", "({}, {})", "({}, {}, {})", ...
template <int N> consteval auto make_id_fmt() {
  static_assert(N >= 1, "make_id_fmt requires N >= 1");
  // "(" + N×"{}" + (N-1)×", " + ")" + '\0'
  constexpr size_t Sz = 1 + static_cast<size_t>(N) * 2 + (N - 1) * 2 + 1 + 1;
  fixed_string<Sz> r{};
  size_t p = 0;
  r.data[p++] = '(';
  for (int i = 0; i < N; i++) {
    if (i > 0) {
      r.data[p++] = ',';
      r.data[p++] = ' ';
    }
    r.data[p++] = '{';
    r.data[p++] = '}';
  }
  r.data[p++] = ')';
  r.data[p] = '\0';
  return r;
}

} // namespace detail

// sycl::range<N> -> "AxBxC"
template <int N> struct formatter<::sycl::range<N>> {
  static constexpr auto format(::sycl::range<N> r) {
    return [&]<size_t... Is>(std::index_sequence<Is...>) {
      return formatted<detail::make_sep_fmt<N, 'x'>(),
                       std::decay_t<decltype(r[Is])>...>{{r[Is]...}};
    }(std::make_index_sequence<N>{});
  }
};

// sycl::id<N> -> "(A, B, C)"
template <int N> struct formatter<::sycl::id<N>> {
  static constexpr auto format(::sycl::id<N> id) {
    return [&]<size_t... Is>(std::index_sequence<Is...>) {
      return formatted<detail::make_id_fmt<N>(),
                       std::decay_t<decltype(id[Is])>...>{{id[Is]...}};
    }(std::make_index_sequence<N>{});
  }
};

// sycl::item<N, WithOffset> -> "item(global=(...), range=...)" — recursively
// resolved through the formatters for sycl::id and sycl::range. The second
// template parameter exists in both DPC++ and ACPP; a single specialization
// covers both backends and both WithOffset values.
template <int N, bool WithOffset>
struct formatter<::sycl::item<N, WithOffset>> {
  static constexpr auto format(::sycl::item<N, WithOffset> it) {
    return formatted<detail::fixed_string{"item(global={}, range={})"},
                     ::sycl::id<N>, ::sycl::range<N>>{
        {it.get_id(), it.get_range()}};
  }
};

// sycl::nd_item<N> -> "nd_item(global=..., local=..., range=...)"
template <int N> struct formatter<::sycl::nd_item<N>> {
  static constexpr auto format(::sycl::nd_item<N> nd) {
    return formatted<detail::fixed_string{
                         "nd_item(global={}, local={}, range={})"},
                     ::sycl::id<N>, ::sycl::id<N>, ::sycl::range<N>>{
        {nd.get_global_id(), nd.get_local_id(), nd.get_global_range()}};
  }
};

} // namespace ffmt

#endif // SYCL_LANGUAGE_VERSION || ACPP
