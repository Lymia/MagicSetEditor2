//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2012 Twan van Laarhoven and Sean Hunt             |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#ifndef HEADER_GUI_CONTROL_FILTERED_CARD_LIST
#define HEADER_GUI_CONTROL_FILTERED_CARD_LIST

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gui/control/card_list.hpp>
#include <data/filter.hpp>

typedef intrusive_ptr<Filter<Card> > CardListFilterP;

// ----------------------------------------------------------------------------- : FilteredCardList

/// A card list that lists a subset of the cards in the set
class FilteredCardList : public CardListBase {
  public:
	FilteredCardList(Window* parent, int id, long additional_style = 0);
	
	/// Change the filter to use
	void setFilter(const CardListFilterP& filter);
	
  protected:
	/// Get only the subset of the cards
	virtual void getItems(vector<VoidP>& out) const;
	
	virtual void onChangeSet();
	
  private:	
	CardListFilterP filter;	///< Filter with which this.cards is made
};


// ----------------------------------------------------------------------------- : EOF
#endif
