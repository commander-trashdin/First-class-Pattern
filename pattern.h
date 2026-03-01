#pragma once

#include <functional>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace pattern {

// ============================================================
// function_traits
// Extract argument + return type from unary const call operator
// ============================================================

template <typename> struct function_traits;

template <typename C, typename R, typename Arg>
struct function_traits<R (C::*)(Arg) const> {
  using arg_type = Arg;
  using return_type = R;
};

// ============================================================
// Pattern
// F must be callable: T -> std::optional<U>
// ============================================================

template <typename F> struct Pattern {
  F f;

  using traits = function_traits<decltype(&F::operator())>;
  using T =
      std::remove_cv_t<std::remove_reference_t<typename traits::arg_type>>;
  using OptionalU = typename traits::return_type;
  using U = typename OptionalU::value_type;

  static_assert(std::is_same_v<OptionalU, std::optional<U>>,
                "Pattern callable must return std::optional<U>");

  OptionalU operator()(const T &value) const { return f(value); }
};

template <typename F> Pattern(F) -> Pattern<F>;

// ============================================================
// Dispatcher
// First-match semantics
// ============================================================

template <typename... Ps> struct Dispatcher {
  std::tuple<Ps...> patterns;

  explicit Dispatcher(Ps... ps) : patterns(std::move(ps)...) {}

  template <typename T> auto operator()(const T &value) const {
    return try_match(value, std::index_sequence_for<Ps...>{});
  }

private:
  template <typename T, std::size_t... I>
  auto try_match(const T &value, std::index_sequence<I...>) const {
    using First = std::tuple_element_t<0, std::tuple<Ps...>>;
    using R = typename First::U;

    std::optional<R> result;

    ((result = result ? result : std::get<I>(patterns)(value)), ...);

    return result;
  }
};

template <typename... Ps> Dispatcher(Ps...) -> Dispatcher<Ps...>;

// ============================================================
// DynamicDispatcher
// Runtime rule aggregation (hybrid model)
// ============================================================

template <typename T, typename R> class DynamicDispatcher {
public:
  using Rule = std::function<std::optional<R>(const T &)>;

  DynamicDispatcher() = default;

  template <typename P> void add(P &&p) {
    using Decayed = std::decay_t<P>;

    static_assert(std::is_same_v<T, typename Decayed::T>,
                  "Pattern input type mismatch");

    static_assert(std::is_same_v<R, typename Decayed::U>,
                  "Pattern output type mismatch");

    rules.emplace_back(
        [pat = std::forward<P>(p)](const T &t) { return pat(t); });
  }

  std::optional<R> operator()(const T &value) const {
    for (const auto &rule : rules) {
      if (auto r = rule(value))
        return r;
    }
    return std::nullopt;
  }

private:
  std::vector<Rule> rules;
};

// ============================================================
// map
// Transform successful match value
// ============================================================

template <typename F, typename G> auto map(const Pattern<F> &pat, G g) {
  using T = typename Pattern<F>::T;
  using U = typename Pattern<F>::U;
  using R = std::invoke_result_t<G, U>;

  auto lambda = [=](const T &t) -> std::optional<R> {
    if (auto r = pat(t))
      return g(*r);
    return std::nullopt;
  };

  return Pattern(lambda);
}

// ============================================================
// or_else
// Prioritized alternative
// ============================================================

template <typename F1, typename F2>
auto or_else(const Pattern<F1> &p1, const Pattern<F2> &p2) {

  using T = typename Pattern<F1>::T;
  using R = typename Pattern<F1>::U;

  static_assert(std::is_same_v<T, typename Pattern<F2>::T>,
                "or_else requires same input type");

  static_assert(std::is_same_v<R, typename Pattern<F2>::U>,
                "or_else requires same output type");

  auto lambda = [=](const T &t) -> std::optional<R> {
    if (auto r = p1(t))
      return r;
    return p2(t);
  };

  return Pattern(lambda);
}

// ============================================================
// and_then
// Dependent chaining (monadic bind)
// ============================================================

template <typename F, typename G> auto and_then(const Pattern<F> &pat, G g) {
  using T = typename Pattern<F>::T;
  using U = typename Pattern<F>::U;

  using NextPattern = decltype(g(std::declval<U>()));
  using R = typename NextPattern::U;

  static_assert(std::is_same_v<T, typename NextPattern::T>,
                "and_then requires same input type");

  auto lambda = [=](const T &t) -> std::optional<R> {
    if (auto r = pat(t))
      return g(*r)(t);
    return std::nullopt;
  };

  return Pattern(lambda);
}

} // namespace pattern