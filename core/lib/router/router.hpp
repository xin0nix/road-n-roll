//
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#pragma once

#include <boost/url/decode_view.hpp>
#include <boost/url/detail/config.hpp>
#include <boost/url/detail/except.hpp>
#include <boost/url/grammar.hpp>
#include <boost/url/parse_path.hpp>

#include "matches.hpp"

#include <deque>

namespace boost {
namespace urls {
namespace router {

// Паттерн сегмента пути к ресурсу
struct SegmentPattern {
  SegmentPattern() = default;
  bool match(pct_string_view seg) const;
  std::string_view string() const { return str_; }
  std::string_view id() const;
  bool empty() const { return str_.empty(); }
  bool isLiteral() const { return isLiteral_; }
  friend bool operator==(SegmentPattern const &a, SegmentPattern const &b) {
    if (a.isLiteral_ and b.isLiteral_)
      return a.str_ == b.str_;
    return false;
  }

  friend bool operator<(SegmentPattern const &a, SegmentPattern const &b) {
    if (a.isLiteral_ and b.isLiteral_)
      return a.str_ < b.str_;
    return false;
  }

  std::string str_;
  bool isLiteral_ = true;
};

struct SegmentPatternRule {
  using value_type = SegmentPattern;

  system::result<value_type> parse(char const *&it,
                                   char const *end) const noexcept;
};

constexpr auto kSegmentPatternRule = SegmentPatternRule{};

constexpr auto kPathPatternRule = grammar::tuple_rule(
    grammar::squelch(grammar::optional_rule(grammar::delim_rule('/'))),
    grammar::range_rule(
        kSegmentPatternRule,
        grammar::tuple_rule(grammar::squelch(grammar::delim_rule('/')),
                            kSegmentPatternRule)));

// Ресурс маршрутизатора с удаленным типом
struct AnyResource {
  virtual ~AnyResource() = default;
  virtual void const *get() const noexcept = 0;
};

/**
 * @brief Узел в дереве ресурсов
 */
struct ResourceNode {
  SegmentPattern seg{};

  // FIXME(xin0nix): меня смущает сырой указатель
  AnyResource const *resource{nullptr};
  std::string pathPattern;
  std::optional<std::reference_wrapper<ResourceNode>> parent;
  std::vector<std::reference_wrapper<ResourceNode>> children;
};

struct ResourceTree {
  ResourceTree() {
    // Корневой узел без каких-либо связанных с ним ресурсов
    nodes_.push_back(ResourceNode{});
  }

  ~ResourceTree() {
    for (auto &r : nodes_)
      delete r.resource;
  }

  /**
   * @brief Вставляет ресурс в дерево маршрутизации.
   *
   * @param path Исходный, ненормализованный путь
   * @param v Указатель на ресурс для вставки
   *
   * @note Метод сохраняет отсортированный порядок дочерних узлов
   * для оптимизации последующего поиска.
   *
   * @warning Если парсинг пути не удался, ресурс удаляется,
   * и никаких изменений в дереве не производится.
   */
  void insertImpl(std::string_view path, AnyResource const *v);

  /**
   * @brief Ищет ресурс в дереве маршрутизации по заданному пути.
   *
   * @param path Закодированные сегменты пути для поиска
   * @param matches Указатель на массив для хранения совпавших сегментов
   * @param ids Указатель на массив для хранения идентификаторов сегментов
   *
   * @return Указатель на найденный ресурс или nullptr, если ресурс не найден
   *
   * @note Метод модифицирует указатели matches и ids для передачи
   * дополнительной информации о найденном пути.
   */
  AnyResource const *findImpl(segments_encoded_view path,
                              std::string_view *&matches,
                              std::string_view *&ids) const;

protected:
  /**
   * @brief Рекурсивно ищет соответствие пути в дереве ресурсов.
   *
   * @param it Итератор начала текущего сегмента пути
   * @param end Итератор конца пути
   * @param cur Текущий узел в дереве
   * @param level Текущий уровень в дереве
   * @param matches Указатель на массив для сохранения совпавших сегментов
   * @param ids Указатель на массив для сохранения идентификаторов сегментов
   *
   * @note В основе алгоритм лежит backtracking с восстановлением состояний
   *
   * @return Указатель на найденный узел ресурса или nullptr
   */
  ResourceNode const *tryMatch(segments_encoded_view::const_iterator it,
                               segments_encoded_view::const_iterator end,
                               ResourceNode const *root, int level,
                               std::string_view *&matches,
                               std::string_view *&ids) const;

  // Пул узлов в дереве ресурсов. Для доступа к ним используется индекс.
  std::deque<ResourceNode> nodes_;
};

} // namespace router

/** @brief Маршрутизатор URL для эффективной обработки веб-запросов.
 *
 * @tparam T Тип обработчика (например, std::function<void(Request&,
 * Response&)>). Позволяет гибко определять тип обработчиков для разных нужд.
 *
 * @par Безопасность исключений
 * Обеспечивает надежную работу в условиях возможных исключений,
 * что критично для стабильности веб-сервера.
 */
template <class T> struct Router : router::ResourceTree {
  /// Конструктор
  Router() = default;

  /** @brief Добавляет новый маршрут в систему маршрутизации.
   *
   * @param pattern URL-шаблон, может содержать параметры в фигурных скобках
   * @param v Обработчик, который будет вызван при совпадении URL
   */
  template <class U> void insert(std::string_view pattern, U &&v) {
    BOOST_STATIC_ASSERT(std::is_same_v<T, U> || std::is_convertible_v<U, T> ||
                        std::is_base_of_v<T, U>);
    using U_ = typename std::decay<
        typename std::conditional<std::is_base_of_v<T, U>, U, T>::type>::type;
    struct Impl : router::AnyResource {
      U_ u;
      explicit Impl(U &&u_) : u(std::forward<U>(u_)) {}
      void const *get() const noexcept override {
        return static_cast<T const *>(&u);
      }
    };
    insertImpl(pattern, new Impl(v));
  }

  /** @brief Находит подходящий обработчик для заданного URL-пути.
   *
   * @param path Входящий URL-путь для обработки
   * @param m Объект для хранения информации о совпадениях
   * @return Указатель на обработчик или nullptr, если совпадений не найдено
   */
  T const *find(segments_encoded_view path, MatchesBase &m) const noexcept {
    std::string_view *matches_it = m.matches();
    std::string_view *ids_it = m.ids();
    router::AnyResource const *p = findImpl(path, matches_it, ids_it);
    if (p) {
      BOOST_ASSERT(matches_it >= m.matches());
      m.resize(static_cast<std::size_t>(matches_it - m.matches()));
      return reinterpret_cast<T const *>(p->get());
    }
    m.resize(0);
    return nullptr;
  }
};

} // namespace urls
} // namespace boost
