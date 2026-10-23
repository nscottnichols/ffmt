#pragma once
namespace ffmt::detail {
template <typename Context, typename Char>
FMT_HD void write_string(Context &ctx, basic_string_view<Char> value,
                         basic_format_spec<Char> spec, bool = false) {
  spec = resolved(ctx, spec);
  auto size = value.size();
  if (spec.precision >= 0 && size > static_cast<std::size_t>(spec.precision))
    size = spec.precision;
  write_padded(
      ctx, spec, size, [&](auto &out) { append(out, value.data(), size); },
      '<');
}
} // namespace ffmt::detail
