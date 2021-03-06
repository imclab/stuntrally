#include "pch.h"
#include "common/Def_Str.h"
#include "common/Gui_Def.h"
#include "../vdrift/pathmanager.h"
#include "../vdrift/settings.h"
#include "CGame.h"
#include "CGui.h"
#include "common/data/CData.h"
#include "common/data/TracksXml.h"
#include "common/GuiCom.h"
#include "common/MultiList2.h"
#include "common/Slider.h"
//#include "../network/masterclient.hpp"
#include "../network/gameclient.hpp"
#include <boost/filesystem.hpp>
#include <MyGUI_Gui.h>
#include <MyGUI_TabControl.h>
#include <MyGUI_TabItem.h>
#include <MyGUI_EditBox.h>
#include <MyGUI_Window.h>
#include <MyGUI_ImageBox.h>
#include <MyGUI_MultiListBox.h>
using namespace std;
using namespace Ogre;
using namespace MyGUI;


///  Car list

//  sort	 . . . . . . . . . . . . . . . . . . . . . . 
//-----------------------------------------------------------------------------------------------------------
/*  common sort code,  no info only by name  */
#define sArg  const CarL& c2, const CarL& c1
#define sortDef  bool t = false;  if (!c1.ci || !c2.ci)  return c1.name > c2.name || t;
const int allSortFunc = 5;

//  sorting functions for columns
/* 0  name    */  bool Sort0 (sArg){  sortDef  return c1.name  < c2.name   || t;  }
/* 1  speed   */  bool Sort1 (sArg){  sortDef  return c1.ci->speed < c2.ci->speed || t;  }
/* 2  year    */  bool Sort2 (sArg){  sortDef  return c1.ci->year < c2.ci->year  || t;  }
/* 3  type    */  bool Sort3 (sArg){  sortDef  return c1.ci->type < c2.ci->type || t;  }
/* 4  rate    */  bool Sort4 (sArg){  sortDef  return c1.ci->rating < c2.ci->rating || t;  }

//  sorting functions array (to access by column index)
bool (*CarSort[allSortFunc])(const CarL& c1, const CarL& c2) = {
	Sort0, Sort1, Sort2, Sort3, Sort4 };


//  done every list sort column change or find edit text change
//  fills gui cars list
//-----------------------------------------------------------------------------------------------------------
void CGui::CarListUpd(bool resetNotFound)
{
	bool filter = isChallGui();
		
	if (carList)
	{	carList->removeAllItems();
		int ii = 0, si = -1;  bool bFound = false;

		//  sort
		int numFunc = min(allSortFunc-1, (int)carList->mSortColumnIndex);
		std::list<CarL> liCar2 = liCar;  // copy
		liCar2.sort(CarSort[numFunc]);
		if (carList->mSortUp)  liCar2.reverse();
		
		//  original
		for (std::list<CarL>::iterator i = liCar2.begin(); i != liCar2.end(); ++i)
		{
			String name = (*i).name;  //, nlow = name;  StringUtil::toLowerCase(nlow);
			//if (sTrkFind == "" || strstr(nlow.c_str(), sTrkFind.c_str()) != 0)

			///  filter for challenge
			if (!filter || IsChallCar(name))
			{
				AddCarL(name, (*i).ci);
				if (name == pSet->gui.car[0])  {  si = ii;
					carList->setIndexSelected(si);
					bFound = true;  }
				ii++;
		}	}

		//  not found last car, set last
		if (resetNotFound && !bFound)
			pSet->gui.car[0] = carList->getItemNameAt(carList->getItemCount()-1).substr(7);

		if (si > -1)  // center
			carList->beginToItemAt(max(0, si-5));
	}
}

void CGui::AddCarL(string name, const CarInfo* ci)
{
	MultiList2* li = carList;
	CarInfo cci;
	if (!ci)  ci = &cci;  //  details
	String clr = data->cars->colormap[ci->type];  if (clr.length() != 7)  clr = "#C0D0E0";
	
	li->addItem(clr+ name);  int l = li->getItemCount()-1, y = ci->year%100;
	li->setSubItemNameAt(1,l, gcom->clrsDiff[std::min(7, (int)(ci->speed*0.9f))]+ toStr(ci->speed));
	li->setSubItemNameAt(2,l, clr+ "\'"+toStr(y/10)+toStr(y%10));
	li->setSubItemNameAt(3,l, clr+ TR("#{CarType_"+ci->type+"}"));
}

void CGui::FillCarList()
{
	liCar.clear();
	strlist li;
	PATHMANAGER::DirList(PATHMANAGER::Cars(), li);
	for (strlist::iterator i = li.begin(); i != li.end(); ++i)
	{
		if (boost::filesystem::exists(PATHMANAGER::Cars() + "/" + *i + "/about.txt"))
		{	String s = *i;
			CarL c;  c.name = *i;  //c.pA = this;
			int id = data->cars->carmap[*i];
			c.ci = id==0 ? 0 : &data->cars->cars[id-1];
			liCar.push_back(c);
	}	}
}
//-----------------------------------------------------------------------------------------------------------


//  ghost filename
//
string ghostFile(SETTINGS* pSet, string sim_mode, string car)
{
	return PATHMANAGER::Ghosts()+"/" +sim_mode+"/"
		+ pSet->game.track + (pSet->game.track_user ? "_u" : "") + (pSet->game.trackreverse ? "_r" : "")
		+ "_" + car + ".rpl";
}

const String& CGui::GetGhostFile(string* ghCar)
{
	static String file;
	string sim_mode = pSet->game.sim_mode, car = pSet->game.car[0];
	file = ghostFile(pSet, sim_mode, car);
	if (PATHMANAGER::FileExists(file))
		return file;
	
	if (!ghCar)
		return file;

	///--  if doesnt exist look for other cars, then other sim modes

	//  cars list sorted by car speed
	std::list<CarL> liCar2 = liCar;
	liCar2.sort(CarSort[1]);

	std::vector<string> cars;
	for (std::list<CarL>::iterator i = liCar2.begin(); i != liCar2.end(); ++i)
	{
		String name = (*i).name;
		cars.push_back(name);
		//LogO(name);
	}

	//  find current
	int i = 0, si = cars.size(), ci = 0;
	while (i < si)
	{	if (cars[i] == car)
		{	ci = i;  break;  }
		++i;
	}
	//LogO(toStr(ci)+" ci "+cars[ci]+" all "+toStr(si));

	std::vector<string> cars2;
	int a = ci, b = ci;  i = 0;
	cars2.push_back(cars[ci]);  // 1st cur
	while (cars2.size() < si)  // same size
	{	// +1, -1, +2, -2 ..
		if (i % 2 == 0)
		{	++a;  // next faster car
			if (a < si)  cars2.push_back(cars[a]);
		}else
		{	--b;  // next slower car
			if (b >= 0)  cars2.push_back(cars[b]);
		}	++i;
	}
	//for (i=0; i < cars2.size(); ++i)
	//	LogO(toStr(i)+"> "+cars2[i]);
	
	bool srch = true;
	i = 0;  a = 0;
	while (srch)
	{
		const string& car = cars2[i];
		file = ghostFile(pSet, sim_mode, car);

		if (PATHMANAGER::FileExists(file))
		{	srch = false;  *ghCar = car;  }
		++i;
		if (i >= si)
		{	i = 0;
			if (sim_mode == "easy")  sim_mode = "normal";
			else  sim_mode = "easy";
			++a;  if (a==2)  srch = false;  // only those 2
		}
	}
	return file;
}

string CGui::GetRplListDir()
{
	return (pSet->rpl_listghosts
		? (PATHMANAGER::Ghosts() + "/" + pSet->gui.sim_mode)
		: PATHMANAGER::Replays() );
}


//  [Game] 	. . . . . . . . . . . . . . . . . . . .    --- lists ----    . . . . . . . . . . . . . . . . . . 

//  car
void CGui::listCarChng(MultiList2* li, size_t pos)
{
	size_t i = li->getIndexSelected();  if (i==ITEM_NONE)  return;
	const UString& sl = li->getItemNameAt(i).substr(7);  sListCar = sl;

	if (imgCar && !pSet->dev_no_prvs)  imgCar->setImageTexture(sListCar+".jpg");
	if (app->mClient)  app->mClient->updatePlayerInfo(pSet->nickname, sListCar);
	
	//  car desc load
	if (carDesc)
	{
		string path = PATHMANAGER::Cars()+"/"+sListCar+"/description.txt";
		ifstream fi(path.c_str());

		string sdesc = "", s;  bool f1 = true;
		while (getline(fi, s))
		{
			if (f1) {  f1 = false;
				if (txCarAuthor)  txCarAuthor->setCaption(s);  }
			else
				sdesc += s + "\n";
		}
		fi.close();

		carDesc->setCaption(sdesc);
	}
	//  car info
	int id = data->cars->carmap[sl];
	if (id > 0 && txCarSpeed && txCarType)
	{	const CarInfo& ci = data->cars->cars[id-1];
		txCarSpeed->setCaption(gcom->clrsDiff[std::min(7, (int)(ci.speed*0.9f))]+ toStr(ci.speed));
		txCarType->setCaption(data->cars->colormap[ci.type]+ TR("#{CarType_"+ci.type+"}"));
	}

	changeCar();
	UpdCarStatsTxt();
}	
void CGui::changeCar()
{
	if (iCurCar < 4)
		pSet->gui.car[iCurCar] = sListCar;
}

///  car stats txt
void CGui::UpdCarStatsTxt()
{
	string path;
	//GetCarPath(&path, 0, 0, sListCar, true);
	//path = path.substr(0, path.length()-4) + "_stats.txt";
	path = PATHMANAGER::CarSim() + "/" + pSet->gui.sim_mode + "/cars/" + sListCar + "_stats.txt";

	string txt(""), vals(""), s;
	ifstream fi(path.c_str());
	if (fi.good())
	{
		int i = 0;
		while (getline(fi, s))
		{
			if (i % 2 == 0)
				txt += s + "\n";
			else
				vals += s + "\n";
			++i;
		}
		fi.close();
	}
	txCarStatsTxt->setCaption(txt);
	txCarStatsVals->setCaption(vals);
}


//  track
void CGui::changeTrack()
{
	pSet->gui.track = gcom->sListTrack;
	pSet->gui.track_user = gcom->bListTrackU;
							//_ only for host..
	if (app->mMasterClient && valNetPassword->getVisible())
	{	uploadGameInfo();
		updateGameInfoGUI();  }
}

//  new game
void CGui::btnNewGame(WP)
{
	if (app->mWndGame->getVisible() && app->mWndTabsGame->getIndexSelected() < TAB_Champs  || app->mClient)
		BackFromChs();  /// champ, back to single race
	
	app->NewGame();  app->isFocGui = false;  // off gui
	if (app->mWndOpts)  app->mWndOpts->setVisible(app->isFocGui);
	if (app->mWndRpl)  app->mWndRpl->setVisible(false);//
	if (gcom->bnQuit)  gcom->bnQuit->setVisible(app->isFocGui);
	
	app->updMouse();
	
	gcom->mToolTip->setVisible(false);
}
void CGui::btnNewGameStart(WP wp)
{
	changeTrack();
	btnNewGame(wp);
}


//  Menu
//-----------------------------------------------------------------------------------------------------------

void CGui::toggleGui(bool toggle)
{
	if (toggle)
		app->isFocGui = !app->isFocGui;

	bool notMain = app->isFocGui && !pSet->isMain;
	if (app->mWndMain)	app->mWndMain->setVisible(app->isFocGui && pSet->isMain);
	if (app->mWndReplays) app->mWndReplays->setVisible(notMain && pSet->inMenu == MNU_Replays);
	if (app->mWndHelp)	app->mWndHelp->setVisible(notMain && pSet->inMenu == MNU_Help);
	if (app->mWndOpts)	app->mWndOpts->setVisible(notMain && pSet->inMenu == MNU_Options);
	
	//  load Readme editbox from file
	if (app->mWndHelp && app->mWndHelp->getVisible() && loadReadme)
	{
		loadReadme = false;
		Ed ed = fEd("Readme");
		if (ed)
		{	string path = PATHMANAGER::Data()+"/../Readme.txt";
			std::ifstream fi(path.c_str());
			if (fi.good())
			{	String text = "", s;
				while (getline(fi,s))
					text += s + "\n";

				text = StringUtil::replaceAll(text, "#", "##");
				ed->setCaption(UString(text));
				ed->setVScrollPosition(0);
	}	}	}

	///  update track tab, for champs wnd
	bool game = pSet->inMenu == MNU_Single, champ = pSet->inMenu == MNU_Champ,
		tutor = pSet->inMenu == MNU_Tutorial, chall = pSet->inMenu == MNU_Challenge,
		chAny = champ || tutor || chall, gc = game || chAny;
	UString sCh = chall ? TR("#90FFD0#{Challenge}") : (tutor ? TR("#FFC020#{Tutorial}") : TR("#80C0FF#{Championship}"));

	UpdChampTabVis();
	
	if (app->mWndGame)
	{	bool vis = notMain  && gc;
		app->mWndGame->setVisible(vis);
		if (vis)
		{
			app->mWndGame->setCaption(chAny ? sCh : TR("#{SingleRace}"));
			TabItem* t = app->mWndTabsGame->getItemAt(TAB_Champs);
			t->setCaption(sCh);
		}
	}
	if (notMain && gc)  // show hide champs,stages
	{
		Tab t = app->mWndTabsGame;
		size_t id = t->getIndexSelected();
		t->setButtonWidthAt(TAB_Track, chAny ? 1 :-1);  if (id == TAB_Track && chAny)  t->setIndexSelected(TAB_Champs);
		t->setButtonWidthAt(TAB_Multi, chAny ? 1 :-1);  if (id == TAB_Multi && chAny)  t->setIndexSelected(TAB_Champs);
		t->setButtonWidthAt(TAB_Champs,chAny ?-1 : 1);  if (id == TAB_Champs && !chAny)  t->setIndexSelected(TAB_Track);
		t->setButtonWidthAt(TAB_Stages,chAny ?-1 : 1);  if (id == TAB_Stages && !chAny)  t->setIndexSelected(TAB_Track);
		t->setButtonWidthAt(TAB_Stage, chAny ?-1 : 1);  if (id == TAB_Stage  && !chAny)  t->setIndexSelected(TAB_Track);
	}

	if (gcom->bnQuit)  gcom->bnQuit->setVisible(app->isFocGui);
	app->updMouse();
	if (!app->isFocGui)  gcom->mToolTip->setVisible(false);

	for (int i=0; i < ciMainBtns; ++i)
		app->mWndMainPanels[i]->setVisible(pSet->inMenu == i);
		
	//  1st center mouse
	static bool first = true;
	if (app->isFocGui && first)
	{	first = false;
		gcom->GuiCenterMouse();
	}
}


//  Gui Shortcut  alt-letters
void CGui::GuiShortcut(MNU_Btns mnu, int tab, int subtab)
{
	if (subtab == -1 && (!app->isFocGui || pSet->inMenu != mnu))  subtab = -2;  // cancel subtab cycling

	app->isFocGui = true;
	pSet->isMain = false;  pSet->inMenu = mnu;
	
	TabPtr mWndTabs = 0;
	std::vector<TabControl*>* subt = 0;
	
	switch (mnu)
	{	case MNU_Replays:	mWndTabs = app->mWndTabsRpl;  break;
		case MNU_Help:		mWndTabs = app->mWndTabsHelp;  break;
		case MNU_Options:	mWndTabs = app->mWndTabsOpts;  subt = &vSubTabsOpts;  break;
		default:			mWndTabs = app->mWndTabsGame;  subt = &vSubTabsGame;  break;
	}
	toggleGui(false);


	size_t t = mWndTabs->getIndexSelected();
	mWndTabs->setIndexSelected(tab);

	if (!subt)  return;
	TabControl* tc = (*subt)[tab];  if (!tc)  return;
	int  cnt = tc->getItemCount();

	if (t == tab && subtab == -1)  // cycle subpages if same tab
	{	if (app->shift)
			tc->setIndexSelected( (tc->getIndexSelected()-1+cnt) % cnt );
		else
			tc->setIndexSelected( (tc->getIndexSelected()+1) % cnt );
	}
	if (subtab > -1)
		tc->setIndexSelected( std::min(cnt-1, subtab) );
	
	if (!tc->eventTabChangeSelect.empty())
		tc->eventTabChangeSelect(tc, tc->getIndexSelected());
}

//  close netw end
void CGui::btnNetEndClose(WP)
{
	app->mWndNetEnd->setVisible(false);
	app->isFocGui = true;  // show back gui
	toggleGui(false);
}


//  utility
//---------------------------------------------------------------------------------------------------------------------

//  next/prev in list by key
int CGui::LNext(Mli2 lp, int rel, int ofs)
{
	size_t cnt = lp->getItemCount();
	if (cnt==0)  return 0;
	int i = std::max(0, std::min((int)cnt-1, (int)lp->getIndexSelected()+rel ));
	lp->setIndexSelected(i);
	lp->beginToItemAt(std::max(0, i-ofs));  // center
	return i;
}
int CGui::LNext(Mli lp, int rel)
{
	size_t cnt = lp->getItemCount();
	if (cnt==0)  return 0;
	int i = std::max(0, std::min((int)cnt-1, (int)lp->getIndexSelected()+rel ));
	lp->setIndexSelected(i);
	return i;
}
int CGui::LNext(Li lp, int rel, int ofs)
{
	size_t cnt = lp->getItemCount();
	if (cnt==0)  return 0;
	int i = std::max(0, std::min((int)cnt-1, (int)lp->getIndexSelected()+rel ));
	lp->setIndexSelected(i);
	lp->beginToItemAt(std::max(0, i-ofs));  // center
	return i;
}

void CGui::LNext(int rel)
{
	//if (!ap->isFocGui || pSet->isMain)  return;
	if (pSet->inMenu == MNU_Replays)
		listRplChng(rplList,  LNext(rplList, rel, 11));
	else
		if (app->mWndGame->getVisible())
		switch (app->mWndTabsGame->getIndexSelected())
		{	case TAB_Track:  gcom->listTrackChng(gcom->trkList,  LNext(gcom->trkList, rel, 11));  return;
			case TAB_Car:	 listCarChng(carList,    LNext(carList, rel, 5));  return;
			case TAB_Game:	 if (rel > 0)  radSimNorm(0);  else  radSimEasy(0);  return;
			case TAB_Champs:
				if (isChallGui())
				      listChallChng(liChalls, LNext(liChalls, rel, 8));
				else  listChampChng(liChamps, LNext(liChamps, rel, 8));
				return;
			case TAB_Stages: listStageChng(liStages, LNext(liStages, rel, 8));  return;
			case TAB_Stage:	 if (rel > 0)  btnStageNext(0);  else  btnStagePrev(0);  return;
		}
}


///  Update (frame start)  .,.,.,.,..,.,.,.,..,.,.,.,..,.,.,.,.
void CGui::GuiUpdate()
{
	gcom->UnfocusLists();

	if (gcom->bGuiReinit)  // after language change from combo
	{	gcom->bGuiReinit = false;

		mGui->destroyWidgets(app->vwGui);
		gcom->bnQuit=0; app->mWndOpts=0;  //todo: rest too..  delete, new gui; ?

		bGI = false;
		InitGui();

		app->bWindowResized = true;
		app->mWndTabsOpts->setIndexSelected(3);  // switch back to view tab
	}

		
	//  sort trk list
	gcom->SortTrkList();

	//  sort car list
	if (gcom->needSort(carList))
	{
		pSet->cars_sort = carList->mSortColumnIndex;
		pSet->cars_sortup = carList->mSortUp;
		CarListUpd(false);
	}

}
