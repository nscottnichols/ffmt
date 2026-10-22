// Byte-counted stdout transport. Format bytes are never reinterpreted as a
// printf format string, and embedded NULs are preserved on capable backends.
#pragma once
#if defined(__INTEL_LLVM_COMPILER) && defined(_OPENMP) &&                      \
    !defined(SYCL_LANGUAGE_VERSION) && !FFMT_COMPILER_ACPP
#if defined(__SPIR__) || defined(__SPIRV__)
template <typename... Args>
extern int __spirv_ocl_printf(const __attribute__((opencl_constant)) char *,
                              Args...);
#define FFMT_DETAIL_PRINTF(...) __spirv_ocl_printf(__VA_ARGS__)
#else
#define FFMT_DETAIL_PRINTF(...) ::printf(__VA_ARGS__)
#endif
#elif defined(FFMT_EMIT_PRINTF)
#define FFMT_DETAIL_PRINTF(...) FFMT_EMIT_PRINTF(__VA_ARGS__)
#elif defined(SYCL_LANGUAGE_VERSION) && !FFMT_COMPILER_ACPP
#define FFMT_DETAIL_PRINTF(...)                                                \
  ::sycl::ext::oneapi::experimental::printf(__VA_ARGS__)
#else
#define FFMT_DETAIL_PRINTF(...) ::printf(__VA_ARGS__)
#endif
// Intel SPIR-V printf currently drops NUL even for a literal %c conversion.
// Enable only after a transport probe demonstrates binary-safe emission.
#ifndef FFMT_SPIRV_NUL_SUPPORTED
#define FFMT_SPIRV_NUL_SUPPORTED 0
#endif
namespace ffmt::detail {
#if (defined(__SPIR__) || defined(__SPIRV__)) && !defined(__HIP_PLATFORM_SPIRV__)
#ifdef _OPENMP
#pragma omp declare target
#endif
inline constexpr __attribute__((opencl_constant)) char emit_char_format[] =
    "%c";
inline constexpr __attribute__((opencl_constant)) char emit_eight_format[] =
    "%c%c%c%c%c%c%c%c";
#ifdef _OPENMP
#pragma omp end declare target
#endif
#define FFMT_DETAIL_CHAR_FORMAT ::ffmt::detail::emit_char_format
#define FFMT_DETAIL_EIGHT_FORMAT ::ffmt::detail::emit_eight_format
#else
#define FFMT_DETAIL_CHAR_FORMAT "%c"
#define FFMT_DETAIL_EIGHT_FORMAT "%c%c%c%c%c%c%c%c"
#endif
FMT_HD inline bool emit_chunk(const char *bytes, std::size_t size) {
#if defined(FFMT_EMIT_CHUNK)
  FFMT_EMIT_CHUNK(bytes, size);
  return true;
#elif defined(FFMT_EMIT_BUFFER)
  // Preserve the original hook signature. Half-sized raw pieces leave room
  // for the legacy percent-escaping helper to expand every byte safely.
  for (std::size_t i = 0; i < size; ++i)
    if (bytes[i] == '\0' || (FFMT_BUFFER_SIZE == 1 && bytes[i] == '%'))
      return false;
  std::size_t position = 0;
  constexpr std::size_t piece_size =
      FFMT_BUFFER_SIZE > 1 ? FFMT_BUFFER_SIZE / 2 : 1;
  while (position < size) {
    fmt_buf legacy;
    auto count = size - position < piece_size ? size - position : piece_size;
    for (std::size_t i = 0; i < count; ++i)
      legacy.data[i] = bytes[position + i];
    legacy.len = static_cast<int>(count);
    legacy.data[count] = '\0';
    FFMT_EMIT_BUFFER(legacy, true);
    position += count;
  }
  return true;
#elif FFMT_COMPILER_ACPP
  bool success = true;
  __acpp_if_target_sscp(
      ::sycl::AdaptiveCpp_jit::compile_if_else(
          ::sycl::AdaptiveCpp_jit::reflect<
              ::sycl::AdaptiveCpp_jit::reflection_query::compiler_backend>() ==
              ::sycl::AdaptiveCpp_jit::compiler_backend::host,
          [&]() { success = ::fwrite(bytes, 1, size, stdout) == size; },
          [&]() {
            char escaped[2 * FFMT_BUFFER_SIZE + 1];
            std::size_t count = 0;
            for (std::size_t i = 0; i < size; ++i) {
              if (bytes[i] == '\0') {
                success = false;
                return;
              }
              escaped[count++] = bytes[i];
              if (bytes[i] == '%')
                escaped[count++] = '%';
            }
            escaped[count] = '\0';
            __acpp_sscp_print(escaped);
          });) return success;
#elif defined(__CUDA_ARCH__) || defined(__HIP_DEVICE_COMPILE__) ||             \
    defined(__SPIR__) || defined(__SPIRV__) || defined(__OPENMP_TARGET__) ||   \
    defined(__SYCL_DEVICE_ONLY__)
#if (defined(__SPIR__) || defined(__SPIRV__)) && !FFMT_SPIRV_NUL_SUPPORTED
  for (std::size_t i = 0; i < size; ++i)
    if (bytes[i] == '\0')
      return false;
#endif
  std::size_t i = 0;
  for (; i + 8 <= size; i += 8)
    if (FFMT_DETAIL_PRINTF(FFMT_DETAIL_EIGHT_FORMAT,
                           int(static_cast<unsigned char>(bytes[i])),
                           int(static_cast<unsigned char>(bytes[i + 1])),
                           int(static_cast<unsigned char>(bytes[i + 2])),
                           int(static_cast<unsigned char>(bytes[i + 3])),
                           int(static_cast<unsigned char>(bytes[i + 4])),
                           int(static_cast<unsigned char>(bytes[i + 5])),
                           int(static_cast<unsigned char>(bytes[i + 6])),
                           int(static_cast<unsigned char>(bytes[i + 7]))) < 0)
      return false;
  for (; i < size; ++i)
    if (FFMT_DETAIL_PRINTF(FFMT_DETAIL_CHAR_FORMAT,
                           int(static_cast<unsigned char>(bytes[i]))) < 0)
      return false;
  return true;
#elif defined(_OPENMP) && (defined(__NVPTX__) || defined(__AMDGCN__))
  for (std::size_t i = 0; i < size; ++i)
    if (FFMT_DETAIL_PRINTF(FFMT_DETAIL_CHAR_FORMAT,
                           int(static_cast<unsigned char>(bytes[i]))) < 0)
      return false;
  return true;
#else
  return ::fwrite(bytes, 1, size, stdout) == size;
#endif
}

static_assert(FFMT_BUFFER_SIZE > 0, "FFMT_BUFFER_SIZE must be positive");
template <typename Char> struct stdout_sink {
  using char_type = Char;
  char data[FFMT_BUFFER_SIZE];
  std::size_t used = 0, written = 0, required = 0;
  format_error error = format_error::none;
  std::uint32_t pending_surrogate = 0;
  FMT_HD void flush() {
    if (used) {
      if (emit_chunk(data, used))
        written += used;
      else
        error = format_error::transport_error;
      used = 0;
    }
  }
  FMT_HD void byte(char c) {
    data[used++] = c;
    if (used == FFMT_BUFFER_SIZE)
      flush();
  }
  FMT_HD void push(Char c) {
    if (error != format_error::none)
      return;
    if constexpr (sizeof(Char) == 1) {
      ++required;
      byte(static_cast<char>(c));
    } else {
      auto cp = static_cast<std::uint32_t>(c);
      if constexpr (sizeof(Char) == 2) {
        if (pending_surrogate) {
          if (cp < 0xdc00 || cp > 0xdfff) {
            error = format_error::invalid_code_point;
            return;
          }
          cp = 0x10000 + ((pending_surrogate - 0xd800) << 10) + (cp - 0xdc00);
          pending_surrogate = 0;
        } else if (cp >= 0xd800 && cp <= 0xdbff) {
          pending_surrogate = cp;
          return;
        }
      }
      if (cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) {
        error = format_error::invalid_code_point;
        return;
      }
      if (cp < 0x80) {
        ++required;
        byte(char(cp));
      } else if (cp < 0x800) {
        required += 2;
        byte(char(0xc0 | (cp >> 6)));
        byte(char(0x80 | (cp & 0x3f)));
      } else if (cp < 0x10000) {
        required += 3;
        byte(char(0xe0 | (cp >> 12)));
        byte(char(0x80 | ((cp >> 6) & 0x3f)));
        byte(char(0x80 | (cp & 0x3f)));
      } else {
        required += 4;
        byte(char(0xf0 | (cp >> 18)));
        byte(char(0x80 | ((cp >> 12) & 0x3f)));
        byte(char(0x80 | ((cp >> 6) & 0x3f)));
        byte(char(0x80 | (cp & 0x3f)));
      }
    }
  }
  FMT_HD void repeat(Char c, std::size_t n) {
    for (std::size_t i = 0; i < n && error == format_error::none; ++i)
      push(c);
  }
  FMT_HD void finish() {
    if (pending_surrogate && error == format_error::none)
      error = format_error::invalid_code_point;
    flush();
  }
};
} // namespace ffmt::detail

namespace ffmt {
template <typename Char, typename... Args>
FMT_HD auto try_print(runtime_format_string<Char> pattern, Args &&...args)
    -> format_result {
  detail::stdout_sink<Char> sink;
  detail::run_format(sink, pattern.str, static_cast<Args &&>(args)...);
  sink.finish();
  return {sink.written, sink.required, sink.error};
}
template <typename Char, typename... Args>
FMT_HD auto try_println(runtime_format_string<Char> pattern, Args &&...args)
    -> format_result {
  detail::stdout_sink<Char> sink;
  detail::run_format(sink, pattern.str, static_cast<Args &&>(args)...);
  if (sink.error == format_error::none)
    sink.push(Char('\n'));
  sink.finish();
  return {sink.written, sink.required, sink.error};
}
template <typename... Args>
FMT_HD auto try_print(format_string<Args...> pattern, Args &&...args)
    -> format_result {
  return try_print(runtime(pattern.str), static_cast<Args &&>(args)...);
}
template <typename... Args>
FMT_HD auto try_print(wformat_string<Args...> pattern, Args &&...args)
    -> format_result {
  return try_print(runtime(pattern.str), static_cast<Args &&>(args)...);
}
template <typename... Args>
FMT_HD auto try_println(format_string<Args...> pattern, Args &&...args)
    -> format_result {
  return try_println(runtime(pattern.str), static_cast<Args &&>(args)...);
}
template <typename... Args>
FMT_HD auto try_println(wformat_string<Args...> pattern, Args &&...args)
    -> format_result {
  return try_println(runtime(pattern.str), static_cast<Args &&>(args)...);
}
template <typename Char, typename... Args>
FMT_HD void print(runtime_format_string<Char> pattern, Args &&...args) {
  (void)try_print(pattern, static_cast<Args &&>(args)...);
}
template <typename Char, typename... Args>
FMT_HD void println(runtime_format_string<Char> pattern, Args &&...args) {
  (void)try_println(pattern, static_cast<Args &&>(args)...);
}
template <typename... Args>
FMT_HD void print(format_string<Args...> pattern, Args &&...args) {
  (void)try_print(pattern, static_cast<Args &&>(args)...);
}
template <typename... Args>
FMT_HD void print(wformat_string<Args...> pattern, Args &&...args) {
  (void)try_print(pattern, static_cast<Args &&>(args)...);
}
template <typename... Args>
FMT_HD void println(format_string<Args...> pattern, Args &&...args) {
  (void)try_println(pattern, static_cast<Args &&>(args)...);
}
template <typename... Args>
FMT_HD void println(wformat_string<Args...> pattern, Args &&...args) {
  (void)try_println(pattern, static_cast<Args &&>(args)...);
}
template <detail::fixed_string Fmt, typename... Args>
FMT_HD auto try_print(Args &&...args) -> format_result {
  using Char = std::remove_cvref_t<decltype(Fmt.data[0])>;
  detail::validate_literal<Char, Args...>({Fmt.data, detail::flen(Fmt)});
  return try_print(
      runtime(basic_string_view<Char>{Fmt.data, detail::flen(Fmt)}),
      static_cast<Args &&>(args)...);
}
template <detail::fixed_string Fmt, typename... Args>
FMT_HD auto try_println(Args &&...args) -> format_result {
  using Char = std::remove_cvref_t<decltype(Fmt.data[0])>;
  detail::validate_literal<Char, Args...>({Fmt.data, detail::flen(Fmt)});
  return try_println(
      runtime(basic_string_view<Char>{Fmt.data, detail::flen(Fmt)}),
      static_cast<Args &&>(args)...);
}
template <detail::fixed_string Fmt, typename... Args>
FMT_HD void print(Args &&...args) {
  (void)try_print<Fmt>(static_cast<Args &&>(args)...);
}
template <detail::fixed_string Fmt, typename... Args>
FMT_HD void println(Args &&...args) {
  (void)try_println<Fmt>(static_cast<Args &&>(args)...);
}
} // namespace ffmt
#undef FFMT_DETAIL_PRINTF

#undef FFMT_DETAIL_CHAR_FORMAT
#undef FFMT_DETAIL_EIGHT_FORMAT
