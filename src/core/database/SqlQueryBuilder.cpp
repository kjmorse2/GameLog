#include "database/SqlQueryBuilder.h"

#include <QSqlQuery>

using gamelog::core::domain::query::SortDirection;

namespace gamelog::core::database
{
    void SqlQueryBuilder::addPredicate(const QString& expression) { predicates_.push_back(expression); }

    void SqlQueryBuilder::addPredicate(const QString& expression, const QString& placeholder, const QVariant& value)
    {
        predicates_.push_back(expression);
        bindings_.push_back({placeholder, value});
    }

    void SqlQueryBuilder::addInPredicate(const QString& column,
                                         const QString& placeholderPrefix,
                                         const QList<QVariant>& values)
    {
        if(values.isEmpty()) { return; }

        QStringList placeholders;
        placeholders.reserve(values.size());

        for(qsizetype index = 0; index < values.size(); ++index)
        {
            const QString placeholder = QStringLiteral(":%1_%2").arg(placeholderPrefix).arg(index);
            placeholders.push_back(placeholder);
            bindings_.push_back({placeholder, values.at(index)});
        }

        predicates_.push_back(QStringLiteral("%1 IN (%2)").arg(column, placeholders.join(QStringLiteral(", "))));
    }

    void SqlQueryBuilder::setOrderBy(const QString& expression, const SortDirection direction)
    {
        orderByExpression_ = expression;
        sortDirection_ = direction;
    }

    void SqlQueryBuilder::setLimitOffset(const std::optional<std::size_t>& limit,
                                         const std::optional<std::size_t>& offset)
    {
        limit_ = limit;
        offset_ = offset;
    }

    QString SqlQueryBuilder::buildSql(const QString& baseSql) const
    {
        QString sql = baseSql;

        if(!predicates_.isEmpty()) { sql += QStringLiteral(" WHERE ") + predicates_.join(QStringLiteral(" AND ")); }

        if(!orderByExpression_.isEmpty())
        {
            sql += QStringLiteral(" ORDER BY ") + orderByExpression_;
            sql += sortDirection_ == SortDirection::Ascending ? QStringLiteral(" ASC") : QStringLiteral(" DESC");
        }

        if(limit_) { sql += QStringLiteral(" LIMIT :limit"); }

        if(offset_)
        {
            // SQLite requires LIMIT when OFFSET is present. -1 means no upper limit.
            if(!limit_) { sql += QStringLiteral(" LIMIT -1"); }
            sql += QStringLiteral(" OFFSET :offset");
        }

        return sql;
    }

    void SqlQueryBuilder::bindTo(QSqlQuery& query) const
    {
        for(const auto& [placeholder, value] : bindings_) { query.bindValue(placeholder, value); }

        if(limit_) { query.bindValue(QStringLiteral(":limit"), QVariant::fromValue<qulonglong>(*limit_)); }
        if(offset_) { query.bindValue(QStringLiteral(":offset"), QVariant::fromValue<qulonglong>(*offset_)); }
    }
} // namespace gamelog::core::database
