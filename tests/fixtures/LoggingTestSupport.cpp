#include "fixtures/LoggingTestSupport.h"

#include <QLoggingCategory>

namespace gamelog::tests::fixtures
{
    namespace
    {
        QLoggingCategory::CategoryFilter previousFilter = nullptr;
        bool installed = false;

        void gameLogCategoryFilter(QLoggingCategory* category)
        {
            if(previousFilter != nullptr) { previousFilter(category); }

            if(QByteArray{category->categoryName()}.startsWith("gamelog."))
            {
                category->setEnabled(QtDebugMsg, true);
                category->setEnabled(QtInfoMsg, true);
                category->setEnabled(QtWarningMsg, true);
                category->setEnabled(QtCriticalMsg, true);
            }
        }
    } // namespace

    void enableGameLogLoggingCategories()
    {
        if(installed) { return; }

        previousFilter = QLoggingCategory::installFilter(gameLogCategoryFilter);
        installed = true;
    }
} // namespace gamelog::tests::fixtures
