#include <include/database/entities/Profile.h>

namespace Configs
{
    Profile::Profile(Configs::outbound *outbound, Configs::AbstractBean *bean, const QString &type_)
    {
        if (!type_.isEmpty()) this->type = type_;

        if (bean != nullptr) {
            this->_bean = std::shared_ptr<Configs::AbstractBean>(bean);
        }

        if (outbound != nullptr) {
            this->outbound = std::shared_ptr<Configs::outbound>(outbound);
        }
    }

    QString Profile::DisplayTestResult() const {
        QString result;
        if (latency < 0) {
            result = "Unavailable";
        } else if (latency > 0) {
            if (!test_country.isEmpty()) result += UNICODE_LRO + CountryCodeToFlag(test_country) + " ";
            result += QString("%1 ms").arg(latency);
        }
        if (!dl_speed.isEmpty() && dl_speed != "N/A") result += " ↓" + dl_speed;
        if (!ul_speed.isEmpty() && ul_speed != "N/A") result += " ↑" + ul_speed;
        return result;
    }

    QColor Profile::DisplayLatencyColor() const {
        if (latency < 0) {
            return Qt::darkGray;
        } else if (latency > 0) {
            if (latency <= 100) {
                return Qt::darkGreen;
            } else if (latency <= 300)
            {
                return Qt::darkYellow;
            } else {
                return Qt::red;
            }
        } else {
            return {};
        }
    }
}
