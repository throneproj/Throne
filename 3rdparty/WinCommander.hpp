/****************************************************************************
**
** Copyright (C) 2014 UpdateNode UG (haftungsbeschränkt)
** Contact: code@updatenode.com
**
** This file is part of the UpdateNode Client.
**
** Commercial License Usage
** Licensees holding valid commercial UpdateNode license may use this file
** under the terms of the the Apache License, Version 2.0
** Full license description file: LICENSE.COM
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation. Please review the following information to ensure the
** GNU General Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
** Full license description file: LICENSE.GPL
**
****************************************************************************/

#pragma once

#include <QString>
#include <QStringList>

class WinCommander {
public:
    // Do not reuse the Win32 SW_* names here: windows.h defines them as
    // preprocessor macros, which also expand in qualified expressions such as
    // WinCommander::SW_HIDE when CMake unity builds combine translation units.
    static constexpr int WindowHidden = 0;
    static constexpr int WindowNormal = 1;
    static constexpr int WindowMinimized = 2;

    static uint runProcessElevated(const QString &path,
                                   const QStringList &parameters = QStringList(),
                                   const QString &workingDir = QString(),
                                   int nShow = WindowMinimized, bool aWait = true);
};
