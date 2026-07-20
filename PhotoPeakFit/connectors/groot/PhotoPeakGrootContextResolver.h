#ifndef __PHOTOPEAKGROOTCONTEXTRESOLVER_H__
#define __PHOTOPEAKGROOTCONTEXTRESOLVER_H__

#include <string>

struct GPluginContext;
class TH1;
class TVirtualPad;

TH1* ResolvePhotoPeakHistogram(const GPluginContext& context,
                               std::string& error);
bool IsPhotoPeakTargetValid(TVirtualPad* pad, TH1* histogram);

#endif
