//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2012 Twan van Laarhoven and Sean Hunt             |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gui/control/graph.hpp>
#include <util/alignment.hpp>
#include <gfx/gfx.hpp>
#include <wx/dcbuffer.h>
#include <wx/tooltip.h>

DECLARE_TYPEOF_COLLECTION(GraphAxisP);
DECLARE_TYPEOF_COLLECTION(GraphElementP);
DECLARE_TYPEOF_COLLECTION(GraphGroup);
DECLARE_TYPEOF_COLLECTION(GraphDataElement*);
DECLARE_TYPEOF_COLLECTION(GraphP);
DECLARE_TYPEOF_COLLECTION(int);
DECLARE_TYPEOF_COLLECTION(vector<int>);
DECLARE_TYPEOF_COLLECTION(String);
DECLARE_TYPEOF_COLLECTION(UInt);
DECLARE_TYPEOF_COLLECTION(pair<String COMMA String>);

template <typename T> inline T sgn(T v) { return v < 0 ? -1 : 1; }

// ----------------------------------------------------------------------------- : Events

DEFINE_EVENT_TYPE(EVENT_GRAPH_SELECT);

// ----------------------------------------------------------------------------- : GraphAxis

void GraphAxis::addGroup(const String& name, UInt size) {
	if (!groups.empty() && groups.back().name == name) {
		groups.back().size += size;
	} else {
		groups.push_back(GraphGroup(name, size));
	}
	max = std::max(max, groups.back().size);
	total += size;
}

// ----------------------------------------------------------------------------- : GraphData

struct ComparingOriginalIndex {
	inline bool operator () (const GraphElementP& a, const GraphElementP& b) {
		return a->original_index < b->original_index;
	}
};

void GraphDataPre::splitList(size_t axis) {
	size_t count = elements.size(); // only the elements that were already there
	for (size_t i = 0 ; i < count ; ++i) {
		GraphElement& e = *elements[i];
		String& v = e.values[axis];
		size_t comma = v.find_first_of(_(','));
		while (comma != String::npos) {
			// split
			GraphElementP e2(new GraphElement(e));
			e2->values[axis] = v.substr(0,comma);
			elements.push_back(e2);
			if (is_substr(v, comma, _(", "))) ++comma; // skip space after it
			v = v.substr(comma + 1);
			comma = v.find_first_of(_(','));
		}
	}
	// re-sort by original_index
	sort(elements.begin(), elements.end(), ComparingOriginalIndex());
}


struct SmartLess{
	inline bool operator () (const String& a, const String& b) const { return smart_less(a,b); }
};
DECLARE_TYPEOF(map<String COMMA UInt COMMA SmartLess>);

String to_bin(double value, double bin_size) {
	if (bin_size <= 0 || value == 0) {
		return String() << (int)value;
	} else {
		int bin = ceil(value / bin_size);
		return String::Format(_("%.0f%c%.0f"), (bin-1) * bin_size + 1, EN_DASH, bin * bin_size);
	}
}
int bin_to_group(double value, double bin_size) {
	if (bin_size <= 0 || value == 0) {
		return 0;
	} else {
		return ceil(value / bin_size);
	}
}

GraphData::GraphData(const GraphDataPre& d)
	: axes(d.axes)
{
	// find groups on each axis
	size_t i = 0;
	FOR_EACH(a, axes) {
		map<String,UInt,SmartLess> counts; // note: default constructor for UInt() does initialize to 0
		FOR_EACH_CONST(e, d.elements) {
			counts[e->values[i]] += 1;
		}
		if (a->numeric) {
			// Add all values, calculate mean of the numeric ones
			UInt numeric_count = 0;
			int prev = 0;
			FOR_EACH(c, counts) {
				// numeric?
				double d;
				if (c.first.ToDouble(&d)) {
					// update mean
					a->mean_value += d * c.second;
					a->max_value  = max(a->max_value, d);
					numeric_count += c.second;
					// add 0 bars before this value
					int next = (int)floor(d);
					for (int i = prev ; i < next ; i++) {
						a->addGroup(to_bin(i, a->bin_size), 0);
					}
					prev = next + 1;
					// add
					if (a->bin_size) {
						a->addGroup(to_bin(d, a->bin_size), c.second);
					} else {
						a->addGroup(c.first, c.second);
					}
				} else {
					// non-numeric, add anyway
					a->addGroup(c.first, c.second);
				}
			}
			a->mean_value /= numeric_count;
		} else if (a->order) {
			// specific group order
			FOR_EACH_CONST(gn, *a->order) {
				a->addGroup(gn, counts[gn]);
			}
		} else {
			FOR_EACH(c, counts) {
				a->addGroup(c.first, c.second);
			}
		}
		// colors
		if (a->auto_color == AUTO_COLOR_NO && a->colors) {
			// use colors from the table
			FOR_EACH(g, a->groups) {
				map<String,Color>::const_iterator it = a->colors->find(g.name);
				if (it != a->colors->end()) {
					g.color = it->second;
				}
			}
		} else {
			// find some nice colors for the groups
			double step = 0;
			bool first = true;
			FOR_EACH(g, a->groups) {
				double amount = a->auto_color == AUTO_COLOR_EVEN
				                  ? 1. / a->groups.size()
				                  : double(g.size) / a->total; // amount this group takes
				if (!first) step += amount/2;
				if (a->numeric) {
					g.color = hsl2rgb(0.65 - 0.82 * step, 0.9 - 0.2 * fabs(step - 0.5), 0.3 + 0.35 * step);
				} else {
					g.color = hsl2rgb(0.6 + step, 0.9, 0.5);
				}
				step += amount / 2;
				first = false;
			}
		}
		++i;
	}
	// count elements in each position
	values.reserve(d.elements.size());
	size_t de_size = sizeof(GraphDataElement) + sizeof(int) * (axes.size() - 1);
	FOR_EACH_CONST(e, d.elements) {
		// make the group_nrs large enough
		GraphDataElement* de = reinterpret_cast<GraphDataElement*>(new char[de_size]);
		de->original_index = e->original_index;
		// find index j in elements
		int i = 0;
		FOR_EACH(a, axes) {
			String v = e->values[i];
			de->group_nrs[i] = -1;
			double d;
			if (a->numeric && a->bin_size > 0 && v.ToDouble(&d)) {
				// calculate group that contains v
				de->group_nrs[i] = bin_to_group(d, a->bin_size);
			} else {
				// find group that contains v
				int j = 0;
				FOR_EACH(g, a->groups) {
					if (v == g.name) {
						de->group_nrs[i] = j;
						break;
					}
					++j;
				}
			}
			++i;
		}
		values.push_back(de);
	}
}

GraphData::~GraphData() {
	FOR_EACH_CONST(v,values) delete v;
}

void GraphData::crossAxis(size_t axis1, size_t axis2, vector<UInt>& out) const {
	size_t a1_size = axes[axis1]->groups.size();
	size_t a2_size = axes[axis2]->groups.size();
	out.clear();
	out.resize(a1_size * a2_size, 0);
	FOR_EACH_CONST(v, values) {
		int v1 = v->group_nrs[axis1], v2 = v->group_nrs[axis2];
		if (v1 >= 0 && v2 >= 0) {
			out[a2_size * v1 + v2]++;
		}
	}
}

void GraphData::crossAxis(size_t axis1, size_t axis2, size_t axis3, vector<UInt>& out) const {
	size_t a1_size = axes[axis1]->groups.size();
	size_t a2_size = axes[axis2]->groups.size();
	size_t a3_size = axes[axis3]->groups.size();
	out.clear();
	out.resize(a1_size * a2_size * a3_size, 0);
	FOR_EACH_CONST(v, values) {
		int v1 = v->group_nrs[axis1], v2 = v->group_nrs[axis2], v3 = v->group_nrs[axis3];
		if (v1 >= 0 && v2 >= 0 && v3 >= 0) {
			out[a3_size * (a2_size * v1 + v2) + v3]++;
		}
	}
}

bool matches(const GraphDataElement* v, const vector<int>& match) {
	for (size_t i = 0 ; i < match.size() ; ++i) {
		if (v->group_nrs[i] == -1 || (match[i] != -1 && v->group_nrs[i] != match[i])) {
			return false;
		}
	}
	return true;
}

UInt GraphData::count(const vector<int>& match) const {
	if (match.size() != axes.size()) return 0;
	UInt count = 0;
	size_t prev_index = (size_t)-1;
	FOR_EACH_CONST(v, values) {
		if (matches(v, match) && v->original_index != prev_index) {
			prev_index = v->original_index; // don't count the same index twice
			count += matches(v, match);
		}
	}
	return count;
}

void GraphData::indices(const vector<int>& match, vector<size_t>& out) const {
	if (match.size() != axes.size()) return;
	size_t prev_index = (size_t)-1;
	FOR_EACH_CONST(v, values) {
		if (matches(v, match) && v->original_index != prev_index) {
			prev_index = v->original_index; // don't select the same index twice
			out.push_back(v->original_index);
		}
	}
}

// ----------------------------------------------------------------------------- : Graph1D

void Graph1D::draw(RotatedDC& dc, const vector<int>& current, DrawLayer layer) const {
	draw(dc, axis < current.size() ? current.at(axis) : -1, layer);
}
bool Graph1D::findItem(const RealPoint& pos, const RealRect& screen_rect, bool tight, vector<int>& out) const {
	int i = findItem(pos, screen_rect, tight);
	if (i == -1) return false;
	else {
		out.clear();
		out.insert(out.begin(), data->axes.size(), -1);
		out.at(axis) = i;
		return true;
	}
}

void Graph1D::setData(const GraphDataP& d) {
	if (d->axes.size() <= axis) {
		Graph::setData(GraphDataP()); // invalid
	} else {
		Graph::setData(d);
	}
	
}

// ----------------------------------------------------------------------------- : Graph2D

void Graph2D::setData(const GraphDataP& d) {
	if (d->axes.size() <= max(axis1,axis2)) {
		Graph::setData(GraphDataP()); // invalid
	} else {
		Graph::setData(d);
		d->crossAxis(axis1,axis2,values);
	}
}

// ----------------------------------------------------------------------------- : Bar Graph

/// Rectangle for the bar of a bar graph
RealRect bar_graph_bar(const RealRect& screen_rect, int group, int group_count, int start, int end, int max) {
	double width_space = screen_rect.width / group_count; // including spacing
	double width       = width_space / 5 * 4;
	double space       = width_space / 5;
	double step_height = screen_rect.height / max; // multiplier for bar height
	int top    = (int)(screen_rect.bottom() - start * step_height);
	int bottom = (int)(screen_rect.bottom() - end   * step_height);
	if (bottom < top) swap(top,bottom);
	bottom += 1;
	return RealRect(
		screen_rect.x + width_space * group + space / 2,
		top,
		width,
		bottom - top
	);
}
/// Which column of the bar graph with count bars is coordinate x in?
int find_bar_graph_column(double width, double x, int count) {
	double width_space = width / count; // including spacing
	double space       = width_space / 5;
	// Find column in which the point could be located
	int    col    = (int)floor(x / width_space);
	if (col < 0 || col >= count) return -1; // not a column
	double in_col = x - col * width_space;
	if (in_col < space / 2)               return -1; // left
	if (in_col > width_space - space / 2) return -1; // right
	return col;
}

void BarGraph::draw(RotatedDC& dc, int current, DrawLayer layer) const {
	if (!data) return;
	// Rectangle for bars
	RealRect screen_rect = dc.getInternalRect();
	GraphAxis& axis = axis_data();
	int count = int(axis.groups.size());
	// Bar sizes
	if (layer == LAYER_SELECTION) {
		// Highlight current column
		Color bg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
		if (current >= 0) {
			const GraphGroup& group = axis.groups[current];
			RealRect bar = bar_graph_bar(screen_rect, current, count, 0, group.size, axis.max);
			dc.SetPen(*wxTRANSPARENT_PEN);
			dc.SetBrush(lerp(bg, group.color, 0.25));
			dc.DrawRectangle(bar.move(-5,-5,10,5));
			dc.SetBrush(lerp(bg, group.color, 0.5));
			dc.DrawRectangle(bar.move(-2,-2,4,2));
		}
	} else if (layer == LAYER_VALUES) {
		// Draw bars
		Color fg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
		int i = 0;
		FOR_EACH_CONST(g, axis.groups) {
			// draw bar
			dc.SetPen(i == current ? fg : lerp(fg,g.color,0.5));
			dc.SetBrush(g.color);
			RealRect bar = bar_graph_bar(screen_rect, i++, count, 0, g.size, axis.max);
			dc.DrawRectangle(bar);
			// redraw axis part
			dc.SetPen(fg);
			dc.DrawLine(bar.bottomLeft()+Vector2D(0,-1), bar.bottomRight()+Vector2D(0,-1));
		}
	}
}
int BarGraph::findItem(const RealPoint& pos, const RealRect& screen_rect, bool tight) const {
	if (!data) return -1;
	if (pos.y > max(screen_rect.top(), screen_rect.bottom())) return -1; // below
	if (pos.y < min(screen_rect.top(), screen_rect.bottom())) return -1; // above
	// TODO: tight check
	return find_bar_graph_column(screen_rect.width, pos.x - screen_rect.x, (int)axis_data().groups.size());
}

// ----------------------------------------------------------------------------- : Bar Graph 2D

void BarGraph2D::draw(RotatedDC& dc, const vector<int>& current, DrawLayer layer) const {
	if (!data || data->axes.size() <= max(axis1,axis2)) return;
	// Rectangle for bars
	RealRect screen_rect = dc.getInternalRect();
	GraphAxis& axis1 = axis1_data(); // the major axis
	GraphAxis& axis2 = axis2_data(); // the stacked axis
	int count = int(axis1.groups.size());
	int cur1 = this->axis1 < current.size() ? current[this->axis1] : -1;
	int cur2 = this->axis2 < current.size() ? current[this->axis2] : -1;
	// Draw
	if (layer == LAYER_SELECTION) {
		// Highlight current column
		Color bg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
		if (cur1 >= 0) {
			// draw selected bar
			int start = 0;
			int j = 0;
			FOR_EACH_CONST(g2, axis2.groups) {
				int end = start + values[j + axis2.groups.size() * cur1];
				if (j == cur2 || cur2 < 0) {
					RealRect bar = bar_graph_bar(screen_rect, cur1, count, start, end, axis1.max);
					dc.SetBrush(lerp(bg, g2.color, 0.25));
					dc.DrawRectangle(bar.move(-5,0,10,0));
					dc.SetBrush(lerp(bg, g2.color, 0.5));
					dc.DrawRectangle(bar.move(-2,0,4,0));
				}
				start = end;
				++j;
			}
		} else if (cur2 >= 0) {
			// entire row
			// TODO
		}
	} else if (layer == LAYER_VALUES) {
		// Draw bars
		Color fg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
		for (int i = 0 ; i < count ; ++i) {
			// draw stacked bars
			int start = 0;
			int j = 0;
			Color prevColor  = fg;
			bool  prevActive = true;
			FOR_EACH_CONST(g2, axis2.groups) {
				bool active = !(cur1 == -1 && cur2 == -1) && (i == cur1 || cur1 == -1) && (j == cur2 || cur2 == -1);
				int end = start + values[j + axis2.groups.size() * i];
				if (start != end) {
					dc.SetBrush(g2.color);
					dc.SetPen(active ? fg : lerp(fg, g2.color, 0.5));
					RealRect bar = bar_graph_bar(screen_rect, i, count, start, end, axis1.max);
					dc.DrawRectangle(bar);
					// fix up line below
					dc.SetPen(active || prevActive ? fg : lerp(fg,lerp(prevColor,g2.color,0.5),0.5));
					dc.DrawLine(bar.bottomLeft()+Vector2D(0,-1), bar.bottomRight()+Vector2D(0,-1));
					// next
					prevActive = active;
					prevColor  = g2.color;
				}
				start = end; j++;
			}
		}
	}
}
bool BarGraph2D::findItem(const RealPoint& pos, const RealRect& screen_rect, bool tight, vector<int>& out) const {
	if (!data || data->axes.size() <= max(axis1,axis2)) return false;
	if (pos.y > max(screen_rect.top(), screen_rect.bottom())) return false; // below
	if (pos.y < min(screen_rect.top(), screen_rect.bottom())) return false; // above
	// column containing pos
	GraphAxis& axis1 = axis1_data(); // the major axis
	int count = (int)axis1.groups.size();
	int col   = find_bar_graph_column(screen_rect.width, pos.x - screen_rect.x, count);
	if (col < 0) return false;
	// row containing pos...
	int max_value = (int)axis1.max;
	int value = (int)((screen_rect.bottom() - pos.y) / screen_rect.height * max_value);
	if (value < 0 || value > max_value) return false;
	// find the row
	int row = -1;
	size_t vs = col * axis2_data().groups.size();
	for (int i = 0 ; i < (int)axis2_data().groups.size() ; ++i) {
		value -= values.at(vs+i);
		if (value < 0) {
			// in this layer of the stack
			row = i;
			break;
		}
	}
	if (row == -1) return false;
	// done
	out.clear();
	out.insert(out.begin(), data->axes.size(), -1);
	out.at(this->axis1) = col;
	out.at(this->axis2) = row;
	return true;
}

// ----------------------------------------------------------------------------- : Pie Graph

void PieGraph::draw(RotatedDC& dc, int current, DrawLayer layer) const {
	if (!data) return;
	// Rectangle for the pie
	GraphAxis& axis = axis_data();
	RealRect screen_rect = dc.getInternalRect();
	double size = min(screen_rect.width, screen_rect.height);
	RealSize pie_size(size, size);
	RealSize pie_size_large(size+20, size+20);
	RealPoint pie_pos = screen_rect.position() + screen_rect.size() * 0.5;
	//RealPoint pos = align_in_rect(ALIGN_MIDDLE_CENTER, RealSize(size,size), screen_rect);
	// draw items
	if (layer == LAYER_VALUES) {
		Color fg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
		dc.SetPen(fg);
		// draw pies
		Radians angle = M_PI/2;
		int i = 0;
		FOR_EACH_CONST(g, axis.groups) {
			// draw pie
			dc.SetBrush(g.color);
			if (g.size > 0) {
				bool active = i == current;
				dc.SetPen(active ? fg : lerp(fg,g.color,0.5));
				Radians end_angle = angle - 2 * M_PI * (double)g.size / axis.total;
				dc.DrawEllipticArc(pie_pos, active ? pie_size_large : pie_size, end_angle, angle);
				angle = end_angle;
			}
			++i;
		}
		// draw spokes
		if (axis.groups.size() > 1) {
			int i = 0;
			Radians angle = M_PI/2;
			FOR_EACH_CONST(g, axis.groups) {
				if (true) {
					int i2 = (i - 1 + (int)axis.groups.size()) % (int)axis.groups.size();
					bool active = i == current || i2 == current;
					dc.SetPen(active ? fg : lerp(fg,lerp(g.color,axis.groups[i2].color,0.5),0.5));
					RealSize size = active ? pie_size_large : pie_size;
					dc.DrawEllipticSpoke(pie_pos, size, angle);
					angle -= 2 * M_PI * (double)g.size / axis.total;
				}
				++i;
			}
		}
	}
}
int PieGraph::findItem(const RealPoint& pos, const RealRect& screen_rect, bool tight) const {
	if (!data) return -1;
	// Rectangle for the pie
	GraphAxis& axis = axis_data();
	double size = min(screen_rect.width, screen_rect.height);
	RealPoint pie_pos = screen_rect.position() + screen_rect.size() / 2;
	// position in circle
	Vector2D delta = pos - pie_pos;
	if (delta.lengthSqr()*4 > size*size) {
		return -1; // outside circle
	}
	double pos_angle = atan2(-delta.y, delta.x) - M_PI/2; // in range [-pi..pi]
	if (pos_angle < 0) pos_angle += 2 * M_PI;
	// find angle
	Radians angle = 2 * M_PI;
	int i = 0;
	FOR_EACH_CONST(g, axis.groups) {
		angle -= 2 * M_PI * (double)g.size / axis.total;
		if (angle < pos_angle) return i;
		++i;
	}
	return -1; //should not happen
}

// ----------------------------------------------------------------------------- : Scatter Plot

inline double lerp(double a, double b, double t) { return a + t * (b-a); }

void ScatterGraph::draw(RotatedDC& dc, const vector<int>& current, DrawLayer layer) const {
	if (!data || data->axes.size() <= max(axis1,axis2)) return;
	// Rectangle for drawing
	RealRect screen_rect = dc.getInternalRect();
	GraphAxis& axis1 = axis1_data(); // the major axis
	GraphAxis& axis2 = axis2_data(); // the stacked axis
	int cur1 = this->axis1 < current.size() ? current[this->axis1] : -1;
	int cur2 = this->axis2 < current.size() ? current[this->axis2] : -1;
	RealSize size(screen_rect.width / axis1.groups.size(), screen_rect.height / axis2.groups.size()); // size for a single cell
	// size increments:
	double step = min(size.width / max_value_x, size.height / max_value_y) * 0.99;
	// Draw
	if (layer == LAYER_SELECTION) {
		dc.SetPen(*wxTRANSPARENT_PEN);
		Color bg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
		if (cur1 >= 0 && cur2 >= 0) {
			UInt value = values[cur1 * axis2.groups.size() + cur2];
			if (value) {
				dc.SetBrush(lerp(bg,lerp(axis1.groups[cur1].color, axis2.groups[cur2].color, 0.5),0.5));
				dc.DrawCircle(RealPoint(screen_rect.left() + cur1 * size.width, screen_rect.bottom() - (cur2+1) * size.height) + size*0.5, scale(value) * step + 5);
			}
		} else if (cur1 >= 0) {
			dc.SetBrush(lerp(bg,axis1.groups[cur1].color,0.3));
			dc.DrawRectangle(RealRect(screen_rect.x + cur1 * size.width, screen_rect.y, size.width, screen_rect.height));
		} else if (cur2 >= 0) {
			dc.SetBrush(lerp(bg,axis2.groups[cur2].color,0.3));
			dc.DrawRectangle(RealRect(screen_rect.x, screen_rect.bottom() - (cur2+1) * size.height, screen_rect.width, size.height));
		}
	} else if (layer == LAYER_VALUES) {
		Color fg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
		dc.SetPen(fg);
		size_t i = 0;
		int x = 0;
		FOR_EACH_CONST(g1, axis1.groups) {
			int y = 0;
			FOR_EACH_CONST(g2, axis2.groups) {
				UInt value = values[i++];
				if (value > 0) {
					Color color = lerp(fg, lerp(g1.color, g2.color, 0.5 - (axis1.auto_color == AUTO_COLOR_NO ? 0.35 : 0.0) + (axis2.auto_color == AUTO_COLOR_NO ? 0.35 : 0.0)), 0.5 + (axis1.auto_color == AUTO_COLOR_NO || axis2.auto_color == AUTO_COLOR_NO ? 0.5 : 0.0));
					bool active = !(cur1 == -1 && cur2 == -1) && (x == cur1 || cur1 == -1) && (y == cur2 || cur2 == -1);
					dc.SetPen(active ? fg : lerp(fg,color,0.5));
					dc.SetBrush(color);
					double radius = floor(scale(value) * step - 0.5) * 2 + 1; // always odd
					double xx = screen_rect.left() + (x+0.5) * size.width + 0.5;
					double yy = screen_rect.bottom() - (y+0.5) * size.height + 0.5;
					dc.DrawEllipse(RealPoint(xx,yy),RealSize(radius,radius));
				}
				++y;
			}
			++x;
		}
	}
}

double ScatterGraph::scale(double x) {
	return pow(x, 0.75);
}

bool ScatterGraph::findItem(const RealPoint& pos, const RealRect& screen_rect, bool tight, vector<int>& out) const {
	if (!data || data->axes.size() <= max(axis1,axis2)) return false;
	// clicked item
	GraphAxis& axis1 = axis1_data();
	GraphAxis& axis2 = axis2_data();
	int col = (int) floor((pos.x - screen_rect.x)        / screen_rect.width  * axis1.groups.size());
	int row = (int) floor((screen_rect.bottom() - pos.y) / screen_rect.height * axis2.groups.size());
	if (col < 0 || col >= (int)axis1.groups.size()) return false;
	if (row < 0 || row >= (int)axis2.groups.size()) return false;
	// any values here?
	if (tight && values[row + col * axis2.groups.size()] == 0) return false;
	// done
	out.clear();
	out.insert(out.begin(), data->axes.size(), -1);
	out.at(this->axis1) = col;
	out.at(this->axis2) = row;
	return true;
}

void ScatterGraph::setData(const GraphDataP& d) {
	Graph2D::setData(d);
	if (!data) return;
	if (values.empty()) return;
	// find maximum
	max_value = 0;
	FOR_EACH(v, values) {
		max_value = max(max_value,v);
	}
	// find maximum (x)
	size_t n1 = axis1_data().groups.size();
	size_t n2 = axis2_data().groups.size();
	double allow_overlap_x = axis1_data().numeric ? 0.9 : 1; // it is okay if we get a bit of a blob on a numeric axis
	max_value_x = 0;
	for (size_t y = 0 ; y < n2 ; ++y) {
		max_value_x = max(max_value_x, 2*scale(values[y])); // left border
		for (size_t x = 0 ; x+1 < n1 ; ++x) {
			double sum_dist = scale(values[x*n2+y]) + scale(values[(x+1)*n2+y]);
			max_value_x = max(max_value_x, allow_overlap_x * sum_dist); // between two
		}
		max_value_x = max(max_value_x, 2*scale(values[(n1-1)*n2+y])); // right border
	}
	// find maximum (y)
	double allow_overlap_y = axis2_data().numeric ? 0.9 : 1;
	max_value_y = 0;
	for (size_t x = 0 ; x < n1 ; ++x) {
		max_value_y = max(max_value_y, 2*scale(values[x*n2])); // top border
		for (size_t y = 0 ; y+1 < n2 ; ++y) {
			double sum_dist = scale(values[x*n2+y]) + scale(values[x*n2+(y+1)]);
			max_value_y = max(max_value_y, allow_overlap_y * sum_dist); // between two
		}
		max_value_y = max(max_value_y, 2*scale(values[x*n2+(n2-1)])); // bottom border
	}
}

// ----------------------------------------------------------------------------- : Scatter Plot plus

void ScatterGraphPlus::setData(const GraphDataP& d) {
	ScatterGraph::setData(d);
	if (!data || data->axes.size() <= max(max(axis1,axis2),axis3)) {
		data = GraphDataP(); // invalid
		return;
	}
	data->crossAxis(axis1,axis2,axis3,values3D);
}

// ----------------------------------------------------------------------------- : Scatter Pie graph

void ScatterPieGraph::draw(RotatedDC& dc, const vector<int>& current, DrawLayer layer) const {
	if (!data) return;
	if (data->axes.size() <= max(max(axis1,axis2),axis3)) return;
	if (layer == LAYER_SELECTION) {
		ScatterGraph::draw(dc, current, layer);
	} else if (layer == LAYER_VALUES) {
		// Rectangle for drawing
		RealRect screen_rect = dc.getInternalRect();
		GraphAxis& axis1 = axis1_data(); // the major axis
		GraphAxis& axis2 = axis2_data(); // the stacked axis
		GraphAxis& axis3 = axis3_data(); // the pie axis
		int cur1 = this->axis1 < current.size() ? current[this->axis1] : -1;
		int cur2 = this->axis2 < current.size() ? current[this->axis2] : -1;
		RealSize size(screen_rect.width / axis1.groups.size(), screen_rect.height / axis2.groups.size()); // size for a single cell
		double step = min(size.width / max_value_x, size.height / max_value_y) * 0.99;
		// Draw pies
		Color fg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
		dc.SetPen(fg);
		for (size_t x = 0 ; x < axis1.groups.size() ; ++x) {
			for (size_t y = 0 ; y < axis2.groups.size() ; ++y) {
				size_t i = x * axis2.groups.size() + y;
				UInt value = values[i];
				double radius = floor(scale(value) * step - 0.5) * 2 + 1; // always odd
				bool active = !(cur1 == -1 && cur2 == -1) && ((int)x == cur1 || cur1 == -1) && ((int)y == cur2 || cur2 == -1);
				RealSize radius_s(radius,radius);
				RealPoint center(screen_rect.left() + (x+0.5) * size.width + 0.5, screen_rect.bottom() - (y+0.5) * size.height + 0.5);
				// draw pie slices
				Radians angle = 0;
				size_t j = 0;
				FOR_EACH(g, axis3.groups) {
					UInt val = values3D[i * axis3.groups.size() + j++];
					if (val > 0) {
						dc.SetBrush(g.color);
						dc.SetPen(active ? fg : lerp(fg,g.color,0.5));
						Radians end_angle = angle + 2 * M_PI * (double)val / value;
						dc.DrawEllipticArc(center, radius_s, angle, end_angle);
						angle = end_angle;
					}
				}
				// draw spokes?
			}
		}
	}
}


// ----------------------------------------------------------------------------- : Graph Stats table

void GraphStats::setData(const GraphDataP& d) {
	Graph1D::setData(d);
	if (!data) return;
	// update values
	GraphAxis& axis = axis_data();
	values.clear();
	if (!axis.numeric) return;
	if (axis.groups.empty()) return;
	values.push_back(make_pair(_("max"),  String::Format(_("%.2f"), axis.max_value)));
	values.push_back(make_pair(_("mean"), String::Format(_("%.2f"), axis.mean_value)));
}

RealSize GraphStats::determineSize(RotatedDC& dc) const {
	if (values.empty()) return RealSize(-1,-1);
	dc.SetFont(*wxNORMAL_FONT);
	item_size = RealSize(0,0);
	label_width = 0;
	FOR_EACH_CONST(v, values) {
		RealSize this_item_size = dc.GetTextExtent(v.first);
		double this_label_width = this_item_size.width + 3;
		this_item_size = dc.GetTextExtent(v.second);
		this_item_size = RealSize(this_item_size.width + 6, this_item_size.height + 5);
		item_size = piecewise_max(item_size, this_item_size);
		label_width = max(label_width, this_label_width);
	}
	item_size.width += label_width;
	size = RealSize(item_size.width + 2, values.size() * item_size.height + 3); // margins
	return size;
}

void GraphStats::draw(RotatedDC& dc, int current, DrawLayer layer) const {
	if (values.empty()) return;
	if (!size.width) determineSize(dc);
	if (layer == LAYER_VALUES) {
		RealRect screen_rect = dc.getInternalRect();
		RealPoint pos = align_in_rect(alignment, size, screen_rect);
		Color fg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
		Color bg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
		// draw border
		dc.SetBrush(bg);
		dc.SetPen(fg);
		dc.DrawRectangle(RealRect(pos,size));
		// draw items
		dc.SetFont(*wxNORMAL_FONT);
		double y = pos.y + 1;
		FOR_EACH_CONST(v, values) {
			dc.DrawText(v.first,  RealPoint(pos.x + 3,               y + 2));
			dc.DrawText(v.second, RealPoint(pos.x + 3 + label_width, y + 2));
			y += item_size.height;
		}
	}
}

// ----------------------------------------------------------------------------- : Graph Legend

RealSize GraphLegend::determineSize(RotatedDC& dc) const {
	if (!data) return RealSize(-1,-1);
	GraphAxis& axis = axis_data();
	dc.SetFont(*wxNORMAL_FONT);
	item_size = RealSize(0,0);
	FOR_EACH(g, axis.groups) {
		RealSize this_item_size = dc.GetTextExtent(g.name);
		this_item_size = RealSize(this_item_size.width + 34, this_item_size.height + 5);
		item_size = piecewise_max(item_size, this_item_size);
	}
	size = RealSize(item_size.width + 2, axis.groups.size() * item_size.height + 3); // margins
	return size;
}

void GraphLegend::draw(RotatedDC& dc, int current, DrawLayer layer) const {
	if (!size.width) determineSize(dc);
	if (layer == LAYER_VALUES) {
		RealRect screen_rect = dc.getInternalRect();
		RealPoint pos = align_in_rect(alignment, size, screen_rect);
		GraphAxis& axis = axis_data();
		Color fg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
		Color bg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
		// draw border
		dc.SetBrush(bg);
		dc.SetPen(fg);
		dc.DrawRectangle(RealRect(pos,size));
		// draw items
		dc.SetFont(*wxNORMAL_FONT);
		double y = pos.y + 1;
		for (int j = 0 ; j < (int)axis.groups.size() ; ++j) {
			int i = reverse ? (int)axis.groups.size() - j - 1 : j;
			const GraphGroup& g = axis.groups[i];
			if (i == current) {
				dc.SetBrush(lerp(bg,g.color,0.5));
				dc.SetPen(*wxTRANSPARENT_PEN);
				dc.DrawRectangle(RealRect(pos.x+1, y, item_size.width, item_size.height + 1));
				dc.SetPen(fg);
			}
			dc.SetBrush(g.color);
			dc.SetPen(i == current ? fg : lerp(fg,g.color,0.5));
			dc.DrawRectangle(RealRect(pos.x+3, y + 2, 26, item_size.height - 3));
			dc.DrawText(g.name, RealPoint(pos.x + 32, y + 2));
			y += item_size.height;
		}
	}
}
int GraphLegend::findItem(const RealPoint& pos, const RealRect& screen_rect, bool tight) const {
	if (tight) return -1;
	RealPoint mypos = align_in_rect(alignment, size, screen_rect);
	RealPoint pos2(pos.x - mypos.x, pos.y - mypos.y);
	if (pos2.x < 0 || pos2.y < 0 || pos2.x >= size.width || pos2.y >= size.height) return -1;
	int col = (int) floor((pos2.y-1) / item_size.height);
	if (col < 0 || col >= (int)axis_data().groups.size()) return -1;
	return reverse ? (int)axis_data().groups.size() - col - 1 : col;
}

// ----------------------------------------------------------------------------- : Graph label axis

void GraphLabelAxis::draw(RotatedDC& dc, int current, DrawLayer layer) const {
	if (!data) return;
	RealRect screen_rect = dc.getInternalRect();
	GraphAxis& axis = axis_data();
	int count = int(axis.groups.size());
	// Draw
	dc.SetFont(*wxNORMAL_FONT);
	Color bg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
	Color fg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
	if (layer == LAYER_SELECTION && current >= 0) {
		// highlight selection
		GraphGroup& group = axis.groups[current];
		if (direction == HORIZONTAL) {
			double width = screen_rect.width / count; // width of an item
			dc.SetBrush(lerp(bg,group.color,0.5));
			dc.SetPen(*wxTRANSPARENT_PEN);
			RealSize text_size = dc.GetTextExtent(group.name);
			dc.DrawRectangle(RealRect(screen_rect.x + current * width, screen_rect.bottom(), width, text_size.height + 5));
		} else {
			double height = screen_rect.height / count;
			dc.SetBrush(lerp(bg,group.color,0.5));
			dc.SetPen(*wxTRANSPARENT_PEN);
			dc.DrawRectangle(RealRect(screen_rect.x, screen_rect.bottom() - (current+1)*height, -78, height));
		}
	} else if (layer == LAYER_AXES) {
		if (direction == HORIZONTAL) {
			double width = screen_rect.width / count; // width of an item
			// Draw labels
			double x = screen_rect.x;
			FOR_EACH_CONST(g, axis.groups) {
				// draw label, aligned bottom center
				RealSize text_size = dc.GetTextExtent(g.name);
				dc.SetClippingRegion(RealRect(x + 2, screen_rect.bottom() + 3, width - 4, text_size.height));
				dc.DrawText(g.name, align_in_rect(ALIGN_TOP_CENTER, text_size, RealRect(x, screen_rect.bottom() + 3, width, 0)));
				dc.DestroyClippingRegion();
				x += width;
			}
			// Draw lines
			if (draw_lines) {
				for (int i = 0 ; i < count ; ++i) {
					dc.SetPen(i == current ? fg : lerp(bg, fg, 0.2));
					if (draw_lines == DRAW_LINES_BETWEEN) {
						dc.DrawLine(RealPoint(screen_rect.x + (i+1.0)*width, screen_rect.top()), RealPoint(screen_rect.x + (i+1.0)*width, screen_rect.bottom()));
					} else {
						dc.DrawLine(RealPoint(screen_rect.x + (i+0.5)*width, screen_rect.top()), RealPoint(screen_rect.x + (i+0.5)*width, screen_rect.bottom() + 2));
					}
				}
			}
			// always draw axis line
			dc.SetPen(fg);
			dc.DrawLine(screen_rect.topLeft(), screen_rect.bottomLeft());
		} else {
			double height = screen_rect.height / count;
			// Draw labels
			double y = screen_rect.bottom();
			FOR_EACH_CONST(g, axis.groups) {
				// draw label, aligned middle right
				RealSize text_size = dc.GetTextExtent(g.name);
				//dc.SetClippingRegion(RealRect(x + 2, screen_rect.bottom() + 3, width - 4, text_size.height));
				dc.DrawText(g.name, align_in_rect(ALIGN_MIDDLE_RIGHT, text_size, RealRect(-4, y, 0, -height)));
				//dc.DestroyClippingRegion();
				y -= height;
			}
			// Draw lines
			if (draw_lines) {
				for (int i = 0 ; i < count ; ++i) {
					dc.SetPen(i == current ? fg : lerp(bg, fg, 0.2));
					if (draw_lines == DRAW_LINES_BETWEEN) {
						dc.DrawLine(RealPoint(screen_rect.left(),     screen_rect.bottom() - (i+1.0)*height), RealPoint(screen_rect.right(), screen_rect.bottom() - (i+1.0)*height));
					} else {
						dc.DrawLine(RealPoint(screen_rect.left() - 2, screen_rect.bottom() - (i+0.5)*height), RealPoint(screen_rect.right(), screen_rect.bottom() - (i+0.5)*height));
					}
				}
			}
			// always draw axis line
			dc.SetPen(fg);
			dc.DrawLine(screen_rect.bottomLeft(), screen_rect.bottomRight());
		}
	}
}
int GraphLabelAxis::findItem(const RealPoint& pos, const RealRect& screen_rect, bool tight) const {
	if (!data) return -1;
	GraphAxis& axis = axis_data();
	int col;
	if (direction == HORIZONTAL) {
		col = (int) floor((pos.x - screen_rect.x) / screen_rect.width  * axis.groups.size());
		if (pos.y < screen_rect.bottom()) return -1;
	} else {
		col = (int) floor((screen_rect.bottom() - pos.y) / screen_rect.height * axis.groups.size());
		if (pos.x > screen_rect.left()) return -1;
	}
	if (col < 0 || col >= (int)axis.groups.size()) return -1;
	return col;
}

// ----------------------------------------------------------------------------- : Graph value axis

void GraphValueAxis::draw(RotatedDC& dc, int current, DrawLayer layer) const {
	if (layer != LAYER_AXES) return;
	if (!data) return;
	// How many labels and lines to draw?
	RealRect screen_rect = dc.getInternalRect();
	GraphAxis& axis = axis_data();
	double step_height = screen_rect.height / axis.max; // height of a single value
	dc.SetFont(*wxNORMAL_FONT);
	int label_step = (int) ceil(max(1.0, (dc.GetCharHeight()) / step_height)); // values per labeled line
	// Colors
	Color bg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
	Color fg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
	// Draw backlines (horizontal) and value labels
	dc.SetPen(lerp(bg, fg, 0.2));
	int highlight = (highlight_value && current >= 0) ? (int)axis.groups[current].size : -1;
	for (int i = 0 ; i <= (int)axis.max ; ++i) {
		if (i % label_step == 0 || i == highlight) {
			// highlight?
			if (i == highlight) {
				wxFont font(*wxNORMAL_FONT);
				font.SetWeight(wxBOLD);
				dc.SetFont(font);
				dc.SetPen(fg);
			}
			// draw line
			int y = (int) (screen_rect.bottom() - i * step_height);
			dc.DrawLine(RealPoint(screen_rect.left() - 2, y), RealPoint(screen_rect.right(), y));
			// draw label, aligned middle right
			if (! ((i < highlight && i + label_step > highlight) ||
			       (i > highlight && i - label_step < highlight)) || highlight == -1) {
				// don't draw labels before/after current to make room
				String label; label << i;
				RealSize text_size = dc.GetTextExtent(label);
				dc.DrawText(label, align_in_rect(ALIGN_MIDDLE_RIGHT, text_size, RealRect(screen_rect.x - 4, y, 0, 0)));
			}
			// restore font/pen
			if (i == highlight) {
				dc.SetFont(*wxNORMAL_FONT);
				dc.SetPen(lerp(bg, fg, 0.2));
			}
		}
	}
	// Draw axis
	dc.SetPen(fg);
	dc.DrawLine(screen_rect.bottomLeft() - RealSize(2,0), screen_rect.bottomRight());
}

// ----------------------------------------------------------------------------- : Graph with margins

void GraphWithMargins::draw(RotatedDC& dc, const vector<int>& current, DrawLayer layer) const {
	RealRect inner = dc.getInternalRect().move(margin_left, margin_top, - margin_left - margin_right, - margin_top - margin_bottom);
	if (upside_down) { inner.y += inner.height; inner.height = -inner.height; }
	Rotation new_size(0, inner);
	Rotater rot(dc, new_size);
	graph->draw(dc, current, layer);
}
bool GraphWithMargins::findItem(const RealPoint& pos, const RealRect& screen_rect, bool tight, vector<int>& out) const {
	RealRect inner = screen_rect.move(margin_left, margin_top, - margin_left - margin_right, - margin_top - margin_bottom);
	if (upside_down) { inner.y += inner.height; inner.height = -inner.height; }
	return graph->findItem(pos, inner, tight, out);
}
void GraphWithMargins::setData(const GraphDataP& d) {
	Graph::setData(d);
	graph->setData(d);
}

// ----------------------------------------------------------------------------- : Graph Container

void GraphContainer::draw(RotatedDC& dc, const vector<int>& current, DrawLayer layer) const {
	FOR_EACH_CONST(g, items) {
		g->draw(dc, current, layer);
	}
}
bool GraphContainer::findItem(const RealPoint& pos, const RealRect& screen_rect, bool tight, vector<int>& out) const {
	FOR_EACH_CONST_REVERSE(g, items) {
		if (g->findItem(pos, screen_rect, tight, out)) return true;
	}
	return false;
}
void GraphContainer::setData(const GraphDataP& d) {
	Graph::setData(d);
	FOR_EACH(g, items) {
		g->setData(d);
	}
}
void GraphContainer::add(const GraphP& graph) {
	items.push_back(graph);
}


// ----------------------------------------------------------------------------- : GraphControl

GraphControl::GraphControl(Window* parent, int id)
	: wxControl(parent, id, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS | wxBORDER_THEME_FIX(wxBORDER_THEME))
	, layout(GRAPH_TYPE_BAR)
{}

void GraphControl::setLayout(GraphType type, bool force) {
	if (!force && graph && type == layout) return;
	GraphDataP data = graph ? graph->getData() : GraphDataP();
	switch (type) {
		case GRAPH_TYPE_BAR: {
			intrusive_ptr<GraphContainer> combined(new GraphContainer());
			combined->add(intrusive(new GraphValueAxis(0, true)));
			combined->add(intrusive(new GraphLabelAxis(0, HORIZONTAL)));
			combined->add(intrusive(new BarGraph(0)));
			combined->add(intrusive(new GraphStats(0, ALIGN_TOP_RIGHT)));
			graph = intrusive(new GraphWithMargins(combined, 23,8,7,20));
			break;
		} case GRAPH_TYPE_PIE: {
			intrusive_ptr<GraphContainer> combined(new GraphContainer());
			combined->add(intrusive(new GraphWithMargins(intrusive(new PieGraph(0)), 0,0,120,0)));
			combined->add(intrusive(new GraphLegend(0, ALIGN_TOP_RIGHT, false)));
			graph = intrusive(new GraphWithMargins(combined, 20,20,20,20));
			break;
		} case GRAPH_TYPE_STACK: {
			intrusive_ptr<GraphContainer> combined(new GraphContainer());
			combined->add(intrusive(new GraphValueAxis(0, false)));
			combined->add(intrusive(new GraphLabelAxis(0, HORIZONTAL)));
			combined->add(intrusive(new BarGraph2D(0,1)));
			combined->add(intrusive(new GraphLegend(1, ALIGN_TOP_RIGHT, true)));
			graph = intrusive(new GraphWithMargins(combined, 23,8,7,20));
			break;
		} case GRAPH_TYPE_SCATTER: {
			intrusive_ptr<GraphContainer> combined(new GraphContainer());
			combined->add(intrusive(new GraphLabelAxis(0, HORIZONTAL, false, DRAW_LINES_MID)));
			combined->add(intrusive(new GraphLabelAxis(1, VERTICAL,   false, DRAW_LINES_MID)));
			combined->add(intrusive(new ScatterGraph(0,1)));
			graph = intrusive(new GraphWithMargins(combined, 80,8,7,20));
			break;
		} case GRAPH_TYPE_SCATTER_PIE: {
			intrusive_ptr<GraphContainer> combined(new GraphContainer());
			combined->add(intrusive(new GraphLabelAxis(0, HORIZONTAL, false, DRAW_LINES_MID)));
			combined->add(intrusive(new GraphLabelAxis(1, VERTICAL,   false, DRAW_LINES_MID)));
			combined->add(intrusive(new ScatterPieGraph(0,1,2)));
			graph = intrusive(new GraphWithMargins(combined, 80,8,7,20));
			break;
		} default:
			graph = GraphP();
	}
	if (data && graph) graph->setData(data);
	layout = type;
}

GraphType GraphControl::getLayout() const {
	return layout;
}

void GraphControl::setData(const GraphDataPre& data) {
	setData(intrusive(new GraphData(data)));
}
void GraphControl::setData(const GraphDataP& data) {
	if (graph) {
		graph->setData(data);
		current_item.clear(); // TODO : preserve selection
	}
	Refresh(false);
}
GraphDataP GraphControl::getData() const {
	if (graph) return graph->getData();
	else       return GraphDataP();
}

size_t GraphControl::getDimensionality() const {
	if (graph) return graph->getData()->axes.size();
	else       return 0;
}

void GraphControl::onPaint(wxPaintEvent&) {
	wxBufferedPaintDC dc(this);
	try {
		wxSize cs = GetClientSize();
		RotatedDC rdc(dc, 0, RealRect(RealPoint(0,0),cs), 1, QUALITY_LOW);
		rdc.SetPen(*wxTRANSPARENT_PEN);
		rdc.SetBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
		rdc.DrawRectangle(rdc.getInternalRect());
		if (graph) {
			for (int layer = LAYER_BOTTOM ; layer < LAYER_COUNT ; ++layer) {
				graph->draw(rdc, current_item, (DrawLayer)layer);
			}
		}
	} CATCH_ALL_ERRORS(false); // don't show message boxes in onPaint!
}

void GraphControl::onSize(wxSizeEvent&) {
	Refresh(false);
}

void GraphControl::onMouseDown(wxMouseEvent& ev) {
	if (!graph) return;
	wxSize cs = GetClientSize();
	if (graph->findItem(RealPoint(ev.GetX(), ev.GetY()), RealRect(RealPoint(0,0),cs), false, current_item)) {
		onSelectionChange();
	}
	ev.Skip(); // focus
}
void GraphControl::onChar(wxKeyEvent& ev) {
	if (!graph) return;
	GraphDataP data = graph->getData();
	if (!data) return;
	switch (ev.GetKeyCode()) {
		case WXK_LEFT:
			if (current_item.size() >= 1 && data->axes.size() >= 1 && current_item[0] != -1) {
				current_item[0]--;
				onSelectionChange();
			}
			break;
		case WXK_RIGHT:
			if (current_item.size() >= 1 && data->axes.size() >= 1 && current_item[0] + 1 < (int)data->axes[0]->groups.size()) {
				current_item[0]++;
				onSelectionChange();
			}
			break;
		case WXK_UP:
			if (current_item.size() >= 2 && data->axes.size() >= 2 && current_item[1] + 1 < (int)data->axes[1]->groups.size()) {
				current_item[1]++;
				onSelectionChange();
			}
			break;
		case WXK_DOWN:
			if (current_item.size() >= 2 && data->axes.size() >= 2 && current_item[1] != -1) {
				current_item[1]--;
				onSelectionChange();
			}
			break;
		case WXK_TAB: {
			// send a navigation event to our parent, to select another control
			// we need this because of wxWANTS_CHARS
			wxNavigationKeyEvent nev;
			nev.SetDirection(!ev.ShiftDown());
			GetParent()->HandleWindowEvent(nev);
			} break;
	}
}

void GraphControl::onSelectionChange() {
	wxCommandEvent ev(EVENT_GRAPH_SELECT, GetId());
	ProcessEvent(ev);
	Refresh(false);
}

bool GraphControl::hasSelection(size_t axis) const {
	return axis < current_item.size() && current_item[axis] >= 0;
}
String GraphControl::getSelection(size_t axis) const {
	if (!graph || axis >= current_item.size() || axis >= graph->getData()->axes.size()) return wxEmptyString;
	GraphAxis& a = *graph->getData()->axes[axis];
	int i = current_item[axis];
	if (i == -1 || (size_t)i >= a.groups.size()) return wxEmptyString;
	return a.groups[current_item[axis]].name;
}
vector<int> GraphControl::getSelectionIndices() const {
	return current_item;
}

void GraphControl::onMotion(wxMouseEvent& ev) {
	if (!graph) return;
	wxSize cs = GetClientSize();
	vector<int> hovered_item(current_item.size());
	if (graph->findItem(RealPoint(ev.GetX(), ev.GetY()), RealRect(RealPoint(0,0),cs), true, hovered_item)) {
		// determine tooltip
		const GraphData& data = *graph->getData();
		String tip;
		for (size_t dim = 0 ; dim < hovered_item.size() ; ++dim) {
			if (hovered_item[dim] != -1 && (size_t)hovered_item[dim] < data.axes[dim]->groups.size()) {
				if (!tip.empty()) tip += _("\n");
				tip += data.axes[dim]->name + _(": ") + data.axes[dim]->groups[hovered_item[dim]].name;
			}
		}
		UInt count = data.count(hovered_item);
		tip += String::Format(_("\n%d "), count) + (count == 1 ? _TYPE_("card") : _TYPE_("cards"));
		tip.Replace(_(" "),_("\xA0"));
		// set tooltip
		SetToolTip(tip);
	} else {
		// Note: don't use SetToolTip directly, we don't want to create a tip if it doesn't exist
		//       on winXP this destroys the multiline behaviour.
		wxToolTip* tooltip = GetToolTip();
		if (tooltip) tooltip->SetTip(_(""));
	}
	ev.Skip();
}

BEGIN_EVENT_TABLE(GraphControl, wxControl)
	EVT_PAINT		(GraphControl::onPaint)
	EVT_SIZE		(GraphControl::onSize)
	EVT_LEFT_DOWN	(GraphControl::onMouseDown)
	EVT_MOTION		(GraphControl::onMotion)
	EVT_CHAR		(GraphControl::onChar)
	EVT_ERASE_BACKGROUND(GraphControl::onEraseBackground)
END_EVENT_TABLE  ()
