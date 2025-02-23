// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ui/wrap/vertical_layout.h"

#include "ui/ui_utility.h"

namespace Ui {

QMargins VerticalLayout::getMargins() const {
	auto result = QMargins();
	if (!_rows.empty()) {
		auto &top = _rows.front();
		auto topMargin = top.widget->getMargins().top();
		result.setTop(
			qMax(topMargin - top.margin.top(), 0));
		auto &bottom = _rows.back();
		auto bottomMargin = bottom.widget->getMargins().bottom();
		result.setBottom(
			qMax(bottomMargin - bottom.margin.bottom(), 0));
		for (auto &row : _rows) {
			auto margins = row.widget->getMargins();
			result.setLeft(qMax(
				margins.left() - row.margin.left(),
				result.left()));
			result.setRight(qMax(
				margins.right() - row.margin.right(),
				result.right()));
		}
	}
	return result;
}

int VerticalLayout::naturalWidth() const {
	auto result = 0;
	for (auto &row : _rows) {
		const auto natural = row.widget->naturalWidth();
		if (natural < 0) {
			return natural;
		}
		accumulate_max(
			result,
			row.margin.left() + natural + row.margin.right());
	}
	return result;
}

void VerticalLayout::setVerticalShift(int index, int shift) {
	Expects(index >= 0 && index < _rows.size());

	auto &row = _rows[index];
	if (const auto delta = shift - row.verticalShift) {
		row.verticalShift = shift;
		row.widget->move(row.widget->x(), row.widget->y() + delta);
		row.widget->update();
	}
}

void VerticalLayout::reorderRows(int oldIndex, int newIndex) {
	Expects(oldIndex >= 0 && oldIndex < _rows.size());
	Expects(newIndex >= 0 && newIndex < _rows.size());
	Expects(!_inResize);

	base::reorder(_rows, oldIndex, newIndex);
	resizeToWidth(width());
}

int VerticalLayout::resizeGetHeight(int newWidth) {
	_inResize = true;
	auto guard = gsl::finally([&] { _inResize = false; });

	auto margins = getMargins();
	auto result = 0;
	for (auto &row : _rows) {
		updateChildGeometry(
			margins,
			row.widget,
			row.margin,
			newWidth,
			result + row.verticalShift);
		result += row.margin.top()
			+ row.widget->heightNoMargins()
			+ row.margin.bottom();
	}
	return result;
}

void VerticalLayout::visibleTopBottomUpdated(
		int visibleTop,
		int visibleBottom) {
	for (auto &row : _rows) {
		setChildVisibleTopBottom(
			row.widget,
			visibleTop,
			visibleBottom);
	}
}

void VerticalLayout::updateChildGeometry(
		const style::margins &margins,
		RpWidget *child,
		const style::margins &margin,
		int width,
		int top) const {
	auto availRowWidth = width
		- margin.left()
		- margin.right();
	child->resizeToNaturalWidth(availRowWidth);
	child->moveToLeft(
		margins.left() + margin.left(),
		margins.top() + margin.top() + top,
		width);
}

RpWidget *VerticalLayout::insertChild(
		int atPosition,
		object_ptr<RpWidget> child,
		const style::margins &margin) {
	Expects(atPosition >= 0 && atPosition <= _rows.size());
	Expects(!_inResize);

	if (const auto weak = AttachParentChild(this, child)) {
		_rows.insert(
			begin(_rows) + atPosition,
			{ std::move(child), margin });
		weak->heightValue(
		) | rpl::start_with_next_done([=] {
			if (!_inResize) {
				childHeightUpdated(weak);
			}
		}, [=] {
			removeChild(weak);
		}, _rowsLifetime);
		return weak;
	}
	return nullptr;
}

void VerticalLayout::childHeightUpdated(RpWidget *child) {
	auto it = ranges::find_if(_rows, [child](const Row &row) {
		return (row.widget == child);
	});

	auto margins = getMargins();
	auto top = [&] {
		if (it == _rows.begin()) {
			return margins.top();
		}
		auto prev = it - 1;
		return prev->widget->bottomNoMargins() + prev->margin.bottom();
	}() - margins.top();
	for (auto end = _rows.end(); it != end; ++it) {
		auto &row = *it;
		auto margin = row.margin;
		auto widget = row.widget.data();
		widget->moveToLeft(
			margins.left() + margin.left(),
			margins.top() + top + margin.top());
		top += margin.top()
			+ widget->heightNoMargins()
			+ margin.bottom();
	}
	resize(width(), margins.top() + top + margins.bottom());
}

void VerticalLayout::removeChild(RpWidget *child) {
	auto it = ranges::find_if(_rows, [child](const Row &row) {
		return (row.widget == child);
	});
	auto end = _rows.end();
	Assert(it != end);

	auto margins = getMargins();
	auto top = [&] {
		if (it == _rows.begin()) {
			return margins.top();
		}
		auto prev = it - 1;
		return prev->widget->bottomNoMargins() + prev->margin.bottom();
	}() - margins.top();
	for (auto next = it + 1; next != end; ++next) {
		auto &row = *next;
		auto margin = row.margin;
		auto widget = row.widget.data();
		widget->moveToLeft(
			margins.left() + margin.left(),
			margins.top() + top + margin.top());
		top += margin.top()
			+ widget->heightNoMargins()
			+ margin.bottom();
	}
	it->widget = nullptr;
	_rows.erase(it);

	resize(width(), margins.top() + top + margins.bottom());
}

void VerticalLayout::clear() {
	while (!_rows.empty()) {
		removeChild(_rows.front().widget.data());
	}
}

} // namespace Ui
