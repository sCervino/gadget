#include "catchstatistics.h"
#include "commentstream.h"
#include "readfunc.h"
#include "readword.h"
#include "errorhandler.h"
#include "areatime.h"
#include "fleet.h"
#include "stock.h"
#include "stockprey.h"
#include "popstatistics.h"
#include "readaggregation.h"
#include "gadget.h"

extern ErrorHandler handle;

CatchStatistics::CatchStatistics(CommentStream& infile, const AreaClass* const Area,
  const TimeClass* const TimeInfo, double weight, const char* name)
  : Likelihood(CATCHSTATISTICSLIKELIHOOD, weight, name) {

  char text[MaxStrLength];
  strncpy(text, "", MaxStrLength);
  int i, j;
  int numarea = 0, numage = 0;

  char datafilename[MaxStrLength];
  char aggfilename[MaxStrLength];
  strncpy(datafilename, "", MaxStrLength);
  strncpy(aggfilename, "", MaxStrLength);
  ifstream datafile;
  CommentStream subdata(datafile);

  timeindex = 0;
  functionname = new char[MaxStrLength];
  strncpy(functionname, "", MaxStrLength);
  readWordAndValue(infile, "datafile", datafilename);
  readWordAndValue(infile, "function", functionname);
  readWordAndVariable(infile, "overconsumption", overconsumption);
  if (overconsumption != 0 && overconsumption != 1)
    handle.Message("Error in catchstatistics - overconsumption must be 0 or 1");

  if ((strcasecmp(functionname, "lengthcalcvar") == 0) || (strcasecmp(functionname, "lengthcalcstddev") == 0))
    functionnumber = 1;
  else if ((strcasecmp(functionname, "lengthgivenvar") == 0) || (strcasecmp(functionname, "lengthgivenstddev") == 0))
    functionnumber = 2;
  else if ((strcasecmp(functionname, "weightgivenvar") == 0) || (strcasecmp(functionname, "weightgivenstddev") == 0))
    functionnumber = 3;
  else if ((strcasecmp(functionname, "weightnovar") == 0) || (strcasecmp(functionname, "weightnostddev") == 0))
    functionnumber = 4;
  else if ((strcasecmp(functionname, "lengthnovar") == 0) || (strcasecmp(functionname, "lengthnostddev") == 0))
    functionnumber = 5;
  else
    handle.Message("Error in catchstatistics - unrecognised function", functionname);

  //read in area aggregation from file
  readWordAndValue(infile, "areaaggfile", aggfilename);
  datafile.open(aggfilename, ios::in);
  handle.checkIfFailure(datafile, aggfilename);
  handle.Open(aggfilename);
  numarea = readAggregation(subdata, areas, areaindex);
  handle.Close();
  datafile.close();
  datafile.clear();

  //read in age aggregation from file
  readWordAndValue(infile, "ageaggfile", aggfilename);
  datafile.open(aggfilename, ios::in);
  handle.checkIfFailure(datafile, aggfilename);
  handle.Open(aggfilename);
  numage = readAggregation(subdata, ages, ageindex);
  handle.Close();
  datafile.close();
  datafile.clear();

  //Must change from outer areas to inner areas.
  for (i = 0; i < areas.Nrow(); i++)
    for (j = 0; j < areas.Ncol(i); j++)
      if ((areas[i][j] = Area->InnerArea(areas[i][j])) == -1)
        handle.UndefinedArea(areas[i][j]);

  //read in the fleetnames
  i = 0;
  infile >> text >> ws;
  if (!(strcasecmp(text, "fleetnames") == 0))
    handle.Unexpected("fleetnames", text);
  infile >> text >> ws;
  while (!infile.eof() && !(strcasecmp(text, "stocknames") == 0)) {
    fleetnames.resize(1);
    fleetnames[i] = new char[strlen(text) + 1];
    strcpy(fleetnames[i++], text);
    infile >> text >> ws;
  }

  //read in the stocknames
  i = 0;
  if (!(strcasecmp(text, "stocknames") == 0))
    handle.Unexpected("stocknames", text);
  infile >> text;
  while (!infile.eof() && !(strcasecmp(text, "[component]") == 0)) {
    infile >> ws;
    stocknames.resize(1);
    stocknames[i] = new char[strlen(text) + 1];
    strcpy(stocknames[i++], text);
    infile >> text;
  }

  //We have now read in all the data from the main likelihood file
  //But we have to read in the statistics data from datafilename
  datafile.open(datafilename, ios::in);
  handle.checkIfFailure(datafile, datafilename);
  handle.Open(datafilename);
  readStatisticsData(subdata, TimeInfo, numarea, numage);
  handle.Close();
  datafile.close();
  datafile.clear();
}

void CatchStatistics::readStatisticsData(CommentStream& infile,
  const TimeClass* TimeInfo, int numarea, int numage) {

  int i, year, step;
  double tmpnumber, tmpmean, tmpstddev;
  char tmparea[MaxStrLength], tmpage[MaxStrLength];
  strncpy(tmparea, "", MaxStrLength);
  strncpy(tmpage, "", MaxStrLength);
  int keepdata, needvar, readvar;
  int timeid, ageid, areaid;
  int count = 0;

  //Find start of statistics data in datafile
  infile >> ws;
  char c = infile.peek();
  if (!isdigit(c)) {
    infile.get(c);
    while (c != '\n' && !infile.eof())
      infile.get(c);
  }

  readvar = 0;
  needvar = 0;
  switch(functionnumber) {
    case 1:
      readvar = 0;
      needvar = 1;
      break;
    case 2:
    case 3:
      readvar = 1;
      needvar = 1;
      break;
    case 4:
    case 5:
      break;
    default:
      handle.logWarning("Warning in catchstatistics - unrecognised function", functionname);
      break;
  }

  //Check the number of columns in the inputfile
  if ((readvar == 1) && (countColumns(infile) != 7))
    handle.Message("Wrong number of columns in inputfile - should be 7");
  else if ((readvar == 0) && (countColumns(infile) != 6))
    handle.Message("Wrong number of columns in inputfile - should be 6");

  while (!infile.eof()) {
    keepdata = 0;
    if (readvar == 1)
      infile >> year >> step >> tmparea >> tmpage >> tmpnumber >> tmpmean >> tmpstddev >> ws;
    else
      infile >> year >> step >> tmparea >> tmpage >> tmpnumber >> tmpmean >> ws;

    //if tmparea is in areaindex find areaid, else dont keep the data
    areaid = -1;
    for (i = 0; i < areaindex.Size(); i++)
      if (strcasecmp(areaindex[i], tmparea) == 0)
        areaid = i;

    if (areaid == -1)
      keepdata = 1;

    //if tmpage is in ageindex find ageid, else dont keep the data
    ageid = -1;
    for (i = 0; i < ageindex.Size(); i++)
      if (strcasecmp(ageindex[i], tmpage) == 0)
        ageid = i;

    if (ageid == -1)
      keepdata = 1;

    //check if the year and step are in the simulation
    timeid = -1;
    if ((TimeInfo->isWithinPeriod(year, step)) && (keepdata == 0)) {
      //if this is a new timestep, resize to store the data
      for (i = 0; i < Years.Size(); i++)
        if ((Years[i] == year) && (Steps[i] == step))
          timeid = i;

      if (timeid == -1) {
        Years.resize(1, year);
        Steps.resize(1, step);
        likelihoodValues.AddRows(1, numarea, 0.0);
        numbers.resize(1, new DoubleMatrix(numarea, numage, 0.0));
        obsMean.resize(1, new DoubleMatrix(numarea, numage, 0.0));
        modelMean.resize(1, new DoubleMatrix(numarea, numage, 0.0));
        if (readvar == 1)
          obsStdDev.resize(1, new DoubleMatrix(numarea, numage, 0.0));
        if (needvar == 1)
          modelStdDev.resize(1, new DoubleMatrix(numarea, numage, 0.0));
        timeid = (Years.Size() - 1);
      }

    } else {
      //dont keep the data
      keepdata = 1;
    }

    if (keepdata == 0) {
      //statistics data is required, so store it
      count++;
      (*numbers[timeid])[areaid][ageid] = tmpnumber;
      (*obsMean[timeid])[areaid][ageid] = tmpmean;
      if (readvar == 1)
        (*obsStdDev[timeid])[areaid][ageid] = tmpstddev;
    }
  }
  AAT.addActions(Years, Steps, TimeInfo);
  if (count == 0)
    handle.logWarning("Warning in catchstatistics - found no data in the data file for", this->Name());
  handle.logMessage("Read catchstatistics data file - number of entries", count);
}

CatchStatistics::~CatchStatistics() {
  int i;
  for (i = 0; i < stocknames.Size(); i++)
    delete[] stocknames[i];
  for (i = 0; i < fleetnames.Size(); i++)
    delete[] fleetnames[i];
  for (i = 0; i < areaindex.Size(); i++)
    delete[] areaindex[i];
  for (i = 0; i < ageindex.Size(); i++)
    delete[] ageindex[i];
  delete aggregator;
  for (i = 0; i < numbers.Size(); i++) {
    delete numbers[i];
    delete obsMean[i];
    delete modelMean[i];
  }
  for (i = 0; i < obsStdDev.Size(); i++)
    delete obsStdDev[i];
  for (i = 0; i < modelStdDev.Size(); i++)
    delete modelStdDev[i];
  delete[] functionname;
  delete LgrpDiv;
}

void CatchStatistics::Reset(const Keeper* const keeper) {
  Likelihood::Reset(keeper);
  timeindex = 0;
  handle.logMessage("Reset catchstatistics component", this->Name());
}

void CatchStatistics::Print(ofstream& outfile) const {
  int i;

  outfile << "\nCatch Statistics " << this->Name() << " - likelihood value " << likelihood
    << "\n\tFunction " << functionname << "\n\tStock names:";
  for (i = 0; i < stocknames.Size(); i++)
    outfile << sep << stocknames[i];
  outfile << "\n\tFleet names:";
  for (i = 0; i < fleetnames.Size(); i++)
    outfile << sep << fleetnames[i];
  outfile << endl;
  aggregator->Print(outfile);
  outfile.flush();
}

void CatchStatistics::setFleetsAndStocks(FleetPtrVector& Fleets, StockPtrVector& Stocks) {
  int i, j, found;
  FleetPtrVector fleets;
  StockPtrVector stocks;

  for (i = 0; i < fleetnames.Size(); i++) {
    found = 0;
    for (j = 0; j < Fleets.Size(); j++)
      if (strcasecmp(fleetnames[i], Fleets[j]->Name()) == 0) {
        found++;
        fleets.resize(1, Fleets[j]);
      }

    if (found == 0)
      handle.logFailure("Error in catchstatistics - unrecognised fleet", fleetnames[i]);

  }

  for (i = 0; i < stocknames.Size(); i++) {
    found = 0;
    for (j = 0; j < Stocks.Size(); j++)
      if (Stocks[j]->isEaten())
        if (strcasecmp(stocknames[i], Stocks[j]->returnPrey()->Name()) == 0) {
          found++;
          stocks.resize(1, Stocks[j]);
        }

    if (found == 0)
      handle.logFailure("Error in catchstatistics - unrecognised stock", stocknames[i]);

  }

  LgrpDiv = new LengthGroupDivision(*(stocks[0]->returnPrey()->returnLengthGroupDiv()));
  for (i = 1; i < stocks.Size(); i++)
    if (!LgrpDiv->Combine(stocks[i]->returnPrey()->returnLengthGroupDiv()))
      handle.logFailure("Error in catchstatistics - length groups for preys not compatible");

  aggregator = new FleetPreyAggregator(fleets, stocks, LgrpDiv, areas, ages, overconsumption);
}

void CatchStatistics::addLikelihood(const TimeClass* const TimeInfo) {

  double l = 0.0;
  if (AAT.AtCurrentTime(TimeInfo)) {
    handle.logMessage("Calculating likelihood score for catchstatistics component", this->Name());
    aggregator->Sum(TimeInfo);
    if (aggregator->checkCatchData() == 1)
      handle.logWarning("Warning in catchstatistics - zero catch found");

    l = calcLikSumSquares();
    likelihood += l;
    timeindex++;
    handle.logMessage("The likelihood score for this component on this timestep is", l);
  }
}

double CatchStatistics::calcLikSumSquares() {

  int nareas, age;
  double lik, totallikelihood, simvar, simdiff;

  lik = totallikelihood = simvar = simdiff = 0.0;
  const AgeBandMatrixPtrVector *alptr = &aggregator->returnSum();

  for (nareas = 0; nareas < alptr->Size(); nareas++) {
    likelihoodValues[timeindex][nareas] = 0.0;
    for (age = 0; age < (*alptr)[nareas].Nrow(); age++) {
      PopStatistics PopStat((*alptr)[nareas][age], aggregator->returnLengthGroupDiv());

      switch(functionnumber) {
        case 1:
          (*modelMean[timeindex])[nareas][age] = PopStat.meanLength();
          simvar = PopStat.sdevLength() * PopStat.sdevLength();
          (*modelStdDev[timeindex])[nareas][age] = PopStat.sdevLength();
          break;
        case 2:
          (*modelMean[timeindex])[nareas][age] = PopStat.meanLength();
          simvar = (*obsStdDev[timeindex])[nareas][age] * (*obsStdDev[timeindex])[nareas][age];
          break;
        case 3:
          (*modelMean[timeindex])[nareas][age] = PopStat.meanWeight();
          simvar = (*obsStdDev[timeindex])[nareas][age] * (*obsStdDev[timeindex])[nareas][age];
          break;
        case 4:
          (*modelMean[timeindex])[nareas][age] = PopStat.meanWeight();
          simvar = 1.0;
          break;
        case 5:
          (*modelMean[timeindex])[nareas][age] = PopStat.meanLength();
          simvar = 1.0;
          break;
        default:
          handle.logWarning("Warning in catchstatistics - unrecognised function", functionname);
          break;
      }

      simdiff = (*modelMean[timeindex])[nareas][age] - (*obsMean[timeindex])[nareas][age];
      if (isZero(simvar))
        lik = 0.0;
      else
        lik = simdiff * simdiff * (*numbers[timeindex])[nareas][age] / simvar;

      likelihoodValues[timeindex][nareas] += lik;
    }
    totallikelihood += likelihoodValues[timeindex][nareas];
  }
  return totallikelihood;
}

void CatchStatistics::LikelihoodPrint(ofstream& outfile, const TimeClass* const TimeInfo) {

  if (!AAT.AtCurrentTime(TimeInfo))
    return;

  int t, area, age;
  t = timeindex - 1; //timeindex was increased before this is called

  if ((t >= Years.Size()) || t < 0)
    handle.logFailure("Error in catchstatistcs - invalid timestep", t);

  for (area = 0; area < (*modelMean[t]).Nrow(); area++) {
    for (age = 0; age < (*modelMean[t]).Ncol(area); age++) {
      outfile << setw(lowwidth) << Years[t] << sep << setw(lowwidth)
        << Steps[t] << sep << setw(printwidth) << areaindex[area] << sep
        << setw(printwidth) << ageindex[age] << sep << setprecision(printprecision)
        << setw(printwidth) << (*numbers[t])[area][age] << sep
        << setprecision(largeprecision) << setw(largewidth) << (*modelMean[t])[area][age];
      switch(functionnumber) {
        case 1:
          outfile << sep << setprecision(printprecision) << setw(printwidth)
            << (*modelStdDev[t])[area][age] << endl;
          break;
        case 2:
        case 3:
          outfile << sep << setprecision(printprecision) << setw(printwidth)
            << (*obsStdDev[t])[area][age] << endl;
          break;
        case 4:
        case 5:
          outfile << endl;
        default:
          handle.logWarning("Warning in catchstatistics - unrecognised function", functionname);
          break;
      }
    }
  }
}

void CatchStatistics::SummaryPrint(ofstream& outfile) {
  int year, area;

  for (year = 0; year < likelihoodValues.Nrow(); year++)
    for (area = 0; area < likelihoodValues.Ncol(year); area++)
      outfile << setw(lowwidth) << Years[year] << sep << setw(lowwidth)
        << Steps[year] << sep << setw(printwidth) << areaindex[area] << sep
        << setw(largewidth) << this->Name() << sep << setprecision(smallprecision)
        << setw(smallwidth) << weight << sep << setprecision(largeprecision)
        << setw(largewidth) << likelihoodValues[year][area] << endl;

  outfile.flush();
}
