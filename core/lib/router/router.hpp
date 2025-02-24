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
#include <boost/url/grammar/unsigned_rule.hpp>
#include <boost/url/parse_path.hpp>

#include "matches.hpp"

namespace boost {
namespace urls {

namespace detail {

// Паттерн сегмента пути к ресурсу
struct SegmentPattern {
  SegmentPattern() = default;

  bool match(pct_string_view seg) const;

  core::string_view string() const { return str_; }

  core::string_view id() const;

  bool empty() const { return str_.empty(); }

  bool isLiteral() const { return isLiteral_; }

  bool hasModifier() const {
    return !isLiteral() && modifier_ != modifier::none;
  }

  bool isOptional() const { return modifier_ == modifier::optional; }

  bool isStar() const { return modifier_ == modifier::star; }

  bool isPlus() const { return modifier_ == modifier::plus; }

  friend bool operator==(SegmentPattern const &a, SegmentPattern const &b) {
    if (a.isLiteral_ != b.isLiteral_)
      return false;
    if (a.isLiteral_)
      return a.str_ == b.str_;
    return a.modifier_ == b.modifier_;
  }

  // Сегменты имеют следующий приоритет:
  // - строковый литерал
  // - уникальный
  // - опциональный
  // - плюс
  // - звездочка
  friend bool operator<(SegmentPattern const &a, SegmentPattern const &b) {
    if (b.isLiteral())
      return false;
    if (a.isLiteral())
      return !b.isLiteral();
    return a.modifier_ < b.modifier_;
  }

private:
  enum class modifier : unsigned char {
    none,
    // {id?}
    optional,
    // {id*}
    star,
    // {id+}
    plus
  };

  std::string str_;
  bool isLiteral_ = true;
  modifier modifier_ = modifier::none;

  friend struct SegmentPatternRule;
};

using ChildIdxVector = std::vector<std::size_t>;

// Ресурс маршрутизатора с удаленным типом
struct AnyResource {
  virtual ~AnyResource() = default;
  virtual void const *get() const noexcept = 0;
};

// Узел в дереве ресурсов
// Каждый сегмент в дереве ресурсов может быть связан с:
struct ResourceNode {
  static constexpr std::size_t npos{std::size_t(-1)};

  // Литеральный сегмент или поле замены
  SegmentPattern seg{};

  // FIXME(xin0nix): меня смущает сырой указатель
  // Указатель на ресурс
  AnyResource const *resource{nullptr};

  // Полное совпадение для ресурса
  std::string pathPattern;

  // Индекс родительского узла в реализации пула узлов
  std::size_t parent{npos};

  // Индексы дочерних узлов в пуле узлов
  ChildIdxVector children;
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
   * @details Этот метод нормализует путь, разбирает его на сегменты,
   * и вставляет ресурс в соответствующий узел дерева. Если необходимые
   * узлы отсутствуют, они создаются динамически. Метод поддерживает
   * как статические, так и динамические сегменты пути.
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
  void insertImpl(core::string_view path, AnyResource const *v);

  /**
   * @brief Ищет ресурс в дереве маршрутизации по заданному пути.
   *
   * @details Метод выполняет поиск ресурса, соответствующего указанному пути.
   * Он обрабатывает пустые пути, используя "./", и рекурсивно проходит по
   * дереву для поиска совпадения. При нахождении совпадения, метод также
   * заполняет информацию о совпавших сегментах и идентификаторах.
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
                              core::string_view *&matches,
                              core::string_view *&ids) const;

protected:
  /**
   * @brief Рекурсивно ищет соответствие пути в дереве ресурсов.
   *
   * @details Метод проходит по сегментам пути, сопоставляя их с узлами дерева.
   * Поддерживает литералы, опциональные и множественные сегменты. Выполняет
   * ветвление при необходимости для поиска наилучшего соответствия.
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
                               core::string_view *&matches,
                               core::string_view *&ids) const;

  /**
   * @brief Поиск опционального ресурса в дереве ресурсов.

   * @param root Указатель на корневой узел, с которого начинается поиск
   * @param ns Вектор узлов ресурсов, представляющий дерево
   * @param matches Указатель на массив для сохранения совпавших сегментов
   * @param ids Указатель на массив для сохранения идентификаторов сегментов
   * @return Указатель на найденный опциональный ресурс или nullptr
   *
   * @details Метод рекурсивно обходит дерево ресурсов, начиная с заданного
   * корневого узла, в поисках опционального ресурса. Он проверяет каждый
   * дочерний узел на наличие опционального сегмента или сегмента-звездочки. При
   * нахождении такого узла, метод обновляет matches и ids, и продолжает поиск в
   * глубину. Если ресурс не найден, метод восстанавливает предыдущие значения
   * matches и ids.
   */
  ResourceNode const *findOptionalResource(const ResourceNode *root,
                                           core::string_view *&matches,
                                           core::string_view *&ids) const;

  // Pool of nodes in the resource tree
  std::vector<ResourceNode> nodes_;
};

} // namespace detail

/** @brief Маршрутизатор URL для эффективной обработки веб-запросов.
 *
 * Этот класс предоставляет механизм для сопоставления входящих URL-запросов
 * с соответствующими обработчиками. Это критически важно для создания
 * масштабируемых веб-приложений, так как позволяет:
 * 1. Организовать логику обработки запросов в структурированном виде.
 * 2. Эффективно направлять запросы к нужным обработчикам.
 * 3. Поддерживать как статические, так и динамические URL-пути.
 *
 * @tparam T Тип обработчика (например, std::function<void(Request&,
 * Response&)>). Позволяет гибко определять тип обработчиков для разных нужд.
 *
 * @par Безопасность исключений
 * Обеспечивает надежную работу в условиях возможных исключений,
 * что критично для стабильности веб-сервера.
 */
template <class T> struct Router : detail::ResourceTree {
  /// Конструктор
  Router() = default;

  /** @brief Добавляет новый маршрут в систему маршрутизации.
   *
   * Этот метод позволяет связать URL-шаблон с конкретным обработчиком.
   * Поддерживает как статические, так и динамические сегменты в URL,
   * что дает гибкость в определении маршрутов.
   *
   * @param pattern URL-шаблон, может содержать параметры в фигурных скобках
   * @param v Обработчик, который будет вызван при совпадении URL
   */
  template <class U> void insert(core::string_view pattern, U &&v) {
    BOOST_STATIC_ASSERT(std::is_same_v<T, U> || std::is_convertible_v<U, T> ||
                        std::is_base_of_v<T, U>);
    using U_ = typename std::decay<
        typename std::conditional<std::is_base_of_v<T, U>, U, T>::type>::type;

    struct Impl : detail::AnyResource {
      U_ u;

      explicit Impl(U &&u_) : u(std::forward<U>(u_)) {}

      void const *get() const noexcept override {
        return static_cast<T const *>(&u);
      }
    };
  }

  /** @brief Находит подходящий обработчик для заданного URL-пути.
   *
   * Этот метод является ключевым для процесса маршрутизации. Он принимает
   * входящий URL-путь и возвращает соответствующий обработчик, если такой
   * найден. Также заполняет информацию о совпавших сегментах пути, что может
   * быть полезно для извлечения параметров из URL.
   *
   * @param path Входящий URL-путь для обработки
   * @param m Объект для хранения информации о совпадениях
   * @return Указатель на обработчик или nullptr, если совпадений не найдено
   */
  T const *find(segments_encoded_view path, MatchesBase &m) const noexcept {
    core::string_view *matches_it = m.matches();
    core::string_view *ids_it = m.ids();
    detail::AnyResource const *p = findImpl(path, matches_it, ids_it);
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
