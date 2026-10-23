// Calendar oracle wrappers differ between fmt and the C++20 standard library.
#include "family_test_support.hpp"
#include <ffmt/chrono.hpp>
#include <fmt/chrono.h>
namespace ffmt_test {
template <> struct oracle_argument<std::chrono::year> {
  static auto get(std::chrono::year value) { return fmt::year(int(value)); }
};
template <> struct oracle_argument<std::chrono::month> {
  static auto get(std::chrono::month value) {
    return fmt::month(unsigned(value));
  }
};
template <> struct oracle_argument<std::chrono::day> {
  static auto get(std::chrono::day value) { return fmt::day(unsigned(value)); }
};
template <> struct oracle_argument<std::chrono::weekday> {
  static auto get(std::chrono::weekday value) {
    return fmt::weekday(value.c_encoding());
  }
};
template <> struct oracle_argument<std::chrono::year_month_day> {
  static auto get(std::chrono::year_month_day value) {
    return fmt::year_month_day(fmt::year(int(value.year())),
                               fmt::month(unsigned(value.month())),
                               fmt::day(unsigned(value.day())));
  }
};
template <typename Duration>
struct oracle_argument<std::chrono::local_time<Duration>> {
  static auto get(std::chrono::local_time<Duration> value) {
    return fmt::local_time<Duration>(value.time_since_epoch());
  }
};
} // namespace ffmt_test
namespace {
ffmt_test::differential_suite suite("chrono");
void duration_units_and_precision() {
  suite.check("{}", std::chrono::seconds(42));
  suite.check("{:>15}", std::chrono::microseconds(-123));
  suite.check("{:%H:%M:%S}", std::chrono::seconds(-12345));
  suite.check("{:%T}", std::chrono::milliseconds(-123456));
  suite.check("{:%Q %q %R %j}", std::chrono::hours(73));
  suite.check("{:%S}", std::chrono::duration<long long, std::ratio<1, 3>>{1});
  suite.check("{}", std::chrono::duration<int, std::ratio<15, 4>>{42});
  suite.check("{:.2}", std::chrono::duration<double, std::milli>{1.2345});
  suite.check("{:%M:%S}", std::chrono::duration<double>{-61.25});
  suite.check("{:.3%T}",
              std::chrono::duration<double, std::nano>{-99123456789});
  suite.check("{:%S %r}", std::chrono::duration<double>{1.0});
  suite.check("{:%S %r}", std::chrono::duration<double>{1.5});
  suite.check("{:.2}", std::chrono::seconds(42));
  suite.check("{:.3%S}", std::chrono::seconds(42));
  suite.check("{:.2%S}", std::chrono::milliseconds(1234));
  for (double value : {-0.0, 0.0, 1.0, 1.5, -1.5, 12345.125, -12345.125,
                       std::numeric_limits<double>::infinity(),
                       -std::numeric_limits<double>::infinity(),
                       std::numeric_limits<double>::quiet_NaN()})
    suite.check("{:%Q %H %Q %M %S %q %R %T %r %p}",
                std::chrono::duration<double>{value});
  suite.check("{:%_T %-R}", std::chrono::milliseconds{1234});
}

void calendar_and_time_points() {
  suite.check("{}", std::chrono::year{2024});
  suite.check("{:>12%B}", std::chrono::month{1});
  suite.check("{}", std::chrono::day{5});
  suite.check("{}", std::chrono::weekday{6});
  auto date = std::chrono::year{2021} / 1 / 1;
  suite.check("{}", date);
  suite.check("{:%a %A %j %G-W%V-%u %U %W}", date);
  suite.check("{:%F %T %z %Z}", std::chrono::sys_seconds{});
  suite.check("{}", std::chrono::sys_time<std::chrono::milliseconds>{
                        std::chrono::milliseconds{-1234}});
  suite.check("{}", std::chrono::sys_time<std::chrono::duration<double>>{
                        std::chrono::duration<double>{-1.5}});
  suite.check("{}", std::chrono::sys_time<std::chrono::duration<double>>{
                        std::chrono::duration<double>{1.0}});
  suite.check("{:%S %T %X %r}",
              std::chrono::sys_time<std::chrono::duration<double>>{
                  std::chrono::duration<double>{1.125}});
  suite.check("{}", std::chrono::local_time<std::chrono::milliseconds>{
                        std::chrono::milliseconds{-61500}});
  for (int year : {1, 27, 999, 1969, 1970, 1975, 1977, 1999, 2000, 2021, 2024})
    for (unsigned month : {1u, 3u, 12u})
      for (unsigned day : {1u, 28u})
        suite.check("{:%F %a %A %j %G-W%V-%u %U %W}",
                    std::chrono::year(year) / month / day);
}

void tm_and_composite_tokens() {
  std::tm tm{};
  tm.tm_year = 124;
  tm.tm_mon = 1;
  tm.tm_mday = 29;
  tm.tm_hour = 23;
  tm.tm_min = 4;
  tm.tm_sec = 5;
  tm.tm_yday = 59;
  tm.tm_wday = 4;
  suite.check("{:*>40%F %T %a %B %j %G-W%V}", tm);
  suite.check("{:%EY %Oy %Om %Od %_H %-M %S %c %x %X %r}", tm);
  for (auto pattern :
       {"{:%F|%D|%x|%R|%T|%X|%r|%c}", "{:%_F|%_D|%_x|%_R|%_T|%_X|%_r|%_c}",
        "{:%-F|%-D|%-x|%-R|%-T|%-X|%-r|%-c}", "{:%Ec|%Ex|%EX}"}) {
    suite.check(pattern, tm);
    suite.check(pattern, std::chrono::sys_time<std::chrono::milliseconds>{
                             std::chrono::milliseconds{-1234}});
    suite.check(pattern, std::chrono::sys_time<std::chrono::duration<double>>{
                             std::chrono::duration<double>{1.125}});
  }
}

void wide_chrono() {
  suite.check_wide(L"{}", std::chrono::microseconds{123});
  suite.check_wide(L"{:%F %T}", std::chrono::sys_seconds{});
}

void invalid_specs() {
  std::tm tm{};
  suite.reject("{:%Y}", std::chrono::seconds(1));
  suite.reject("{:.2%Q}", std::chrono::seconds(1));
  suite.reject("{:%}", tm);
  suite.reject("{:%Ea}", tm);
  suite.reject("{:%Oa}", tm);
  suite.reject("{:%0S}", tm);
  suite.reject("{:literal}", tm);
  suite.reject("{:%z}", std::chrono::local_time<std::chrono::seconds>{});
}

} // namespace
int main() {
  duration_units_and_precision();
  calendar_and_time_points();
  tm_and_composite_tokens();
  wide_chrono();
  invalid_specs();
  suite.report();
}
