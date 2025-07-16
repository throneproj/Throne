#include <QFile>
#include <QFileInfo>
#include <QString>
#include <include/global/Utils.hpp>

int Mac_Run_Command(QString command) {
    auto cmd = QString("osascript -e 'tell application \"Terminal\" to activate' -e 'tell application \"Terminal\" to do script \"%1\"' with administrator privileges").arg(command);
    return system(cmd.toStdString().c_str());
}
