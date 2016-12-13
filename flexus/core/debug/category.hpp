// DO-NOT-REMOVE begin-copyright-block 
//
// Redistributions of any form whatsoever must retain and/or include the
// following acknowledgment, notices and disclaimer:
//
// This product includes software developed by Carnegie Mellon University.
//
// Copyright 2012 by Mohammad Alisafaee, Eric Chung, Michael Ferdman, Brian 
// Gold, Jangwoo Kim, Pejman Lotfi-Kamran, Onur Kocberber, Djordje Jevdjic, 
// Jared Smolens, Stephen Somogyi, Evangelos Vlachos, Stavros Volos, Jason 
// Zebchuk, Babak Falsafi, Nikos Hardavellas and Tom Wenisch for the SimFlex 
// Project, Computer Architecture Lab at Carnegie Mellon, Carnegie Mellon University.
//
// For more information, see the SimFlex project website at:
//   http://www.ece.cmu.edu/~simflex
//
// You may not use the name "Carnegie Mellon University" or derivations
// thereof to endorse or promote products derived from this software.
//
// If you modify the software you must place a notice on or within any
// modified version provided or made available to any third party stating
// that you have modified the software.  The notice shall include at least
// your name, address, phone number, email address and the date and purpose
// of the modification.
//
// THE SOFTWARE IS PROVIDED "AS-IS" WITHOUT ANY WARRANTY OF ANY KIND, EITHER
// EXPRESS, IMPLIED OR STATUTORY, INCLUDING BUT NOT LIMITED TO ANY WARRANTY
// THAT THE SOFTWARE WILL CONFORM TO SPECIFICATIONS OR BE ERROR-FREE AND ANY
// IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE,
// TITLE, OR NON-INFRINGEMENT.  IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY
// BE LIABLE FOR ANY DAMAGES, INCLUDING BUT NOT LIMITED TO DIRECT, INDIRECT,
// SPECIAL OR CONSEQUENTIAL DAMAGES, ARISING OUT OF, RESULTING FROM, OR IN
// ANY WAY CONNECTED WITH THIS SOFTWARE (WHETHER OR NOT BASED UPON WARRANTY,
// CONTRACT, TORT OR OTHERWISE).
//
// DO-NOT-REMOVE end-copyright-block   
#ifndef FLEXUS_CORE_DEBUG_CATEGORY_HPP_INCLUDED
#define FLEXUS_CORE_DEBUG_CATEGORY_HPP_INCLUDED

#include <utility>
#include <vector>
#include <string>
#include <map>

namespace Flexus {
namespace Dbg {

using namespace std::rel_ops;

class Category;

class CategoryHolder {
private:
  std::vector<Category const *> theCategories;
public:
  CategoryHolder(Category const & aCategory) {
    theCategories.push_back(&aCategory);
  }

  CategoryHolder(Category const & aFirstCategory, Category const & aSecondCategory) {
    theCategories.push_back(&aFirstCategory);
    theCategories.push_back(&aSecondCategory);
  }

  CategoryHolder & operator | (CategoryHolder const & aHolder) {
    theCategories.insert(theCategories.end(), aHolder.theCategories.begin(), aHolder.theCategories.end());
    return *this;
  }

  CategoryHolder & operator | (Category const & aCategory) {
    theCategories.push_back(&aCategory);
    return *this;
  }

  typedef std::vector<Category const *>::const_iterator const_iterator;

  const_iterator begin() const {
    return theCategories.begin();
  }
  const_iterator end() const {
    return theCategories.end();
  }
};

class Category {
private:
  std::string theName;
  int32_t theNumber;
  bool theIsDynamic;

public:
  Category(std::string const & aName, bool * aSwitch, bool aIsDynamic = false);

  std::string const & name() const {
    return theName;
  }

  int32_t number() const {
    return theNumber;
  }

  bool isDynamic() const {
    return theIsDynamic;
  }

  bool operator ==(Category const & aCategory) {
    return (theNumber == aCategory.theNumber);
  }

  bool operator <(Category const & aCategory) {
    return (theNumber < aCategory.theNumber);
  }

  CategoryHolder operator | (Category const & aCategory) {
    return CategoryHolder(*this, aCategory);
  }
};

class CategoryMgr {
  std::map<std::string, Category * > theCategories;
  int32_t theCatCount;
public:
  CategoryMgr()
    : theCatCount(0)
  {}
  ~CategoryMgr() {
    std::map<std::string, Category * >::iterator iter = theCategories.begin();
    while (iter != theCategories.end()) {
      if ((*iter).second->isDynamic()) {
        delete (*iter).second;
      }
      ++iter;
    }
  }

  static CategoryMgr & categoryMgr() {
    static CategoryMgr theStaticCategoryMgr;
    return theStaticCategoryMgr;
  }

  Category const & category(std::string const & aCategory) {
    Category * & cat = theCategories[aCategory];
    if (cat == 0) {
      cat = new Category(aCategory, 0, true);
    }
    return *cat;
  }

  int32_t addCategory(Category & aCategory) {
    int32_t cat_num = 0;
    Category * & cat = theCategories[aCategory.name()];
    if (cat != 0) {
      cat_num = cat->number();
    } else {
      cat_num = ++theCatCount;
      cat = & aCategory;
    }
    return cat_num;
  }

};

} //Dbg
} //Flexus

#endif //FLEXUS_CORE_DEBUG_CATEGORY_HPP_INCLUDED

