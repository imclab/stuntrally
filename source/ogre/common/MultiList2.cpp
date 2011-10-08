/*!
	@file
	@author		Albert Semenov
	@date		04/2008
	@module
*/
/*
	This file is part of MyGUI.

	MyGUI is free software: you can redistribute it and/or modify
	it under the terms of the GNU Lesser General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	MyGUI is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public License
	along with MyGUI.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "MyGUI_Precompiled.h"
#include "MultiList2.h"
#include "MyGUI_ResourceSkin.h"
#include "MyGUI_Button.h"
#include "MyGUI_StaticImage.h"
#include "MyGUI_List.h"
#include "MyGUI_Gui.h"
#include "MyGUI_WidgetManager.h"

namespace MyGUI
{

	MultiList2::MultiList2() :
		mHeightButton(0),
		mWidthBar(0),
		mButtonMain(nullptr),
		mLastMouseFocusIndex(ITEM_NONE),
		mSortUp(true),
		mSortColumnIndex(ITEM_NONE),
		mWidthSeparator(0),
		mOffsetButtonSeparator(2),
		mItemSelected(ITEM_NONE),
		mFrameAdvise(false),
		mClient(nullptr)
	{
	}

	void MultiList2::_initialise(WidgetStyle _style, const IntCoord& _coord, Align _align, ResourceSkin* _info, Widget* _parent, ICroppedRectangle * _croppedParent, IWidgetCreator * _creator, const std::string& _name)
	{
		Base::_initialise(_style, _coord, _align, _info, _parent, _croppedParent, _creator, _name);

		initialiseWidgetSkin(_info);
	}

	MultiList2::~MultiList2()
	{
		frameAdvise(false);
		shutdownWidgetSkin();
	}

	void MultiList2::baseChangeWidgetSkin(ResourceSkin* _info)
	{
		shutdownWidgetSkin();
		Base::baseChangeWidgetSkin(_info);
		initialiseWidgetSkin(_info);
	}

	void MultiList2::initialiseWidgetSkin(ResourceSkin* _info)
	{
		// парсим свойства
		const MapString& properties = _info->getProperties();
		if (!properties.empty())
		{
			MapString::const_iterator iter = properties.find("SkinButton");
			if (iter != properties.end()) mSkinButton = iter->second;
			iter = properties.find("HeightButton");
			if (iter != properties.end()) mHeightButton = utility::parseInt(iter->second);
			if (mHeightButton < 0) mHeightButton = 0;

			iter = properties.find("SkinList");
			if (iter != properties.end()) mSkinList = iter->second;

			iter = properties.find("SkinButtonEmpty");
			if (iter != properties.end())
			{
				mButtonMain = mClient->createWidget<Button>(iter->second,
					IntCoord(0, 0, mClient->getWidth(), mHeightButton), Align::Default);
			}

			iter = properties.find("WidthSeparator");
			if (iter != properties.end()) mWidthSeparator = utility::parseInt(iter->second);
			iter = properties.find("SkinSeparator");
			if (iter != properties.end()) mSkinSeparator = iter->second;
		}

		for (VectorWidgetPtr::iterator iter=mWidgetChildSkin.begin(); iter!=mWidgetChildSkin.end(); ++iter)
		{
			if (*(*iter)->_getInternalData<std::string>() == "Client")
			{
				MYGUI_DEBUG_ASSERT( ! mClient, "widget already assigned");
				mClient = (*iter);
				mWidgetClient = (*iter); // чтобы размер возвращался клиентской зоны
			}
		}
		// мона и без клиента
		if (nullptr == mClient) mClient = this;
	}

	void MultiList2::shutdownWidgetSkin()
	{
		mWidgetClient = nullptr;
		mClient = nullptr;
	}

	//----------------------------------------------------------------------------------//
	// методы для работы со столбцами
	void MultiList2::insertColumnAt(size_t _column, const UString& _name, int _width, Any _data)
	{
		MYGUI_ASSERT_RANGE_INSERT(_column, mVectorColumnInfo.size(), "MultiList2::insertColumnAt");
		if (_column == ITEM_NONE) _column = mVectorColumnInfo.size();

		// скрываем у крайнего скролл
		if (!mVectorColumnInfo.empty())
			mVectorColumnInfo.back().list->setScrollVisible(false);
		else mSortColumnIndex = 0;

		ColumnInfo column;
		column.width = _width < 0 ? 0 : _width;

		column.list = mClient->createWidget<List>(mSkinList, IntCoord(), Align::Left | Align::VStretch);
		column.list->eventListChangePosition = newDelegate(this, &MultiList2::notifyListChangePosition);
		column.list->eventListMouseItemFocus = newDelegate(this, &MultiList2::notifyListChangeFocus);
		column.list->eventListChangeScroll = newDelegate(this, &MultiList2::notifyListChangeScrollPosition);
		column.list->eventListSelectAccept = newDelegate(this, &MultiList2::notifyListSelectAccept);
		//column.list->setColour(Colour(0.3,0.6,0.8));

		column.button = mClient->createWidget<Button>(mSkinButton, IntCoord(), Align::Default);
		column.button->eventMouseButtonClick = newDelegate(this, &MultiList2::notifyButtonClick);
		column.button->setColour(Colour(0.8,0.9,1.0));
		column.name = _name;
		column.data = _data;

		// если уже были столбики, то делаем то же колличество полей
		if (!mVectorColumnInfo.empty())
		{
			size_t count = mVectorColumnInfo.front().list->getItemCount();
			for (size_t pos=0; pos<count; ++pos)
				column.list->addItem("");
		}

		mVectorColumnInfo.insert(mVectorColumnInfo.begin() + _column, column);

		updateColumns();

		// показываем скролл нового крайнего
		mVectorColumnInfo.back().list->setScrollVisible(true);
	}

	/// //////////////////////////////////////////////////////////////////////////
	void MultiList2::beginToItemAt(size_t id)
	{
		for (VectorColumnInfo::iterator iter=mVectorColumnInfo.begin(); iter!=mVectorColumnInfo.end(); ++iter)
		{
			//(*iter).list->setTextColour(Colour(1,0,1));
			(*iter).list->beginToItemAt(id);  //setScrollPosition(800);
		}
	}

	void MultiList2::setColumnNameAt(size_t _column, const UString& _name)
	{
		MYGUI_ASSERT_RANGE(_column, mVectorColumnInfo.size(), "MultiList2::setColumnNameAt");
		mVectorColumnInfo[_column].name = _name;
		redrawButtons();
	}

	void MultiList2::setColumnWidthAt(size_t _column, int _width)
	{
		MYGUI_ASSERT_RANGE(_column, mVectorColumnInfo.size(), "MultiList2::setColumnWidthAt");
		mVectorColumnInfo[_column].width = _width < 0 ? 0 : _width;
		updateColumns();
	}

	const UString& MultiList2::getColumnNameAt(size_t _column)
	{
		MYGUI_ASSERT_RANGE(_column, mVectorColumnInfo.size(), "MultiList2::getColumnNameAt");
		return mVectorColumnInfo[_column].name;
	}

	int MultiList2::getColumnWidthAt(size_t _column)
	{
		MYGUI_ASSERT_RANGE(_column, mVectorColumnInfo.size(), "MultiList2::getColumnWidthAt");
		return mVectorColumnInfo[_column].width;
	}

	void MultiList2::removeColumnAt(size_t _column)
	{
		MYGUI_ASSERT_RANGE(_column, mVectorColumnInfo.size(), "MultiList2::removeColumnAt");

		ColumnInfo& info = mVectorColumnInfo[_column];

		WidgetManager& manager = WidgetManager::getInstance();
		manager.destroyWidget(info.button);
		manager.destroyWidget(info.list);

		mVectorColumnInfo.erase(mVectorColumnInfo.begin() + _column);

		if (mVectorColumnInfo.empty())
		{
			mSortColumnIndex = ITEM_NONE;
			mItemSelected = ITEM_NONE;
		}
		else
		{
			mSortColumnIndex = 0;
			mSortUp = true;
			sortList();
		}

		updateColumns();
	}

	void MultiList2::removeAllColumns()
	{
		WidgetManager& manager = WidgetManager::getInstance();
		for (VectorColumnInfo::iterator iter=mVectorColumnInfo.begin(); iter!=mVectorColumnInfo.end(); ++iter)
		{
			manager.destroyWidget((*iter).button);
			manager.destroyWidget((*iter).list);
		}
		mVectorColumnInfo.clear();
		mSortColumnIndex = ITEM_NONE;

		updateColumns();

		mItemSelected = ITEM_NONE;
	}

	void MultiList2::sortByColumn(size_t _column, bool _backward)
	{
		mSortColumnIndex = _column;
		if (_backward)
		{
			mSortUp = !mSortUp;
			redrawButtons();
			// если было недосортированно то сортируем
			if (mFrameAdvise) sortList();

			flipList();
		}
		else
		{
			mSortUp = true;
			redrawButtons();
			sortList();
		}
	}

	size_t MultiList2::getItemCount() const
	{
		if (mVectorColumnInfo.empty()) return 0;
		return mVectorColumnInfo.front().list->getItemCount();
	}

	void MultiList2::removeAllItems()
	{
		BiIndexBase::removeAllItems();
		for (VectorColumnInfo::iterator iter=mVectorColumnInfo.begin(); iter!=mVectorColumnInfo.end(); ++iter)
		{
			(*iter).list->removeAllItems();
		}

		mItemSelected = ITEM_NONE;
	}

	/*size_t MultiList2::getItemIndexSelected()
	{
		if (mVectorColumnInfo.empty()) return ITEM_NONE;
		size_t item = mVectorColumnInfo.front().list->getItemIndexSelected();
		return (ITEM_NONE == item) ? ITEM_NONE : BiIndexBase::convertToFace(item);
	}*/

	void MultiList2::updateBackSelected(size_t _index)
	{
		if (_index == ITEM_NONE)
		{
			for (VectorColumnInfo::iterator iter=mVectorColumnInfo.begin(); iter!=mVectorColumnInfo.end(); ++iter)
			{
				(*iter).list->clearIndexSelected();
			}
		}
		else
		{
			//size_t index = BiIndexBase::convertToBack(_index);
			for (VectorColumnInfo::iterator iter=mVectorColumnInfo.begin(); iter!=mVectorColumnInfo.end(); ++iter)
			{
				(*iter).list->setIndexSelected(_index);
			}
		}
	}

	void MultiList2::setIndexSelected(size_t _index)
	{
		if (_index == mItemSelected) return;

		MYGUI_ASSERT_RANGE(0, mVectorColumnInfo.size(), "MultiList2::setIndexSelected");
		MYGUI_ASSERT_RANGE_AND_NONE(_index, mVectorColumnInfo.begin()->list->getItemCount(), "MultiList2::setIndexSelected");

		mItemSelected = _index;
		updateBackSelected(BiIndexBase::convertToBack(mItemSelected));
	}

	void MultiList2::setSubItemNameAt(size_t _column, size_t _index, const UString& _name)
	{
		MYGUI_ASSERT_RANGE(_column, mVectorColumnInfo.size(), "MultiList2::setSubItemAt");
		MYGUI_ASSERT_RANGE(_index, mVectorColumnInfo.begin()->list->getItemCount(), "MultiList2::setSubItemAt");

		size_t index = BiIndexBase::convertToBack(_index);
		mVectorColumnInfo[_column].list->setItemNameAt(index, _name);

		// если мы попортили список с активным сортом, надо пересчитывать
		if (_column == mSortColumnIndex) frameAdvise(true);
	}

	const UString& MultiList2::getSubItemNameAt(size_t _column, size_t _index)
	{
		MYGUI_ASSERT_RANGE(_column, mVectorColumnInfo.size(), "MultiList2::getSubItemNameAt");
		MYGUI_ASSERT_RANGE(_index, mVectorColumnInfo.begin()->list->getItemCount(), "MultiList2::getSubItemNameAt");

		size_t index = BiIndexBase::convertToBack(_index);
		return mVectorColumnInfo[_column].list->getItemNameAt(index);
	}

	size_t MultiList2::findSubItemWith(size_t _column, const UString& _name)
	{
		MYGUI_ASSERT_RANGE(_column, mVectorColumnInfo.size(), "MultiList2::findSubItemWith");

		size_t index = mVectorColumnInfo[_column].list->findItemIndexWith(_name);
		return BiIndexBase::convertToFace(index);
	}
	//----------------------------------------------------------------------------------//

	void MultiList2::updateOnlyEmpty()
	{
		if (nullptr == mButtonMain) return;
		// кнопка, для заполнения пустоты
		if (mWidthBar >= mClient->getWidth()) mButtonMain->setVisible(false);
		else
		{
			mButtonMain->setCoord(mWidthBar, 0, mClient->getWidth()-mWidthBar, mHeightButton);
			mButtonMain->setVisible(true);
		}
	}

	void MultiList2::notifyListChangePosition(List* _sender, size_t _position)
	{
		for (VectorColumnInfo::iterator iter=mVectorColumnInfo.begin(); iter!=mVectorColumnInfo.end(); ++iter)
		{
			if (_sender != (*iter).list) (*iter).list->setIndexSelected(_position);
		}

		updateBackSelected(_position);

		mItemSelected = BiIndexBase::convertToFace(_position);

		// наш евент
		eventListChangePosition(this, mItemSelected);
	}

	void MultiList2::notifyListSelectAccept(List* _sender, size_t _position)
	{
		// наш евент
		eventListSelectAccept(this, BiIndexBase::convertToFace(_position));
	}

	void MultiList2::notifyListChangeFocus(List* _sender, size_t _position)
	{
		for (VectorColumnInfo::iterator iter=mVectorColumnInfo.begin(); iter!=mVectorColumnInfo.end(); ++iter)
		{
			if (_sender != (*iter).list)
			{
				if (ITEM_NONE != mLastMouseFocusIndex) (*iter).list->_setItemFocus(mLastMouseFocusIndex, false);
				if (ITEM_NONE != _position) (*iter).list->_setItemFocus(_position, true);
			}
		}
		mLastMouseFocusIndex = _position;
	}

	void MultiList2::notifyListChangeScrollPosition(List* _sender, size_t _position)
	{
		for (VectorColumnInfo::iterator iter=mVectorColumnInfo.begin(); iter!=mVectorColumnInfo.end(); ++iter)
		{
			if (_sender != (*iter).list)
				(*iter).list->setScrollPosition(_position);
		}
	}

	void MultiList2::notifyButtonClick(MyGUI::Widget* _sender)
	{
		size_t index = *_sender->_getInternalData<size_t>();
		sortByColumn(index, index == mSortColumnIndex);
	}

	void MultiList2::redrawButtons()
	{
		size_t pos = 0;
		for (VectorColumnInfo::iterator iter=mVectorColumnInfo.begin(); iter!=mVectorColumnInfo.end(); ++iter)
		{
			if (pos == mSortColumnIndex)
			{
				if (mSortUp) setButtonImageIndex((*iter).button, SORT_UP);
				else setButtonImageIndex((*iter).button, SORT_DOWN);
			}
			else setButtonImageIndex((*iter).button, SORT_NONE);
			(*iter).button->setCaption((*iter).name);
			pos++;
		}
	}

	void MultiList2::setButtonImageIndex(Button* _button, size_t _index)
	{
		StaticImage* image = _button->getStaticImage();
		if ( nullptr == image ) return;
		if (image->getItemResource())
		{
			static const size_t CountIcons = 3;
			static const char * IconNames[CountIcons + 1] = { "None", "Up", "Down", "" };
			if (_index >= CountIcons) _index = CountIcons;
			image->setItemName(IconNames[_index]);
		}
		else
		{
			image->setItemSelect(_index);
		}
	}

	void MultiList2::frameEntered(float _frame)
	{
		sortList();
	}

	void MultiList2::frameAdvise(bool _advise)
	{
		if ( _advise )
		{
			if ( ! mFrameAdvise )
			{
				MyGUI::Gui::getInstance().eventFrameStart += MyGUI::newDelegate( this, &MultiList2::frameEntered );
				mFrameAdvise = true;
			}
		}
		else
		{
			if ( mFrameAdvise )
			{
				MyGUI::Gui::getInstance().eventFrameStart -= MyGUI::newDelegate( this, &MultiList2::frameEntered );
				mFrameAdvise = false;
			}
		}
	}

	Widget* MultiList2::getSeparator(size_t _index)
	{
		if (!mWidthSeparator || mSkinSeparator.empty()) return nullptr;
		// последний столбик
		if (_index == mVectorColumnInfo.size()-1) return nullptr;

		while (_index >= mSeparators.size())
		{
			Widget* separator = mClient->createWidget<Widget>(mSkinSeparator, IntCoord(), Align::Default);
			mSeparators.push_back(separator);
		}

		return mSeparators[_index];
	}

	void MultiList2::updateColumns()
	{
		mWidthBar = 0;
		size_t index = 0;
		for (VectorColumnInfo::iterator iter=mVectorColumnInfo.begin(); iter!=mVectorColumnInfo.end(); ++iter)
		{
			(*iter).list->setCoord(mWidthBar, mHeightButton, (*iter).width, mClient->getHeight() - mHeightButton);
			(*iter).button->setCoord(mWidthBar, 0, (*iter).width, mHeightButton);
			(*iter).button->_setInternalData(index);

			mWidthBar += (*iter).width;

			// промежуток между листами
			Widget* separator = getSeparator(index);
			if (separator)
			{
				separator->setCoord(mWidthBar, 0, mWidthSeparator, mClient->getHeight());
			}

			mWidthBar += mWidthSeparator;
			index++;
		}

		redrawButtons();
		updateOnlyEmpty();
	}

	void MultiList2::flipList()
	{
		if (ITEM_NONE == mSortColumnIndex) return;

		size_t last = mVectorColumnInfo.front().list->getItemCount();
		if (0 == last) return;
		last --;
		size_t first = 0;

		while (first < last)
		{
			BiIndexBase::swapItemsBackAt(first, last);
			for (VectorColumnInfo::iterator iter=mVectorColumnInfo.begin(); iter!=mVectorColumnInfo.end(); ++iter)
			{
				(*iter).list->swapItemsAt(first, last);
			}

			first++;
			last--;
		}

		updateBackSelected(BiIndexBase::convertToBack(mItemSelected));
	}

	bool MultiList2::compare(List* _list, size_t _left, size_t _right)
	{
		bool result = false;
		if (mSortUp) std::swap(_left, _right);
		if (requestOperatorLess.empty()) result = _list->getItemNameAt(_left) < _list->getItemNameAt(_right);
		else requestOperatorLess(this, mSortColumnIndex, _list->getItemNameAt(_left), _list->getItemNameAt(_right), result);
		return result;
	}

	void MultiList2::sortList()
	{
		if (ITEM_NONE == mSortColumnIndex) return;

		List* list = mVectorColumnInfo[mSortColumnIndex].list;

		size_t count = list->getItemCount();
		if (0 == count) return;

		// shell sort
		int first, last;
		for (size_t step = count>>1; step>0 ; step >>= 1)
		{
			for (size_t i=0;i<(count-step);i++)
			{
				first=i;
				while (first>=0)
				{
					last = first+step;
					if (compare(list, first, last))
					{
						BiIndexBase::swapItemsBackAt(first, last);
						for (VectorColumnInfo::iterator iter=mVectorColumnInfo.begin(); iter!=mVectorColumnInfo.end(); ++iter)
						{
							(*iter).list->swapItemsAt(first, last);
						}
					}
					first--;
				}
			}
		}

		frameAdvise(false);

		updateBackSelected(BiIndexBase::convertToBack(mItemSelected));
	}

	void MultiList2::insertItemAt(size_t _index, const UString& _name, Any _data)
	{
		MYGUI_ASSERT_RANGE(0, mVectorColumnInfo.size(), "MultiList2::insertItemAt");
		MYGUI_ASSERT_RANGE_INSERT(_index, mVectorColumnInfo.front().list->getItemCount(), "MultiList2::insertItemAt");
		if (ITEM_NONE == _index) _index = mVectorColumnInfo.front().list->getItemCount();

		// если надо, то меняем выделенный элемент
		// при сортировке, обновится
		if ((mItemSelected != ITEM_NONE) && (_index <= mItemSelected)) mItemSelected ++;

		size_t index = BiIndexBase::insertItemAt(_index);

		// вставляем во все поля пустые, а потом присваиваем первому
		for (VectorColumnInfo::iterator iter=mVectorColumnInfo.begin(); iter!=mVectorColumnInfo.end(); ++iter)
		{
			(*iter).list->insertItemAt(index, "");
		}
		mVectorColumnInfo.front().list->setItemNameAt(index, _name);
		mVectorColumnInfo.front().list->setItemDataAt(index, _data);

		frameAdvise(true);
	}

	void MultiList2::removeItemAt(size_t _index)
	{
		MYGUI_ASSERT_RANGE(0, mVectorColumnInfo.size(), "MultiList2::removeItemAt");
		MYGUI_ASSERT_RANGE(_index, mVectorColumnInfo.begin()->list->getItemCount(), "MultiList2::removeItemAt");

		size_t index = BiIndexBase::removeItemAt(_index);

		for (VectorColumnInfo::iterator iter=mVectorColumnInfo.begin(); iter!=mVectorColumnInfo.end(); ++iter)
		{
			(*iter).list->removeItemAt(index);
		}

		// если надо, то меняем выделенный элемент
		size_t count = mVectorColumnInfo.begin()->list->getItemCount();
		if (count == 0) mItemSelected = ITEM_NONE;
		else if (mItemSelected != ITEM_NONE)
		{
			if (_index < mItemSelected) mItemSelected --;
			else if ((_index == mItemSelected) && (mItemSelected == count)) mItemSelected --;
		}
		updateBackSelected(BiIndexBase::convertToBack(mItemSelected));
	}

	void MultiList2::swapItemsAt(size_t _index1, size_t _index2)
	{
		MYGUI_ASSERT_RANGE(0, mVectorColumnInfo.size(), "MultiList2::removeItemAt");
		MYGUI_ASSERT_RANGE(_index1, mVectorColumnInfo.begin()->list->getItemCount(), "MultiList2::swapItemsAt");
		MYGUI_ASSERT_RANGE(_index2, mVectorColumnInfo.begin()->list->getItemCount(), "MultiList2::swapItemsAt");

		// при сортированном, меняем только индексы
		BiIndexBase::swapItemsFaceAt(_index1, _index2);

		// при несортированном, нужно наоборот, поменять только данные
		// FIXME
	}

	void MultiList2::setColumnDataAt(size_t _index, Any _data)
	{
		MYGUI_ASSERT_RANGE(_index, mVectorColumnInfo.size(), "MultiList2::setColumnDataAt");
		mVectorColumnInfo[_index].data = _data;
	}

	void MultiList2::setSubItemDataAt(size_t _column, size_t _index, Any _data)
	{
		MYGUI_ASSERT_RANGE(_column, mVectorColumnInfo.size(), "MultiList2::setSubItemDataAt");
		MYGUI_ASSERT_RANGE(_index, mVectorColumnInfo.begin()->list->getItemCount(), "MultiList2::setSubItemDataAt");

		size_t index = BiIndexBase::convertToBack(_index);
		mVectorColumnInfo[_column].list->setItemDataAt(index, _data);
	}

} // namespace MyGUI