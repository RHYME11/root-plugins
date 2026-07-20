#include "PhotoPeakGrootContextResolver.h"

#include <vector>

#include <TH1.h>
#include <TList.h>
#include <TObject.h>
#include <TVirtualPad.h>

#include <Plugin/GPluginContext.h>

// ============== IsPhotoPeakTargetValid ==============
// Purpose: Test whether a locked histogram remains directly drawn in its pad.
// Inputs: Session pad and target histogram identity.
// Outputs: True without dereferencing a removed target.
bool IsPhotoPeakTargetValid(TVirtualPad* pad, TH1* histogram) {
  return pad && histogram && pad->GetListOfPrimitives() &&
    pad->GetListOfPrimitives()->FindObject(histogram) == histogram;
}

// ============== ResolvePhotoPeakHistogram ==============
// Purpose: Resolve one unambiguous directly drawn one-dimensional histogram.
// Inputs: Groot action context and error output.
// Outputs: Selected or sole TH1/GH1 target.
TH1* ResolvePhotoPeakHistogram(const GPluginContext& context,
                               std::string& error) {
  auto* selected = dynamic_cast<TH1*>(context.selected);
  if(IsPhotoPeakTargetValid(context.pad, selected) &&
     selected->GetDimension() == 1)
    return selected;
  if(!context.pad || !context.pad->GetListOfPrimitives()) {
    error = "select or draw one one-dimensional histogram first";
    return nullptr;
  }

  std::vector<TH1*> histograms;
  TIter next(context.pad->GetListOfPrimitives());
  while(TObject* object = next()) {
    auto* histogram = dynamic_cast<TH1*>(object);
    if(histogram && histogram->GetDimension() == 1)
      histograms.push_back(histogram);
  }
  if(histograms.size() == 1)
    return histograms.front();
  error = histograms.empty() ?
    "select or draw one one-dimensional histogram first" :
    "multiple histograms are drawn; click the target histogram first";
  return nullptr;
}
