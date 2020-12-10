#include "ListBox.h"

#include "../../Constants.h"
#include "../../Cache.h"

#include <NAS2D/Utility.h>
#include <NAS2D/MathUtils.h>
#include <NAS2D/StringUtils.h>
#include <NAS2D/Renderer/Renderer.h>
#include <NAS2D/Renderer/Point.h>

#include <algorithm>
#include <cmath>


using namespace NAS2D;


ListBox::ListBox() :
	mFont{fontCache.load(constants::FONT_PRIMARY, constants::FONT_PRIMARY_NORMAL)}
{
	Utility<EventHandler>::get().mouseButtonDown().connect(this, &ListBox::onMouseDown);
	Utility<EventHandler>::get().mouseMotion().connect(this, &ListBox::onMouseMove);
	Utility<EventHandler>::get().mouseWheel().connect(this, &ListBox::onMouseWheel);

	mSlider.displayPosition(false);
	mSlider.length(0);
	mSlider.thumbPosition(0);
	mSlider.change().connect(this, &ListBox::slideChanged);
	_updateItemDisplay();

	mLineHeight = static_cast<unsigned int>(mFont.height() + constants::MARGIN_TIGHT);
	mLineCount = static_cast<unsigned int>(mRect.height) / mLineHeight;
	_updateItemDisplay();
}


ListBox::~ListBox()
{
	Utility<EventHandler>::get().mouseButtonDown().disconnect(this, &ListBox::onMouseDown);
	Utility<EventHandler>::get().mouseMotion().disconnect(this, &ListBox::onMouseMove);
	Utility<EventHandler>::get().mouseWheel().disconnect(this, &ListBox::onMouseWheel);

	mSlider.change().disconnect(this, &ListBox::slideChanged);
}


void ListBox::onSizeChanged()
{
	_updateItemDisplay();
}


void ListBox::visibilityChanged(bool /*visible*/)
{
	_updateItemDisplay();
}


void ListBox::_updateItemDisplay()
{
	// Account for border around control
	mScrollArea = mRect.inset(1);

	mItemWidth = static_cast<unsigned int>(mRect.width);

	if ((mLineHeight * mItems.size()) > static_cast<std::size_t>(mRect.height))
	{
		mLineCount = static_cast<unsigned int>(mRect.height) / mLineHeight;
		if (mLineCount < mItems.size())
		{
			mSlider.position({rect().x + mRect.width - 14, mRect.y});
			mSlider.size({14, mRect.height});
			mSlider.length(static_cast<float>(static_cast<int>(mLineHeight * mItems.size()) - mRect.height));
			mCurrentOffset = static_cast<std::size_t>(mSlider.thumbPosition());
			mItemWidth = static_cast<unsigned int>(mRect.width - mSlider.size().x);
			mScrollArea.width -= mSlider.size().x; // Remove scroll bar from scroll area
			mSlider.visible(true);
		}
	}
	else
	{
		mCurrentOffset = 0;
		mSlider.length(0);
		mSlider.visible(false);
	}
}


/**
 * Gets whether the menu is empty or not.
 */
bool ListBox::empty() const
{
	return mItems.empty();
}


const std::string& ListBox::selectionText() const
{
	if (mCurrentSelection == constants::NO_SELECTION) { return constants::EMPTY_STR; }
	return mItems[mCurrentSelection].text;
}


int ListBox::selectionTag() const
{
	if (mCurrentSelection == constants::NO_SELECTION) { return 0; }
	return mItems[mCurrentSelection].tag;
}


/**
 * Adds an item to the Menu.
 *
 * \param	item	Item to add.
 *
 * \warning	Menu::font(Font& font) must have been called with a valid Font
 *			before this function can be safely called.
 *
 * \todo	Make this function safe to call regardless of whether a font
 *			has been defined or not.
 */
void ListBox::addItem(const std::string& item, int tag)
{
	mItems.push_back(ListBoxItem{item, tag});
	_updateItemDisplay();
}


/**
 * Removes a named item from the Menu.
 *
 * \todo	Complete this function.
 *
 * \note	Safe to call if Menu is empty.
 */
void ListBox::removeItem(const std::string& item)
{
	if (empty()) { return; }

	auto it = std::find(mItems.begin(), mItems.end(), item);

	if (it != mItems.end())
	{
		mItems.erase(it);
		mCurrentSelection = constants::NO_SELECTION;
		_updateItemDisplay();
	}
}


bool ListBox::itemExists(const std::string& item)
{
	return std::find(mItems.begin(), mItems.end(), item) != mItems.end();
}


void ListBox::setSelectionByName(const std::string& item)
{
	const auto target = toLowercase(item);
	for (std::size_t i = 0; i < mItems.size(); i++)
	{
		if (toLowercase(mItems[i].text) == target) { mCurrentSelection = i; return; }
	}
}


/**
 * Drops all items from the list.
 */
void ListBox::dropAllItems()
{
	mItems.clear();
	mCurrentSelection = 0;
	_updateItemDisplay();
}


void ListBox::sort()
{
	std::sort(mItems.begin(), mItems.end());
}


void ListBox::onMouseDown(EventHandler::MouseButton /*button*/, int x, int y)
{
	// Ignore if menu is empty or invisible
	if (empty() || !visible()) { return; }

	const auto point = NAS2D::Point{x, y};

	if (!rect().contains(point) || mCurrentHighlight == constants::NO_SELECTION)
	{
		return;
	}

	if (mSlider.visible() && mSlider.rect().contains(point))
	{
		return; // if the mouse is on the slider then the slider should handle that
	}

	if (static_cast<std::size_t>(mCurrentHighlight) >= mItems.size())
	{
		return;
	}

	currentSelection(mCurrentHighlight);
}


void ListBox::onMouseMove(int x, int y, int /*relX*/, int /*relY*/)
{
	// Ignore if menu is empty or invisible
	if (empty() || !visible()) { return; }

	const auto point = NAS2D::Point{x, y};

	// Ignore mouse motion events if the pointer isn't within the menu rect.
	if (!rect().contains(point))
	{
		mCurrentHighlight = constants::NO_SELECTION;
		return;
	}

	// if the mouse is on the slider then the slider should handle that
	if (mSlider.visible() && mSlider.rect().contains(point))
	{
		mCurrentHighlight = constants::NO_SELECTION;
		return;
	}

	mCurrentHighlight = (static_cast<std::size_t>(y - mRect.y) + mCurrentOffset) / static_cast<std::size_t>(mFont.height() + 2);

	if (mCurrentHighlight >= mItems.size())
	{
		mCurrentHighlight = constants::NO_SELECTION;
	}
}


/**
 * \todo	Make the scroll amount configurable.
 */
void ListBox::onMouseWheel(int /*x*/, int y)
{
	if (empty() || !visible()) { return; }

	mSlider.changeThumbPosition((y < 0 ? 16.0f : -16.0f));
}


void ListBox::update()
{
	// Ignore if menu is empty or invisible
	if (!visible()) { return; }

	auto& renderer = Utility<Renderer>::get();

	if (empty())
	{
		renderer.drawBoxFilled(mRect, NAS2D::Color::Black);
		const auto borderColor = hasFocus() ? mBorderColorActive : mBorderColorNormal;
		renderer.drawBox(mRect, borderColor);
		return;
	}

	renderer.clipRect(mRect);

	// draw boundaries of the widget
	NAS2D::Rectangle<int> listBounds = mRect;
	listBounds.width = static_cast<int>(mItemWidth);
	renderer.drawBox(listBounds, NAS2D::Color{0, 0, 0, 100});
	renderer.drawBoxFilled(listBounds, mBackgroundColorNormal);

	// Highlight currently selected item
	auto itemBounds = listBounds;
	itemBounds.height = static_cast<int>(mLineHeight);
	itemBounds.y += static_cast<int>((mCurrentSelection * mLineHeight) - mCurrentOffset);
	renderer.drawBoxFilled(itemBounds, mBackgroundColorSelected);

	// Highlight On mouse Over
	if (mCurrentHighlight != constants::NO_SELECTION)
	{
		auto highlightBounds = listBounds;
		highlightBounds.height = static_cast<int>(mLineHeight);
		highlightBounds.y += static_cast<int>((mCurrentHighlight * mLineHeight) - mCurrentOffset);
		renderer.drawBox(highlightBounds, mBackgroundColorMouseHover);
	}

	// display actuals values that are meant to be
	auto textPosition = listBounds.startPoint();
	textPosition += {constants::MARGIN_TIGHT, -static_cast<int>(mCurrentOffset)};
	for(std::size_t i = 0; i < mItems.size(); i++)
	{
		const auto textColor = (i == mCurrentHighlight) ? mTextColorMouseHover : mTextColorNormal;
		renderer.drawTextShadow(mFont, mItems[i].text, textPosition, {1, 1}, textColor, NAS2D::Color::Black);
		textPosition.y += mLineHeight;
	}

	mSlider.update();

	renderer.clipRectClear();
}


void ListBox::slideChanged(float newPosition)
{
	_updateItemDisplay();
	// Intentional truncation of fractional value
	const auto pos = std::floor(newPosition);
	if (pos != newPosition)
	{
		mSlider.thumbPosition(pos);
	}
}
