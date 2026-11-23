#pragma once

#include "Types.h"
#include "Config.h"
#include "Knowledge.h"

namespace iso {

class Classifier
{
public:
    Classifier(const Config &cfg, Knowledge &knowledge);

    Decision classify(const ProbeResult &probe) const;
    QStringList explain(const ProbeResult &probe, const Decision &d) const;

private:
    double applyRules(const ClassDef &cls, const ProbeResult &probe, QStringList &reasons) const;
    const Config &m_cfg;
    Knowledge &m_kb;
};

} // namespace iso
