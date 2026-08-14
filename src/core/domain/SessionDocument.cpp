#include "SessionDocument.h"

#include <QDebugStateSaver>

namespace gamelog::core::domain
{
    QDebug operator<<(QDebug debug, const SessionDocument& document)
    {
        QDebugStateSaver saver{debug};

        debug.nospace() << "SessionDocument {"
                << "sessionId: " << document.sessionId
                << ", htmlContent: " << document.htmlContent
                << ", lastSavedTimestamp: " << document.lastSavedTimestamp
                << "}";
        return debug;
    }
} // namespace gamelog::core::domain
