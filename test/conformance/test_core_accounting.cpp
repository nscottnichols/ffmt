#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <ffmt/detail/checked.hpp>
#include <limits>
#include <string>

int main() {
  char bytes[4] = {'!', '!', '!', '!'};
  ffmt::detail::iterator_sink<char *, char> sink{bytes + 1, 2};
  ffmt::detail::sink_iterator out{&sink};
  auto copy = out;
  *out++ = 'a';
  *copy++ = 'b';
  copy.repeat('c', 5);
  assert(sink.required == 7 && sink.written == 2);
  assert(bytes[0] == '!' && bytes[1] == 'a' && bytes[2] == 'b' &&
         bytes[3] == '!');
  ffmt::detail::iterator_sink<char *, char> count{nullptr, 0};
  count.repeat('x', std::numeric_limits<std::size_t>::max());
  count.push('x');
  assert(count.error == ffmt::format_error::output_overflow &&
         count.written == 0);
}
