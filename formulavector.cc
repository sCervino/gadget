#include "formulavector.h"

FormulaVector::FormulaVector(const FormulaVector& initial) {
  int i;
  size = initial.size;
  if (size > 0) {
    v = new Formula[size];
    for (i = 0; i < size; i++)
      v[i] = initial.v[i];
  } else
    v = 0;
}

FormulaVector::FormulaVector(int sz) {
  size = (sz > 0 ? sz : 0);
  if (size > 0)
    v = new Formula[size];
  else
    v = 0;
}

FormulaVector::~FormulaVector() {
  if (v != 0) {
    delete[] v;
    v = 0;
  }
}

void FormulaVector::resize(int addsize, Keeper* keeper) {
  int i;
  if (v == 0) {
    size = addsize;
    v = new Formula[size];
  } else if (addsize > 0) {
    Formula* vnew = new Formula[addsize + size];
    for (i = 0; i < size; i++)
      v[i].Interchange(vnew[i], keeper);
    delete[] v;
    v = vnew;
    size += addsize;
  }
}

void FormulaVector::Inform(Keeper* keeper) {
  int i;
  for (i = 0; i < size; i++)
    v[i].Inform(keeper);
}

CommentStream& operator >> (CommentStream& infile, FormulaVector& fv) {
  if (infile.fail()) {
    infile.makebad();
    return infile;
  }
  int i;
  for (i = 0; i < fv.size; i++) {
    if (!(infile >> fv[i])) {
      infile.makebad();
      return infile;
    }
  }
  return infile;
}
