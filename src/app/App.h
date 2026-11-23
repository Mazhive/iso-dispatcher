#pragma once

#include "core/Config.h"
#include "core/Knowledge.h"
#include "core/Classifier.h"
#include "core/IsoProbe.h"
#include "core/Launcher.h"

#include <QStringList>

namespace iso {

class App
{
public:
    int run(int argc, char **argv);

private:
    bool ensureConfig(const QString &path, Config &cfg);
};

} // namespace iso
