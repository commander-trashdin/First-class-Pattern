# Pattern

A small header-only library providing first-class pattern objects and ordered dispatch in C++.

Patterns are modeled as:

`T -> std::optional<U>`

A pattern attempts to extract a value from an input. If it matches, it returns `std::optional<U>`. Otherwise, it returns `std::nullopt`. This enables:

- First-class pattern objects
- Composable pattern transformations
- Ordered dispatch (first-match semantics)
- Arbitrary predicate-based rules

## Basic Pattern:

```c++
#include <variant>
#include <iostream>
#include "pattern.hpp"

using E = std::variant<int, bool>;
using namespace pattern;

int main() {
    auto int_pattern = Pattern{
        [](const E& e) -> std::optional<int> {
            if (auto p = std::get_if<int>(&e))
                return *p;
            return std::nullopt;
        }
    };

    E e = 42;

    if (auto r = int_pattern(e)) {
        std::cout << "matched int: " << *r << "\n";
    }
}
```

## Map

Transforms the extracted value.

```c++
auto int_to_string =
    map(int_pattern, [](int x) {
        return std::string("int: ") + std::to_string(x);
    });

E e = 21;

if (auto r = int_to_string(e)) {
    std::cout << *r << "\n";
}
```

Type transformation:

```
Pattern<t, u="">  +  (U -> R) → Pattern<t, r="">
```

## Or_else

Prioritized alternative (first successful match wins).

```c++
auto bool_pattern = Pattern{
    [](const E& e) -> std::optional<std::string> {
        if (auto p = std::get_if<bool>(&e))
            return *p ? "true" : "false";
        return std::nullopt;
    }
};

auto int_string =
    map(int_pattern, [](int x) {
        return std::to_string(x);
    });

auto combined = or_else(int_string, bool_pattern);

E e = true;

if (auto r = combined(e)) {
    std::cout << *r << "\n";
}
```

Both patterns must share:

- Same input type
- Same output type

## And_then

Dependent chaining (monadic bind). Allows the next pattern to depend on the extracted value.

```c++
auto positive_only =
    and_then(int_pattern, [](int x) {
        return Pattern{
            [=](const E&) -> std::optional<std::string> {
                if (x > 0)
                    return "positive";
                return std::nullopt;
            }
        };
    });

E e = 5;

if (auto r = positive_only(e)) {
    std::cout << *r << "\n";
}
```

Type behavior:

```
Pattern<T, U>  +  (U -> Pattern<T, R>) → Pattern<T, R>
```

## Dispatcher

Ordered rule dispatch over multiple patterns.

```c++
auto int_string =
    Pattern{
        [](const E& e) -> std::optional<std::string> {
            if (auto p = std::get_if<int>(&e))
                return "int: " + std::to_string(*p);
            return std::nullopt;
        }
    };

auto bool_string =
    Pattern{
        [](const E& e) -> std::optional<std::string> {
            if (auto p = std::get_if<bool>(&e))
                return std::string("bool: ") + (*p ? "true" : "false");
            return std::nullopt;
        }
    };

auto dispatch = Dispatcher{ int_string, bool_string };

E e = true;

if (auto r = dispatch(e)) {
    std::cout << *r << "\n";
}
```

Dispatcher semantics:

- Tries patterns in order
- Returns first successful match
- Returns std::nullopt if none match

## Dynamic Dispatcher

Same as above, but allows for runtime extension.

```c++
#include <variant>
#include <iostream>
#include "pattern.hpp"

using namespace pattern;

using E = std::variant<int, bool>;

int main() {

    auto int_pat = Pattern{
        [](const E& e) -> std::optional<std::string> {
            if (auto p = std::get_if<int>(&e))
                return "int: " + std::to_string(*p);
            return std::nullopt;
        }
    };

    auto bool_pat = Pattern{
        [](const E& e) -> std::optional<std::string> {
            if (auto p = std::get_if<bool>(&e))
                return std::string("bool: ") + (*p ? "true" : "false");
            return std::nullopt;
        }
    };

    DynamicDispatcher<E, std::string> dispatcher;

    dispatcher.add(int_pat);   // static pattern
    dispatcher.add(bool_pat);  // static pattern

    // runtime rule
    dispatcher.add([](const E& e) -> std::optional<std::string> {
        if (auto p = std::get_if<int>(&e); p && *p == 0)
            return "zero";
        return std::nullopt;
    });

    E e = true;

    if (auto r = dispatcher(e)) {
        std::cout << *r << "\n";
    }
}
```

Important trade-offs:

- Runtime indirection via `std::function`
- Linear scan over rules
- Registration order matters

## Arbitrary Conditions

Patterns are not restricted to structural matching.

```c++
auto positive_int = Pattern{
    [](const E& e) -> std::optional<std::string> {
        if (auto p = std::get_if<int>(&e); p && *p > 0)
            return "positive int";
        return std::nullopt;
    }
};
```

Patterns may overlap. Dispatcher ordering determines behavior.

## Design Notes

- No exhaustiveness guarantees
- Patterns may overlap
- First-match semantics
- Fully compile-time
- No reflection beyond lambda signature extraction

Types:

```
Pattern<T, U> ≅ T -> std::optional<U>
Dispatcher<T, U> ≅ T -> std::optional<U>
```

This is a lightweight rule-dispatch abstraction layered over C++.
