#pragma once

#include <cstddef>
#include <optional>

#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVariant>

#include "domain/query/QueryOptions.h"

class QSqlQuery;

namespace gamelog::core::database
{
    /**
     * Assembles the WHERE/ORDER BY/LIMIT/OFFSET tail of a repository query and
     * binds its parameters.
     *
     * Deliberately free of domain knowledge: it never decides which columns
     * exist or what a filter means. Repositories keep their own column mapping,
     * sort-field translation, and any dialect-specific predicates, and use this
     * only for the mechanical assembly the two of them would otherwise duplicate.
     *
     * Predicates are combined with AND in the order they were added, and every
     * value is bound as a named parameter rather than inlined, so callers cannot
     * accidentally build an injectable statement.
     */
    class SqlQueryBuilder
    {
    public:
        /**
         * Adds one predicate with no bound value, such as "x IS NULL".
         */
        void addPredicate(const QString& expression);

        /**
         * Adds one predicate together with the value for its placeholder.
         * @param expression The SQL predicate, referencing placeholder.
         * @param placeholder The named placeholder, including its leading colon.
         * @param value The value to bind.
         */
        void addPredicate(const QString& expression, const QString& placeholder, const QVariant& value);

        /**
         * Adds a "column IN (...)" predicate, generating one numbered
         * placeholder per value. Adds nothing when values is empty.
         * @param column The column or expression to test.
         * @param placeholderPrefix Prefix for generated placeholders, without a colon.
         * @param values The values to match.
         */
        void addInPredicate(const QString& column, const QString& placeholderPrefix, const QList<QVariant>& values);

        /**
         * Sets the ORDER BY expression and direction. The expression is emitted
         * verbatim, so callers must supply a trusted, non-user-derived string.
         */
        void setOrderBy(const QString& expression, domain::query::SortDirection direction);

        /**
         * Sets the optional LIMIT and OFFSET.
         */
        void setLimitOffset(const std::optional<std::size_t>& limit, const std::optional<std::size_t>& offset);

        /**
         * Appends the assembled tail to baseSql.
         */
        [[nodiscard]] QString buildSql(const QString& baseSql) const;

        /**
         * Binds every collected value onto an already-prepared query.
         */
        void bindTo(QSqlQuery& query) const;

    private:
        QStringList predicates_;
        QList<QPair<QString, QVariant>> bindings_;
        QString orderByExpression_;
        domain::query::SortDirection sortDirection_{domain::query::SortDirection::Ascending};
        std::optional<std::size_t> limit_;
        std::optional<std::size_t> offset_;
    };
} // namespace gamelog::core::database
