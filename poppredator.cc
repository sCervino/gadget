#include "poppredator.h"
#include "errorhandler.h"
#include "gadget.h"

extern ErrorHandler handle;

PopPredator::PopPredator(const char* givenname, const IntVector& Areas,
  const LengthGroupDivision* const OtherLgrpDiv, const LengthGroupDivision* const GivenLgrpDiv)
  : Predator(givenname, Areas) {

  int i;
  if (isZero(GivenLgrpDiv->dl())) {
    DoubleVector dv(GivenLgrpDiv->numLengthGroups() + 1);
    for (i = 0; i < dv.Size() - 1; i++)
      dv[i] = GivenLgrpDiv->minLength(i);
    dv[i] = GivenLgrpDiv->maxLength(i - 1);
    LgrpDiv = new LengthGroupDivision(dv);
  } else
    LgrpDiv = new LengthGroupDivision(*GivenLgrpDiv);

  CI = new ConversionIndex(OtherLgrpDiv, LgrpDiv);
}

PopPredator::~PopPredator() {
  delete LgrpDiv;
  delete CI;
}

void PopPredator::Print(ofstream& outfile) const {
  Predator::Print(outfile);
  int i, area;

  outfile << TAB;
  LgrpDiv->Print(outfile);
  for (area = 0; area < areas.Size(); area++) {
    outfile << "\tNumber of predators on internal area " << areas[area] << ":\n\t";
    for (i = 0; i < LgrpDiv->numLengthGroups(); i++) {
      outfile.precision(smallprecision);
      outfile.width(smallwidth);
      outfile << sep << prednumber[area][i].N;
    }
    outfile << "\n\tWeight of predators on internal area " << areas[area] << ":\n\t";
    for (i = 0; i < LgrpDiv->numLengthGroups(); i++) {
      outfile.precision(smallprecision);
      outfile.width(smallwidth);
      outfile << sep << prednumber[area][i].W;
    }
    outfile << "\n\tTotal amount eaten on internal area " << areas[area] << ":\n\t";
    for (i = 0; i < LgrpDiv->numLengthGroups(); i++) {
      outfile.precision(smallprecision);
      outfile.width(smallwidth);
      outfile << sep << totalconsumption[area][i];
    }
    outfile << "\n\tOverconsumption on internal area " << areas[area] << ":\n\t";
    for (i = 0; i < LgrpDiv->numLengthGroups(); i++) {
      outfile.precision(smallprecision);
      outfile.width(smallwidth);
      outfile << sep << overconsumption[area][i];
    }
    outfile << endl;
  }
}

const BandMatrix& PopPredator::Consumption(int area, const char* preyname) const {
  int prey;
  for (prey = 0; prey < this->numPreys(); prey++)
    if (strcasecmp(Preyname(prey), preyname) == 0)
      return consumption[this->areaNum(area)][prey];

  handle.logFailure("Error in poppredator - failed to match prey", preyname);
  exit(EXIT_FAILURE);
}

const double PopPredator::getConsumptionBiomass(int prey, int area) const{
  int age, len;
  double kilos = 0.0;
  //Note area is already the internal area ...
  const BandMatrix& bio = consumption[area][prey];
  for (age = bio.minAge(); age <= bio.maxAge(); age++)
    for (len = bio.minLength(age); len < bio.maxLength(age); len++)
      kilos += bio[age][len];

  return kilos;
}

void PopPredator::DeleteParametersForPrey(int prey, Keeper* const keeper) {
  Predator::DeleteParametersForPrey(prey, keeper);
}

void PopPredator::Reset(const TimeClass* const TimeInfo) {
  Predator::Reset(TimeInfo);
  //Now the matrices Suitability(prey) are of the correct size.
  //We must adjust the size of consumption accordingly.
  int i, j, area, prey;
  for (area = 0; area < areas.Size(); area++) {
    for (prey = 0; prey < this->numPreys(); prey++) {
      if (this->DidChange(prey, TimeInfo)) {
        //adjust the size of consumption[area][prey].
        cons.ChangeElement(area, prey, Suitability(prey));
        consumption.ChangeElement(area, prey, Suitability(prey));
        for (i = 0; i < Suitability(prey).Nrow(); i++)
          for (j = consumption[area][prey].minCol(i); j < consumption[area][prey].maxCol(i); j++)
            consumption[area][prey][i][j] = 0.0;
      }
    }
  }

  if (TimeInfo->CurrentSubstep() == 1) {
    for (area = 0; area < areas.Size(); area++) {
      for (i = 0; i < LgrpDiv->numLengthGroups(); i++) {
        prednumber[area][i].N = 0.0;
        prednumber[area][i].W = 0.0;
        overconsumption[area][i] = 0.0;
        totalconsumption[area][i] = 0.0;
        for (prey = 0; prey < this->numPreys(); prey++)
          for (j = consumption[area][prey].minCol(i); j < consumption[area][prey].maxCol(i); j++)
            consumption[area][prey][i][j] = 0.0;

      }
    }
  }

  handle.logMessage("Reset predatation data for predator", this->getName());
}

void PopPredator::resizeObjects() {
  Predator::resizeObjects();
  while (consumption.Nrow())
    consumption.DeleteRow(0);
  while (cons.Nrow())
    cons.DeleteRow(0);
  while (totalconsumption.Nrow())
    totalcons.DeleteRow(0);
  while (totalcons.Nrow())
    totalconsumption.DeleteRow(0);
  while (prednumber.Nrow())
    prednumber.DeleteRow(0);

  PopInfo nullpop;
  //Add rows to matrices and initialise
  cons.AddRows(areas.Size(), this->numPreys());
  totalcons.AddRows(areas.Size(), LgrpDiv->numLengthGroups(), 0.0);
  overcons.AddRows(areas.Size(), LgrpDiv->numLengthGroups(), 0.0);
  consumption.AddRows(areas.Size(), this->numPreys());
  totalconsumption.AddRows(areas.Size(), LgrpDiv->numLengthGroups(), 0.0);
  overconsumption.AddRows(areas.Size(), LgrpDiv->numLengthGroups(), 0.0);
  prednumber.AddRows(areas.Size(), LgrpDiv->numLengthGroups(), nullpop);
}

double PopPredator::getTotalOverConsumption(int area) const {
  int i, inarea = this->areaNum(area);
  double total;
  total = 0.0;
  for (i = 0; i < LgrpDiv->numLengthGroups(); i++)
    total += overconsumption[inarea][i];
  return total;
}
