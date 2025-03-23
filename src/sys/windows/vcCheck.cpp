#include <include/sys/windows/vcCheck.h>

#include <QSettings>

bool checkVCRedist()
{
    QSettings settings("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Microsoft\\VisualStudio\\14.0\\VC\\Runtimes\\x64", QSettings::NativeFormat);

    return (settings.value("Installed").toInt() == 1 ? settings.value("Bld").toInt() >= 32919 : false);
}
